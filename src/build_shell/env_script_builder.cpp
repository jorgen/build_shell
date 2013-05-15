/*
 * Copyright © 2013 Jørgen Lind

 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.

 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
*/
#include "env_script_builder.h"

#include "json_tree.h"
#include "tree_builder.h"
#include "tree_writer.h"

#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

EnvScriptBuilder::EnvScriptBuilder(const Configuration &configuration)
    : m_configuration(configuration)
    , m_out_file(configuration.buildDir() + "/build_shell/build_environment.json")
{
    if (access(m_out_file.c_str(), R_OK) == 0) {
        TreeBuilder tree_builder(m_out_file);
        m_environment_node = tree_builder.takeRootNode();
    } else {
        m_environment_node = new JT::ObjectNode();
    }
}

EnvScriptBuilder::~EnvScriptBuilder()
{
    mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH;
    int out_file = open(m_out_file.c_str(), O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC, mode);

    if (out_file >= 0) {
        TreeWriter writer(out_file,m_environment_node,true);
        if (writer.error()) {
        }
    } else {
        fprintf(stderr, "Could not open %s : %s\n", m_out_file.c_str(), strerror(errno));

    }

    delete m_environment_node;
}
static void addOrRemoveIfEmpty(JT::ObjectNode *node, const std::string &property, const std::string &value)
{
    if (value.size()) {
        node->insertNode(property, new JT::StringNode(value), true);
    } else {
        delete node->take(property);
    }
}
void EnvScriptBuilder::addProjectNode(const std::string &project_name, JT::ObjectNode *project_root)
{
    if (!project_root)
        return;

    JT::ObjectNode *project_environment = 0;
    for (auto it = m_environment_node->begin(); it != m_environment_node->end(); ++it) {
        if (it->first.compareString(project_name)) {
            project_environment = it->second->asObjectNode();
        }
    }
    if (!project_environment) {
        project_environment = new JT::ObjectNode();
        m_environment_node->insertNode(project_name, project_environment);
    }

    addOrRemoveIfEmpty(project_environment, "project_src_path", project_root->stringAt("arguments.src_path"));
    addOrRemoveIfEmpty(project_environment, "project_build_path", project_root->stringAt("arguments.build_path"));
    for (auto it = project_root->begin(); it != project_root->end(); ++it) {
        if (it->first.string() != "env" && it->first.string() != "buildsystem_env")
            continue;
        JT::ObjectNode *env_object = it->second->asObjectNode();
        if (!env_object)
            continue;

        env_object = env_object->copy();
        project_environment->insertNode(it->first.string(), env_object, true);

        for (auto env_it = env_object->begin(); env_it != env_object->end(); ++env_it) {
            JT::StringNode *value = env_it->second->asStringNode();
            if (!value) {
                JT::ObjectNode *value_object = env_it->second->asObjectNode();
                if (value_object) {
                    value = value_object->stringNodeAt("value");
                }
                if (!value) {
                    continue;
                }
            }
        }
    }
}

void EnvScriptBuilder::writeSetScript(TempFile &tempFile, const std::string &toProject)
{
    if (tempFile.closed()) {
        fprintf(stderr, "Writing set script to closed temp file\n");
        return;
    }
    auto variables = make_variable_map_up_until(toProject);
    std::string build_set_name = tempFile.name();
    build_set_name = dirname(&build_set_name[0]);
    writeSetScript(build_set_name, "", variables, tempFile.fileStream("w+"),false);
}

void EnvScriptBuilder::writeScripts(const std::string &setFileName, const std::string &unsetFileName, const std::string &toProject)
{
    auto variables = make_variable_map_up_until(toProject);
    if (setFileName.size()) {
        FILE *out_file = fopen(setFileName.c_str(), "w+");
        if (!out_file) {
            fprintf(stderr, "Failed to open out file to write environment script to %s : %s\n",
                    setFileName.c_str(), strerror(errno));
            return;
        }
        std::string build_set_name = setFileName;
        build_set_name = dirname(&build_set_name[0]);
        writeSetScript(build_set_name, unsetFileName, variables, out_file,true);
    }

    if (unsetFileName.size()) {
        FILE *out_file = fopen(unsetFileName.c_str(), "w+");
        if (!out_file) {
            fprintf(stderr, "Failed to open out file to write environment unsetting script to %s : %s\n",
                    unsetFileName.c_str(), strerror(errno));
            return;
        }
        writeUnsetScript(out_file, true, variables);
    }
}

bool EnvScriptBuilder::substitue_variable_value(const std::map<std::string, std::string> jsonVariableMap, std::string &value_string) const
{
    size_t start_pos = value_string.find("{$") + 2;
    if (start_pos < value_string.size()) {
        size_t end_pos = value_string.find("}", start_pos);
        if (end_pos < value_string.size()) {
            const std::string variable = value_string.substr(start_pos, end_pos - start_pos);

            if (jsonVariableMap.count(variable) && jsonVariableMap.at(variable).size()) {
                value_string.replace(start_pos -2, end_pos - start_pos + 3, jsonVariableMap.at(variable));
                return true;
            } else {
                fprintf(stderr, "Failed to find variable substitute for %s\n", value_string.c_str());
                return false;
            }
        }
    }
    return true;
}

