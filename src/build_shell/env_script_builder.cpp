#include "env_script_builder.h"

#include "json_tree.h"
#include "tree_builder.h"
#include "tree_writer.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

EnvScriptBuilder::EnvScriptBuilder(const Configuration &configuration)
    : m_configuration(configuration)
    , m_out_file(configuration.buildShellConfigPath() + "/build_environment.json")
{
    if (access(m_out_file.c_str(), R_OK) == 0) {
        TreeBuilder tree_builder(m_out_file);
        m_node = tree_builder.takeRootNode();
    } else {
        m_node = new JT::ObjectNode();
    }
}

EnvScriptBuilder::~EnvScriptBuilder()
{
    mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH;
    int out_file = open(m_out_file.c_str(), O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC, mode);

    if (out_file >= 0) {
        TreeWriter writer(out_file,m_node,true);
    }

    delete m_node;
}

void EnvScriptBuilder::substitue_variable_value(std::string &value_string, JT::ObjectNode *project_root) const
{
    size_t start_pos = value_string.find("{$") + 2;
    if (start_pos < value_string.size()) {
        size_t end_pos = value_string.find("}", start_pos);
        if (end_pos < value_string.size()) {
            const std::string variable = value_string.substr(start_pos, end_pos - start_pos);

            std::map<std::string, std::string> variables;
            const std::string &project_src_path = project_root->stringAt("arguments.src_path");
            variables["src_path"] = project_src_path;
            const std::string &project_build_path = project_root->stringAt("arguments.build_path");
            variables["build_path"] = project_build_path;
            variables["install_path"] = m_configuration.installDir();

            if (variables.count(variable)) {
                value_string.replace(start_pos -2, end_pos - start_pos + 3, variables.at(variable));
            }
        }
    }
}
void EnvScriptBuilder::addProjectNode(JT::ObjectNode *project_root)
{
    if (!project_root)
        return;

    for (auto it = project_root->begin(); it != project_root->end(); ++it) {
        if (it->first.string() != "env" && it->first.string() != "buildsystem_env")
            continue;
        JT::ObjectNode *env_object = it->second->asObjectNode();
        if (!env_object)
            continue;

        for (auto env_it = env_object->begin(); env_it != env_object->end(); ++env_it) {
            bool overwrite = false;
            JT::StringNode *value = env_it->second->asStringNode();
            std::string value_string;
            if (!value) {
                JT::ObjectNode *value_object = env_it->second->asObjectNode();
                if (value_object) {
                    value = value_object->stringNodeAt("value");
                    overwrite = value_object->booleanAt("overwrite");
                }
                if (!value)
                    continue;

            }

            value_string = value->string();
            fprintf(stderr, "value OF %s : %s\n", env_it->first.string().c_str(), value_string.c_str());
            substitue_variable_value(value_string, project_root);
            fprintf(stderr, "value after OF %s : %s\n", env_it->first.string().c_str(), value_string.c_str());

            if (overwrite)
                m_root_env[env_it->first.string()].clear();

            m_root_env[env_it->first.string()].push_back(EnvVariable(false,value_string));
        }
    }
}

void EnvScriptBuilder::writeSetScript(const std::string &file)
{
    FILE *out_file = fopen(file.c_str(), "w+");
    if (!out_file) {
        fprintf(stderr, "Failed to print out env script: %s", strerror(errno));
        return;
    }
    writeSetScript(file,out_file,true);

}

static void writeEnvironmentVariable(FILE *file, const std::string &name, const std::string &value, bool overwrite)
{
    std::string escaped_value = value;
    if (value.find(' ') != std::string::npos && value[0] != '"') {
        escaped_value = std::string("\"") + value + "\"";
    }
    fprintf(file, "if [ -n \"$%s\" ]; then\n", name.c_str());
    fprintf(file, "    export BUILD_SHELL_OLD_%s=$%s\n", name.c_str(), name.c_str());
    fprintf(file, "fi\n");
    if (overwrite) {
        fprintf(file, "export %s=%s\n", name.c_str(), escaped_value.c_str());
    } else {
        fprintf(file, "if [ -n \"$%s\" ]; then\n", name.c_str());
        fprintf(file, "    export %s=%s:$%s\n", name.c_str(), escaped_value.c_str(), name.c_str());
        fprintf(file, "else\n");
        fprintf(file, "    export %s=%s\n", name.c_str(), escaped_value.c_str());
        fprintf(file, "fi\n");
    }
}

static std::string make_env_variable(const std::list<EnvVariable> &variables)
{
    std::string ret;
    const std::string &previous = ret;
    for (auto it = variables.begin(); it != variables.end(); ++it) {
        if (it->value == previous)
            continue;
        if (it != variables.begin())
            ret += ":";
        ret += it->value;
    }
    return ret;
}

