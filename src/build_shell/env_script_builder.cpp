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

#include <algorithm>

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
    auto variables = make_variable_list_for(m_to_project, clean_environment());
    writeSetScript("", variables, tempFile.fileStream("w+"),false);
}

void EnvScriptBuilder::writeSetScript(FILE *file)
{
    auto variables = make_variable_list_for(m_to_project, clean_environment());
    writeSetScript("", variables, file, false);
}

void EnvScriptBuilder::writeScripts(const std::string &setFileName, const std::string &unsetFileName)
{
    auto variables = make_variable_list_for(m_to_project, clean_environment());
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

class RemoveVariable
{
public:
    RemoveVariable(const EnvVariable &variable)
        : m_variable(variable)
    { }
    bool operator() (const EnvVariable& variable){
        if (m_variable.overwrite && m_variable.name == variable.name)
            return true;
        if (variable.singular && m_variable.name == variable.name)
            return true;
        return false;
    }
private:
    const EnvVariable &m_variable;
};

static std::list<std::string> get_variable_values(const std::string &values, const std::string &seperator)
{
    std::list<std::string> return_variables;

    size_t start_seperator = 0;
    while (start_seperator < values.size()) {
        size_t end_seperator = values.find(seperator, start_seperator + 1);
        if (end_seperator - start_seperator > 0) {
            std::string value = values.substr(start_seperator, end_seperator - start_seperator);
            return_variables.push_back(value);
        }
        if (end_seperator < values.size())
            start_seperator = end_seperator + 1;
        else
            start_seperator = end_seperator;
    }
    return return_variables;
}

static std::list<std::string> get_variable_values(JT::Node *variable_node, const std::string &seperator)
{
    std::list<std::string> return_variables;
    if (!variable_node)
        return return_variables;
    if (variable_node->asStringNode()) {
        return_variables = get_variable_values(variable_node->asStringNode()->string(), seperator);
    } else if (variable_node->asArrayNode()) {
        JT::ArrayNode *value_array = variable_node->asArrayNode();
        for (size_t i = 0; i < value_array->size(); i++) {
            if (value_array->index(i)->asStringNode()) {
                const std::string &values = value_array->index(i)->asStringNode()->string();
                return_variables.splice(return_variables.end(), get_variable_values(values,seperator));
            }
        }
    }
    return return_variables;
}

void EnvScriptBuilder::populateListFromVariableNode(const std::string &projectName, JT::ObjectNode *variableNode, std::list<EnvVariable> &variables) const
{
    std::list<std::string> value_list;
    for (auto it = variableNode->begin(); it != variableNode->end(); ++it) {
        EnvVariable variable(projectName, it->first.string());

        if (JT::ObjectNode *complex_variable = it->second->asObjectNode()) {
            variable.overwrite = complex_variable->booleanAt("overwrite");
            variable.singular = complex_variable->booleanAt("singular");
            auto seperatorNode = complex_variable->stringNodeAt("seperator");
            if (seperatorNode)
                variable.seperator = seperatorNode->string();
            value_list = get_variable_values(complex_variable->nodeAt("value"), variable.seperator);
        } else {
            value_list = get_variable_values(it->second, variable.seperator);
        }
        if (!value_list.empty()) {
            for (const std::string &un_expanded_variable : value_list) {
                variable.values.push_back(m_build_environment.expandVariablesInString(un_expanded_variable, projectName));
            }

            variables.remove_if(RemoveVariable(variable));

            variables.push_back(variable);
        }
    }
}

void EnvScriptBuilder::populateListFromEnvironmentNode(const std::string &projectName, JT::ObjectNode *environmentNode, std::list<EnvVariable> &variables) const
{
    JT::ObjectNode *local_variable = environmentNode->objectNodeAt("local");
    if (local_variable && projectName == m_to_project) {
        populateListFromVariableNode(projectName, local_variable, variables);
    }

    JT::ObjectNode *pre_variable = environmentNode->objectNodeAt("pre");
    if (pre_variable) {
        populateListFromVariableNode(projectName, pre_variable, variables);
    }

    JT::ObjectNode *post_variable = environmentNode->objectNodeAt("post");
    if (post_variable && projectName != m_to_project) {
        populateListFromVariableNode(projectName, post_variable, variables);
    }
}

void EnvScriptBuilder::populateListFromProjectNode(const std::string &projectName, JT::ObjectNode *projectNode, std::list<EnvVariable> &variables) const
{
    for (auto it = projectNode->begin(); it != projectNode->end(); ++it) {
        if (it->first.string() != "env")
            continue;
        JT::ObjectNode *env_node = it->second->asObjectNode();
        if (!env_node)
            continue;
        populateListFromEnvironmentNode(projectName, env_node, variables);
    }
}

class RemoveVariableIfInList
{
public:
    RemoveVariableIfInList(const EnvVariable &variable)
        : m_variable(variable)
    {
    }

    bool operator() (const EnvVariable& variable) {
        if (m_variable.name == variable.name) {
            for (auto it = m_variable.values.begin(); it != m_variable.values.end(); ++it) {
                if (std::find(variable.values.begin(), variable.values.end(), *it) != variable.values.end()) {
                    return true;
                }
            }
        }
        return false;
    }
private:
    const EnvVariable &m_variable;
};

static void add_if_not_existent(std::list<EnvVariable> &to_add, std::list<EnvVariable> &target)
{
    for (auto it = target.begin(); it != target.end(); ++it) {
        to_add.remove_if(RemoveVariableIfInList(*it));
    }

    target.splice(target.end(), to_add);
}

std::list<EnvVariable> EnvScriptBuilder::make_variable_list_for(const std::string &project_name, bool clean_environment) const
{
    std::list<EnvVariable> return_list;
    if (clean_environment) {
        JT::ObjectNode *project_node = m_buildset_node->objectNodeAt(project_name);
        if (project_node) {
            populateListFromProjectNode(project_name, project_node, return_list);
        }
    } else {
        for (auto it = m_buildset_node->begin(); it != m_buildset_node->end(); ++it) {
            if (!it->second->asObjectNode())
                continue;
            JT::ObjectNode *project_node = it->second->asObjectNode();
            populateListFromProjectNode(it->first.string(), project_node, return_list);
            if (project_name.size() && it->first.compareString(project_name))
                break;
        }
    }

    if (!clean_environment && m_configuration.installDir() != "/usr" && m_configuration.installDir() != "/usr/local") {
        std::list<EnvVariable> build_shell_variables;
#ifdef __APPLE__
        const std::string library_path("DYLD_LIBRARY_PATH");
#else
        const std::string library_path("LD_LIBRARY_PATH");
#endif
        std::string install_lib = m_configuration.installDir() + "/lib";
        std::string install_bin = m_configuration.installDir() + "/bin";
        std::string install_pkg_config = install_lib + "/pkgconfig";

        EnvVariable library_path_variable("build_shell", library_path, install_lib);
        library_path_variable.directory = true;
        build_shell_variables.push_back(library_path_variable);

        EnvVariable bin_path_variable("build_shell", "PATH", install_bin);
        bin_path_variable.directory = true;
        build_shell_variables.push_back(bin_path_variable);

        EnvVariable pkg_config_variable("build_shell", "PKG_CONFIG_PATH", install_pkg_config);
        pkg_config_variable.directory = true;
        build_shell_variables.push_back(pkg_config_variable);

        add_if_not_existent(build_shell_variables, return_list);
    }
    return return_list;
}

static void writeEnvironmentVariable(FILE *file, const EnvVariable &variable, const std::string &requested_indent = "")
{

    std::string value;
    bool first = true;
    for (auto it = variable.values.begin(); it != variable.values.end(); ++it) {
        if (first) {
            first = false;
            value = *it;
        } else {
            value += variable.seperator + (*it);
        }
    }
    const std::string build_shell_project_variable = "BUILD_SHELL_" + variable.project + "_" + variable.name;
    const std::string build_shell_project_variable_set = build_shell_project_variable + "_SET";
    std::string indent = requested_indent;
    if (variable.directory) {
        fprintf(file, "%sif [ -d \"%s\" ]; then\n", indent.c_str(), variable.values.front().c_str());
        indent += "    ";
    }
    fprintf(file, "%sexport %s=$%s\n", indent.c_str(), build_shell_project_variable.c_str(), variable.name.c_str());
    fprintf(file, "%sexport %s=1\n", indent.c_str(), build_shell_project_variable_set.c_str());
    if (variable.overwrite || variable.singular) {
        fprintf(file, "%sexport %s=\"%s\"\n", indent.c_str(), variable.name.c_str(), value.c_str());
    } else {
        fprintf(file, "%sexport %s=\"%s%s$%s\"\n", indent.c_str(), variable.name.c_str(), value.c_str(),variable.seperator.c_str(), variable.name.c_str());
    }
    if (variable.directory) {
        indent.erase(0,4);
        fprintf(file, "%sfi\n", indent.c_str());
    }
}

void EnvScriptBuilder::writeSetScript(const std::string &unsetFileName, const std::list<EnvVariable> &variables, FILE *file, bool close)
{
    fprintf(file, "#!/bin/bash\n");
    fprintf(file, "\n");
    fprintf(file, "if [ ! -z \"$BUILD_SHELL_UNSET_ENV_FILE\" ]; then\n");
    fprintf(file, "    source \"$BUILD_SHELL_UNSET_ENV_FILE\"\n");
    fprintf(file, "elif [ ! -z \"$BUILD_SHELL_BUILD_DIR\" ] && [ -e \"$BUILD_SHELL_BUILD_DIR/build_shell/unset_build_env.sh\" ]; then\n");
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
        if (it->values.empty())
            continue;
        writeEnvironmentVariable(file, *it);
        fprintf(file, "\n");
    }

    if (close)
        fclose(file);
}