static bool containsValue(const std::string &value, const std::list<EnvVariable> &list) {
    for (auto it = list.begin(); it != list.end(); it++) {
        if (it->value == value)
            return true;
    }
    return false;
}

void EnvScriptBuilder::populateMapFromEnvironmentNode(JT::ObjectNode *environmentNode, const std::map<std::string, std::string> &jsonVariableMap, std::map<std::string, std::list<EnvVariable>> &map) const
{
    for (auto it = environmentNode->begin(); it != environmentNode->end(); ++it) {
        EnvVariable variable;
        bool found = false;
        if (JT::ObjectNode *complex_variable = it->second->asObjectNode()) {
            variable.value = complex_variable->stringAt("value");
            variable.overwrite = complex_variable->booleanAt("overwrite");
            found = true;
        } else if (JT::StringNode *simple_variable = it->second->asStringNode()) {
            variable.value = simple_variable->string();
            variable.overwrite = false;
            found = true;
        }
        if (found) {
            if (substitue_variable_value(jsonVariableMap, variable.value)) {
                if (variable.overwrite) {
                    map[it->first.string()].clear();
                }

                const std::string &variable_name = it->first.string();
                std::list<EnvVariable> &variable_list = map[variable_name];
                if (!containsValue(variable.value, variable_list))
                    variable_list.push_back(variable);
            }
        }
    }
}

void EnvScriptBuilder::populateMapFromProjectNode(JT::ObjectNode *projectNode, std::map<std::string, std::list<EnvVariable>> &map) const
{
    std::map<std::string, std::string> json_variable_map;

    const std::string &project_src_path = projectNode->stringAt("project_src_path");
    const std::string &project_build_path = projectNode->stringAt("project_build_path");

    json_variable_map["project_src_path"] = project_src_path;
    json_variable_map["project_build_path"] = project_build_path;
    json_variable_map["install_path"] = m_configuration.installDir();
    json_variable_map["build_path"] = m_configuration.buildDir();
    json_variable_map["src_dir"] = m_configuration.srcDir();

    for (auto it = projectNode->begin(); it != projectNode->end(); ++it) {
        if (it->first.string() != "env" && it->first.string() != "buildsystem_env")
            continue;
        JT::ObjectNode *env_node = it->second->asObjectNode();
        if (!env_node)
            continue;
        populateMapFromEnvironmentNode(env_node, json_variable_map, map);
    }
}

std::map<std::string, std::list<EnvVariable>> EnvScriptBuilder::make_variable_map_up_until(const std::string &project_name) const
{
    std::map<std::string, std::list<EnvVariable>> return_map;
    for (auto it = m_environment_node->begin(); it != m_environment_node->end(); ++it) {
        if (project_name.size() && it->first.compareString(project_name))
                break;
        if (!it->second->asObjectNode())
            continue;
        JT::ObjectNode *project_node = it->second->asObjectNode();
        populateMapFromProjectNode(project_node, return_map);
    }
    return return_map;
}

static void writeEnvironmentVariable(FILE *file, const std::string &name, const std::string &value, bool overwrite)
{
    fprintf(file, "if [ -n \"$%s\" ]; then\n", name.c_str());
    fprintf(file, "    export BUILD_SHELL_OLD_%s=$%s\n", name.c_str(), name.c_str());
    if (overwrite) {
        fprintf(file, "    export %s=\"%s\"\n", name.c_str(), value.c_str());
    } else {
        fprintf(file, "    export %s=\"%s:$%s\"\n", name.c_str(), value.c_str(), name.c_str());
    }
    fprintf(file, "else\n");
    fprintf(file, "    export %s=\"%s\"\n", name.c_str(), value.c_str());
    fprintf(file, "fi\n");
    fprintf(file, "\n");
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
    fprintf(file, "            [ \"$i\" == \"%s\" ] && %s_VARIABLE_FOUND=\"yes\"\n", dir.c_str(), variable.c_str());
    fprintf(file, "        done\n");
    fprintf(file, "        unset %s_VARIABLE_SPLIT\n", variable.c_str());
    fprintf(file, "        if [ ! -n \"$%s_VARIABLE_FOUND\" ]; then\n", variable.c_str());
    fprintf(file, "            if [ ! -n \"$BUILD_SHELL_OLD_%s\" ]; then\n", variable.c_str());
    fprintf(file, "                export BUILD_SHELL_OLD_%s=$%s\n", variable.c_str(), variable.c_str());
    fprintf(file, "            fi\n");
    fprintf(file, "            export %s=\"%s:$%s\"\n", variable.c_str(), dir.c_str(), variable.c_str());
    fprintf(file, "            unset %s_VARIABLE_FOUND\n", variable.c_str());
    fprintf(file, "        fi\n");
    fprintf(file, "    else\n");
    fprintf(file, "        export %s=\"%s\"\n", variable.c_str(), dir.c_str());
    fprintf(file, "        export BUILD_SHELL_DEFINED_%s=\"true\"\n", variable.c_str());
    fprintf(file, "    fi\n");
    fprintf(file, "fi\n");
    fprintf(file, "\n");
}