static void writeAddDirToVariable(FILE *file, const std::string &dir, const std::string &variable)
{
    fprintf(file, "if [ -d \"%s\" ]; then\n", dir.c_str());
    fprintf(file, "    if [ -n \"$%s\" ]; then\n", variable.c_str());
    fprintf(file, "        %s_VARIABLE_FOUND=""\n",variable.c_str());
    fprintf(file, "        IFS=':' read -ra %s_VARIABLE_SPLIT<<< \"$%s\"\n", variable.c_str(), variable.c_str());
    fprintf(file, "        for i in \"${%s_VARIABLE_SPLIT[@]}\"; do\n", variable.c_str());
    fprintf(file, "            [ \"$i\" == %s ] && %s_VARIABLE_FOUND=\"yes\"\n", dir.c_str(), variable.c_str());
    fprintf(file, "        done\n");
    fprintf(file, "        unset %s_VARIABLE_SPLIT\n", variable.c_str());
    fprintf(file, "        if [ ! -n \"$%s_VARIABLE_FOUND\" ]; then\n", variable.c_str());
    fprintf(file, "            if [ ! -n \"$BUILD_SHELL_OLD_%s\" ]; then\n", variable.c_str());
    fprintf(file, "                export BUILD_SHELL_OLD_%s=$%s\n", variable.c_str(), variable.c_str());
    fprintf(file, "            fi\n");
    fprintf(file, "            export %s=%s:$%s\n", variable.c_str(), dir.c_str(), variable.c_str());
    fprintf(file, "            unset %s_VARIABLE_FOUND\n", variable.c_str());
    fprintf(file, "        fi\n");
    fprintf(file, "    else\n");
    fprintf(file, "        export %s=%s\n", variable.c_str(), dir.c_str());
    fprintf(file, "        export BUILD_SHELL_DEFINED_%s=\"true\"\n", variable.c_str());
    fprintf(file, "    fi\n");
    fprintf(file, "fi\n");
}

void EnvScriptBuilder::writeSetScript(const std::string &file_name, FILE *file, bool close)
{

    fprintf(file, "#!/bin/bash\n");
    fprintf(file, "if [ -n \"$BUILD_SHELL_UNSET_ENV_FILE\" ]; then\n");
    fprintf(file, "    source $BUILD_SHELL_UNSET_ENV_FILE\n");
    fprintf(file, "fi\n");
    std::string unset_file_name = std::string("un") + file_name;
    fprintf(file, "export BUILD_SHELL_UNSET_ENV_FILE=%s\n", unset_file_name.c_str());

    for (auto it = m_root_env.begin(); it != m_root_env.end(); ++it) {
        if (!it->second.size())
            continue;
        it->second.sort();
        const std::string value = make_env_variable(it->second);
        writeEnvironmentVariable(file, it->first, value, it->second.front().overwrite);
    }

    if (m_paths["install_path"] != "/usr" || m_paths["install_path"] != "/usr/local") {
        std::string install_lib = m_paths["install_path"] + "/lib";
        std::string install_bin = m_paths["install_path"] + "/bin";
        std::string install_pkg_config = install_lib + "/pkgconfig";
        writeAddDirToVariable(file, install_lib, "LD_LIBRARY_PATH");
        writeAddDirToVariable(file, install_bin, "PATH");
        writeAddDirToVariable(file, install_pkg_config, "PKG_CONFIG_PATH");
    }

    fprintf(file, "\n");

    if (close)
        fclose(file);
}

void EnvScriptBuilder::writeUnsetScript(const std::string &file)
{
    FILE *out_file = fopen(file.c_str(), "w+");
    writeUnsetScript(out_file, true);
}

void write_unset_for_variable(FILE *file, const std::string &variable)
{
    fprintf(file, "if [ -n \"$BUILD_SHELL_OLD_%s\" ]; then\n", variable.c_str());
    fprintf(file, "    export %s=BUILD_SHELL_OLD_%s\n", variable.c_str(), variable.c_str());
    fprintf(file, "    unset BUILD_SHELL_OLD_%s\n", variable.c_str());
    fprintf(file, "else\n");
    fprintf(file, "    unset %s\n", variable.c_str());
    fprintf(file, "fi\n");
}

void write_strict_unset_for_variable(FILE *file, const std::string &variable)
{
    fprintf(file, "if [ -n \"$BUILD_SHELL_DEFINED_%s\" ]; then\n", variable.c_str());
    fprintf(file, "    unset %s\n", variable.c_str());
    fprintf(file, "    unset BUILD_SHELL_DEFINED_%s\n", variable.c_str());
    fprintf(file, "elif [ -n \"$BUILD_SHELL_OLD_%s\" ]; then\n", variable.c_str());
    fprintf(file, "    export %s=BUILD_SHELL_OLD_%s\n", variable.c_str(), variable.c_str());
    fprintf(file, "    unset BUILD_SHELL_OLD_%s\n", variable.c_str());
    fprintf(file, "fi\n");
}

void EnvScriptBuilder::writeUnsetScript(FILE *file, bool close_file)
{
    for (auto it = m_root_env.begin(); it != m_root_env.end(); ++it) {
        if (!it->second.size())
            continue;
        write_unset_for_variable(file, it->first);
    }

    write_strict_unset_for_variable(file, "LD_LIBRARY_PATH");
    write_strict_unset_for_variable(file, "PATH");
    write_strict_unset_for_variable(file, "PKG_CONFIG_PATH");

    fprintf(file, "\n");

    if (close_file)
        fclose(file);
}