void EnvScriptBuilder::writeUnsetScript(const std::string &file, const std::list<EnvVariable> &variables)
{
    FILE *out_file = fopen(file.c_str(), "w+");
    writeUnsetScript(out_file, true, variables);
}

static void write_unset_for_variable(FILE *file, const EnvVariable &variable, const std::string &indent = "")
{
    const std::string build_shell_project_variable = "BUILD_SHELL_" + variable.project + "_" + variable.name;
    const std::string build_shell_project_variable_set = build_shell_project_variable + "_SET";
    fprintf(file, "%sif [ ! -z \"$%s\" ]; then\n", indent.c_str(), build_shell_project_variable_set.c_str());
    fprintf(file, "%s    if [ -z \"$%s\" ]; then\n",indent.c_str(), build_shell_project_variable.c_str());
    fprintf(file, "%s        unset %s\n", indent.c_str(), variable.name.c_str());
    fprintf(file, "%s        unset %s\n", indent.c_str(), build_shell_project_variable.c_str());
    fprintf(file, "%s    else\n", indent.c_str());
    fprintf(file, "%s        export %s=$%s\n", indent.c_str(), variable.name.c_str(), build_shell_project_variable.c_str());
    fprintf(file, "%s        unset %s\n", indent.c_str(), build_shell_project_variable.c_str());
    fprintf(file, "%s    fi\n", indent.c_str());
    fprintf(file, "%s    unset %s\n", indent.c_str(), build_shell_project_variable_set.c_str());
    fprintf(file, "%sfi\n", indent.c_str());
}

void EnvScriptBuilder::writeUnsetScript(FILE *file, bool close_file, const std::list<EnvVariable> &variables)
{
    fprintf(file, "#!/bin/bash\n");
    fprintf(file, "\n");
    fprintf(file, "unset BUILD_SHELL_NAME\n");
    fprintf(file, "unset BUILD_SHELL_UNSET_ENV_FILE\n");
    fprintf(file, "unset BUILD_SHELL_SRC_DIR\n");
    fprintf(file, "unset BUILD_SHELL_BUILD_DIR\n");
    fprintf(file, "unset BUILD_SHELL_INSTALL_DIR\n");
    fprintf(file, "\n");

    for (auto it = variables.rbegin(); it != variables.rend(); ++it) {
        write_unset_for_variable(file, *it);
        fprintf(file, "\n");
    }

    if (close_file)
        fclose(file);
}

bool EnvScriptBuilder::clean_environment() const
{
    JT::ObjectNode *project_node = m_buildset_node->objectNodeAt(m_to_project);
    return project_node && project_node->booleanAt("clean_environment");
}
