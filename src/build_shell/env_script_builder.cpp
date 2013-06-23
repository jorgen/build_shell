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

EnvScriptBuilder::EnvScriptBuilder(const Configuration &configuration, const BuildEnvironment &buildEnvironment, JT::ObjectNode *buildset_node)
    : m_configuration(configuration)
    , m_build_environment(buildEnvironment)
    , m_buildset_node(buildset_node)
{
}

EnvScriptBuilder::~EnvScriptBuilder()
{
}

void EnvScriptBuilder::setToProject(const std::string &toProject)
{
    m_to_project = toProject;
}

void EnvScriptBuilder::writeSetScript(TempFile &tempFile)
{
    if (tempFile.closed()) {
        fprintf(stderr, "Writing set script to closed temp file\n");
        return;
    }
    auto variables = make_variable_map_up_until(m_to_project);
    writeSetScript("", variables, tempFile.fileStream("w+"),false);
}

void EnvScriptBuilder::writeSetScript(FILE *file)
{
    auto variables = make_variable_map_up_until(m_to_project);
    writeSetScript("", variables, file, false);
}

void EnvScriptBuilder::writeScripts(const std::string &setFileName, const std::string &unsetFileName)
{
    auto variables = make_variable_map_up_until(m_to_project);
    if (setFileName.size()) {
        FILE *out_file = fopen(setFileName.c_str(), "w+");
        if (!out_file) {
            fprintf(stderr, "Failed to open out file to write environment script to %s : %s\n",
                    setFileName.c_str(), strerror(errno));
            return;
        }
        std::string build_set_name = setFileName;
        build_set_name = dirname(&build_set_name[0]);
        writeSetScript(unsetFileName, variables, out_file,true);
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

static bool containsValue(const std::string &value, const std::list<EnvVariable> &list) {
    for (auto it = list.begin(); it != list.end(); it++) {
        if (it->value == value)
            return true;
    }
    return false;
}

void EnvScriptBuilder::populateMapFromVariableNode(const std::string &projectName, JT::ObjectNode *variableNode, std::map<std::string, std::list<EnvVariable>> &map) const
{
    for (auto it = variableNode->begin(); it != variableNode->end(); ++it) {
        EnvVariable variable;
        bool found = false;
        if (JT::ObjectNode *complex_variable = it->second->asObjectNode()) {
            variable.value = complex_variable->stringAt("value");
            variable.overwrite = complex_variable->booleanAt("overwrite");
            variable.singular = complex_variable->booleanAt("singular");
            found = true;
        } else if (JT::StringNode *simple_variable = it->second->asStringNode()) {
            variable.value = simple_variable->string();
            variable.overwrite = false;
            found = true;
        }
        if (found) {
            variable.value = m_build_environment.expandVariablesInString(variable.value, projectName);

            const std::string &variable_name = it->first.string();

            if (variable.overwrite || variable.singular) {
                map[variable_name].clear();
            }

            if (map.count(variable_name) && map[variable_name].front().singular) {
                map[variable_name].clear();
            }

            std::list<EnvVariable> &variable_list = map[variable_name];
            if (!containsValue(variable.value, variable_list))
                variable_list.push_back(variable);
        }
    }
}

void EnvScriptBuilder::populateMapFromEnvironmentNode(const std::string &projectName, JT::ObjectNode *environmentNode, std::map<std::string, std::list<EnvVariable>> &map) const
{
    JT::ObjectNode *local_variable = environmentNode->objectNodeAt("local");
    if (local_variable && projectName == m_to_project) {
        populateMapFromVariableNode(projectName, local_variable, map);
    }

    JT::ObjectNode *pre_variable = environmentNode->objectNodeAt("pre");
    if (pre_variable) {
        populateMapFromVariableNode(projectName, pre_variable, map);
    }

    JT::ObjectNode *post_variable = environmentNode->objectNodeAt("post");
    if (post_variable && projectName != m_to_project) {
        populateMapFromVariableNode(projectName, post_variable, map);
    }
}

void EnvScriptBuilder::populateMapFromProjectNode(const std::string &projectName, JT::ObjectNode *projectNode, std::map<std::string, std::list<EnvVariable>> &map) const
{
    for (auto it = projectNode->begin(); it != projectNode->end(); ++it) {
        if (it->first.string() != "env")
            continue;
        JT::ObjectNode *env_node = it->second->asObjectNode();
        if (!env_node)
            continue;
        populateMapFromEnvironmentNode(projectName, env_node, map);
    }
}

std::map<std::string, std::list<EnvVariable>> EnvScriptBuilder::make_variable_map_up_until(const std::string &project_name) const
{
    std::map<std::string, std::list<EnvVariable>> return_map;
    for (auto it = m_buildset_node->begin(); it != m_buildset_node->end(); ++it) {
        if (!it->second->asObjectNode())
            continue;
        JT::ObjectNode *project_node = it->second->asObjectNode();
        populateMapFromProjectNode(it->first.string(), project_node, return_map);
        if (project_name.size() && it->first.compareString(project_name))
                break;
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
    fprintf(file, "    if [ ! -z \"$%s\" ]; then\n", variable.c_str());
    fprintf(file, "        %s_VARIABLE_FOUND=""\n",variable.c_str());
    fprintf(file, "        IFS=':' read -ra %s_VARIABLE_SPLIT<<< \"$%s\"\n", variable.c_str(), variable.c_str());
    fprintf(file, "        for i in \"${%s_VARIABLE_SPLIT[@]}\"; do\n", variable.c_str());
    fprintf(file, "            [ \"$i\" == \"%s\" ] && %s_VARIABLE_FOUND=\"yes\"\n", dir.c_str(), variable.c_str());
    fprintf(file, "        done\n");
    fprintf(file, "        unset %s_VARIABLE_SPLIT\n", variable.c_str());
    fprintf(file, "        if [ -z \"$%s_VARIABLE_FOUND\" ]; then\n", variable.c_str());
    fprintf(file, "            if [ -z \"$BUILD_SHELL_OLD_%s\" ]; then\n", variable.c_str());
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

void EnvScriptBuilder::writeSetScript(const std::string &unsetFileName, const std::map<std::string, std::list<EnvVariable>> &variables, FILE *file, bool close)
{
    fprintf(file, "#!/bin/bash\n");
    fprintf(file, "\n");
    fprintf(file, "if [ ! -z \"$BUILD_SHELL_BUILD_DIR\" ] && [ -e \"$BUILD_SHELL_BUILD_DIR/build_shell/unset_build_env.sh\" ]; then\n");
    fprintf(file, "    source \"$BUILD_SHELL_BUILD_DIR/build_shell/unset_build_env.sh\"\n");
    fprintf(file, "fi\n");
    fprintf(file, "\n");
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
    fprintf(file, "if [ ! -z \"$BUILD_SHELL_OLD_%s\" ]; then\n", variable.c_str());
    fprintf(file, "    export %s=\"$BUILD_SHELL_OLD_%s\"\n", variable.c_str(), variable.c_str());
    fprintf(file, "    unset BUILD_SHELL_OLD_%s\n", variable.c_str());
    fprintf(file, "else\n");
    fprintf(file, "    unset %s\n", variable.c_str());
    fprintf(file, "fi\n");
    fprintf(file, "\n");
}

void write_strict_unset_for_variable(FILE *file, const std::string &variable)
{
    fprintf(file, "if [ ! -z \"$BUILD_SHELL_DEFINED_%s\" ]; then\n", variable.c_str());
    fprintf(file, "    unset %s\n", variable.c_str());
    fprintf(file, "    unset BUILD_SHELL_DEFINED_%s\n", variable.c_str());
    fprintf(file, "elif [ -n \"$BUILD_SHELL_OLD_%s\" ]; then\n", variable.c_str());
    fprintf(file, "    export %s=\"$BUILD_SHELL_OLD_%s\"\n", variable.c_str(), variable.c_str());
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

