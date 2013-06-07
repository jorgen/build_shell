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

#include "build_environment.h"

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "json_tree.h"
#include "tree_builder.h"
#include "tree_writer.h"


static const char *strfind(const char *data, const char *pattern, size_t datasize)
{
    size_t pattern_size = strlen(pattern);
    size_t pattern_match = 0;

    const char *return_pos;
    for (size_t current_index = 0; current_index < datasize; current_index++) {
        if (*(data + current_index) == *(pattern + pattern_match)) {
            if (!pattern_match)
                return_pos = data + current_index;

            pattern_match++;
            if (pattern_match == pattern_size)
                return return_pos;
        } else {
            pattern_match = 0;
        }
    }
    return nullptr;
}

BuildEnvironment::BuildEnvironment(const Configuration &configuration)
    : m_configuration(configuration)
    , m_environment_file(configuration.buildDir() + "/build_shell/build_environment.json")
{
    if (access(m_environment_file.c_str(), R_OK) == 0) {
        TreeBuilder tree_builder(m_environment_file);
        tree_builder.load();
        m_environment_node.reset(tree_builder.takeRootNode());
    } else {
        m_environment_node.reset(new JT::ObjectNode());
    }
}

BuildEnvironment::~BuildEnvironment()
{
    mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH;
    int out_file = open(m_environment_file.c_str(), O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC, mode);

    if (out_file >= 0) {
        TreeWriter writer(out_file,true);
        writer.write(m_environment_node.get());
        if (writer.error()) {
            fprintf(stderr, "Failed to write BuildEnvironment\n");
        }
    } else {
        fprintf(stderr, "Could not open %s : %s\n", m_environment_file.c_str(), strerror(errno));

    }
}

std::string BuildEnvironment::getVariable(const std::string &variable, const std::string &project) const
{
    JT::ObjectNode *projects_node = m_environment_node->objectNodeAt("projects");
    if (projects_node) {
        JT::ObjectNode *project_environment = projects_node->objectNodeAt(project);
        if (project_environment) {
            JT::StringNode *variable_node = project_environment->stringNodeAt(variable);
            if (variable_node)
                return variable_node->string();

        }
    }

    JT::ObjectNode *default_node = m_environment_node->objectNodeAt("default");
    if (default_node) {
        JT::StringNode *variable_node = default_node->stringNodeAt(variable);
        if (variable_node)
            return variable_node->string();
    }

    if (variable == "project_src_path") {
        return m_configuration.srcDir() + "/" + project;
    } else if (variable == "project_build_path") {
        return m_configuration.buildDir() + "/" + project;
    } else if (variable == "build_path") {
        return m_configuration.buildDir();
    } else if (variable == "src_path") {
        return m_configuration.srcDir();
    } else if (variable == "install_path") {
        return m_configuration.installDir();
    }
    return std::string();
}

void BuildEnvironment::setVariable(const std::string &variable, const std::string &value, const std::string &project)
{
    JT::ObjectNode *insert_object;
    if (project.size()) {
        JT::ObjectNode *projects_node = m_environment_node->objectNodeAt("projects");
        if (!projects_node) {
            projects_node = new JT::ObjectNode();
            m_environment_node->insertNode(JT::Property("projects"), projects_node, true);
        }
        JT::ObjectNode *project_env = projects_node->objectNodeAt(project);
        if (!project_env) {
            project_env = new JT::ObjectNode();
            projects_node->insertNode(project, project_env, true);
        }
        insert_object = project_env;
    } else {
        JT::ObjectNode *default_node = m_environment_node->objectNodeAt("default");
        if (!default_node) {
            default_node = new JT::ObjectNode();
            m_environment_node->insertNode(JT::Property("default"), default_node, true);
        }
        insert_object = default_node;
    }

    JT::Token token;
    token.value_type = JT::Token::String;
    token.value.data = value.c_str();
    token.value.size = value.size();
    JT::Node *value_node = JT::Node::createValueNode(&token);
    insert_object->insertNode(variable, value_node, true);
}

const std::set<std::string> &BuildEnvironment::staticVariables() const
{
    if (m_static_variables.size() == 0) {
        BuildEnvironment *self = const_cast<BuildEnvironment *>(this);
        self->m_static_variables.insert(std::string("src_path"));
        self->m_static_variables.insert("build_path");
        self->m_static_variables.insert("install_path");
        self->m_static_variables.insert("project_src_path");
        self->m_static_variables.insert("project_build_path");
    }
    return m_static_variables;
}

bool BuildEnvironment::isStaticVariable(const std::string &variable) const
{
    return staticVariables().find(variable) != staticVariables().end();
}

static Variable next_variable(const char *data, size_t length)
{
    Variable returnVariable;
    if (length <= 3)
        return returnVariable;

    returnVariable.start = strfind(data, "{$", length);

    if (!returnVariable.start)
        return returnVariable;

    size_t diff = (returnVariable.start - data);

    size_t rest = length - diff;
    if (rest < 4) {
        returnVariable.start = 0;
        return Variable();
    }

    if (diff && *(returnVariable.start - 1) == '\\') {
        //escaped sequence. Ignoring
        if (diff < length) {
            diff+=2;
            return next_variable(data + diff, length - diff);
        } else {
            return Variable();
        }
    }

    const char *end = strfind(returnVariable.start + 2, "}", rest - 2);

    if (!end)
        return Variable();

    returnVariable.start +=2;

    if (returnVariable.start > end)
        return Variable();
    returnVariable.size = (end) - returnVariable.start;

    return returnVariable;
}

const std::string BuildEnvironment::expandVariablesInString(const std::string &str, const std::string &project) const
{
    auto variables = findVariables(str.c_str(), str.size());
    if (!variables.size())
        return str;

    std::string return_str = str;
    size_t diff = 0;
    for (auto it = variables.begin(); it != variables.end(); ++it) {
        std::string variable_name(it->start, it->size);
        std::string variable_value = getVariable(variable_name, project);
        if (!variable_value.size()) {
            fprintf(stderr, "Could not expand variable %s for project %s\n", variable_name.c_str(), project.c_str());
            continue;
        }
        size_t pos = it->start - str.c_str();
        return_str.replace(pos - 2, it->size + 3,variable_value);
        diff += variable_value.size() - (variable_name.size() + 3);
    }

    return return_str;
}

bool BuildEnvironment::canResolveVariable(const std::string &variable, const std::string &project) const
{
    return getVariable(variable, project).size();
}

const std::list<Variable> BuildEnvironment::findVariables(const char *str, const size_t size)
{
    std::list<Variable> return_list;
    const char *start = str;
    size_t size_left = size;
    while (start && size_left) {
        Variable variable = next_variable(start, size_left);
        if (!variable.start)
            break;
        return_list.push_back(variable);

        start = variable.start + variable.size + 1;
        size_left = size - (start - str);
    }
    return return_list;
}

