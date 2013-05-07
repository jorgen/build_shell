#include "env_script_builder.h"

#include "json_tree.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

EnvScriptBuilder::EnvScriptBuilder()
{
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
            if (!value) {
                JT::ObjectNode *value_object = env_it->second->asObjectNode();
                value = value_object->stringNodeAt("value");
                overwrite = value_object->booleanAt("overwrite");
            }
            if (!value)
                continue;
            if (overwrite)
                m_root_env[env_it->first.string()].clear();

            m_root_env[env_it->first.string()].push_back(EnvVariable(false,value->string()));
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

void EnvScriptBuilder::writeUnsetScript(FILE *file, bool close_file)
{
    for (auto it = m_root_env.begin(); it != m_root_env.end(); ++it) {
        if (!it->second.size())
            continue;
        write_unset_for_variable(file, it->first);
    }

    fprintf(file, "\n");
    if (close_file)
        fclose(file);
}