void EnvScriptBuilder::writeSetScript(const std::string &buildSetName, const std::string &unsetFileName, const std::map<std::string, std::list<EnvVariable>> &variables, FILE *file, bool close)
{
    fprintf(file, "#!/bin/bash\n");
    fprintf(file, "\n");
    fprintf(file, "if [ -n \"$BUILD_SHELL_BUILD_DIR\" ] && [ -e \"$BUILD_SHELL_BUILD_DIR/set_build_env.sh\" ]; then\n");
    fprintf(file, "    source \"$BUILD_SHELL_BUILD_DIR/set_build_env.sh\"\n");
    fprintf(file, "fi\n");
    fprintf(file, "\n");
    fprintf(file, "export BUILD_SHELL_NAME=\"%s\"\n", buildSetName.c_str());
    fprintf(file, "export BUILD_SHELL_SRC_DIR=\"%s\"\n", m_configuration.srcDir().c_str());
    fprintf(file, "export BUILD_SHELL_BUILD_DIR=\"%s\"\n", m_configuration.buildDir().c_str());
    fprintf(file, "export BUILD_SHELL_INSTALL_DIR=\"%s\"\n", m_configuration.installDir().c_str());
    if (unsetFileName.size())
        fprintf(file, "export BUILD_SHELL_UNSET_ENV_FILE=\"%s\"\n", unsetFileName.c_str());
    fprintf(file, "\n");

    for (auto it = variables.begin(); it != variables.end(); ++it) {
        if (!it->second.size())
            continue;
        const std::string value = make_env_variable(it->second);
        writeEnvironmentVariable(file, it->first, value, it->second.front().overwrite);
    }

    if (m_configuration.installDir() != "/usr" && m_configuration.installDir() != "/usr/local") {
        std::string install_lib = m_configuration.installDir() + "/lib";
        std::string install_bin = m_configuration.installDir() + "/bin";
        std::string install_pkg_config = install_lib + "/pkgconfig";
#ifdef __APPLE__
        writeAddDirToVariable(file, install_lib, "DYLD_LIBRARY_PATH");
#else
        writeAddDirToVariable(file, install_lib, "LD_LIBRARY_PATH");
#endif
        writeAddDirToVariable(file, install_bin, "PATH");
        writeAddDirToVariable(file, install_pkg_config, "PKG_CONFIG_PATH");
    }

    fprintf(file, "\n");

    if (close)
        fclose(file);
}

void EnvScriptBuilder::writeUnsetScript(const std::string &file, const std::map<std::string, std::list<EnvVariable>> &variables)
{
    FILE *out_file = fopen(file.c_str(), "w+");
    writeUnsetScript(out_file, true, variables);
}

void write_unset_for_variable(FILE *file, const std::string &variable)
{
    fprintf(file, "if [ -n \"$BUILD_SHELL_OLD_%s\" ]; then\n", variable.c_str());
    fprintf(file, "    export %s=\"$BUILD_SHELL_OLD_%s\"\n", variable.c_str(), variable.c_str());
    fprintf(file, "    unset BUILD_SHELL_OLD_%s\n", variable.c_str());
    fprintf(file, "else\n");
    fprintf(file, "    unset %s\n", variable.c_str());
    fprintf(file, "fi\n");
    fprintf(file, "\n");
}

void write_strict_unset_for_variable(FILE *file, const std::string &variable)
{
    fprintf(file, "if [ -n \"$BUILD_SHELL_DEFINED_%s\" ]; then\n", variable.c_str());
    fprintf(file, "    unset %s\n", variable.c_str());
    fprintf(file, "    unset BUILD_SHELL_DEFINED_%s\n", variable.c_str());
    fprintf(file, "elif [ -n \"$BUILD_SHELL_OLD_%s\" ]; then\n", variable.c_str());
    fprintf(file, "    export %s=\"BUILD_SHELL_OLD_%s\"\n", variable.c_str(), variable.c_str());
    fprintf(file, "    unset BUILD_SHELL_OLD_%s\n", variable.c_str());
    fprintf(file, "fi\n");
}

void EnvScriptBuilder::writeUnsetScript(FILE *file, bool close_file, const std::map<std::string, std::list<EnvVariable>> &variables)
{
    fprintf(file, "#!/bin/bash\n");
    fprintf(file, "\n");
    fprintf(file, "unset BUILD_SHELL_NAME\n");
    fprintf(file, "unset BUILD_SHELL_UNSET_ENV_FILE\n");
    fprintf(file, "\n");

    for (auto it = variables.begin(); it != variables.end(); ++it) {
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

