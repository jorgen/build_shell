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
#ifndef ENV_SCRIPT_BUILDER_H
#define ENV_SCRIPT_BUILDER_H

#include "configuration.h"
#include "temp_file.h"
#include "build_environment.h"

#include <string>
#include <memory>
#include <list>
#include <set>

namespace JT {
    class ObjectNode;
}
class EnvVariable
{
public:
    EnvVariable(const std::string &project, const std::string &name)
        : overwrite(false)
        , singular(false)
        , directory(false)
        , project(project)
        , name(name)
        , seperator(":")
    { }
    EnvVariable(const std::string &project, const std::string &name, const std::string &value)
        : overwrite(false)
        , singular(false)
        , directory(false)
        , project(project)
        , name(name)
        , seperator(":")
    {
        values.push_back(value);
    }

    bool overwrite;
    bool singular;
    bool directory;
    const std::string project;
    const std::string name;
    std::list<std::string> values;
    std::string seperator;

};

class EnvScriptBuilder
{
public:
    EnvScriptBuilder(const Configuration &configuration, const BuildEnvironment &buildEnvironment, JT::ObjectNode *buildset_node);
    ~EnvScriptBuilder();

    void setToProject(const std::string &toProject);

    void writeSetScript(TempFile &tempFile);
    void writeSetScript(FILE *file);
    void writeScripts(const std::string &setFileName, const std::string &unsetFileName);

private:
    std::list<EnvVariable> make_variable_list_for(const std::string &project_name, bool clean_environment) const;

    void writeSetScript(const std::string &unsetFileName, const std::list<EnvVariable> &variables, FILE *file, bool close);
    void writeUnsetScript(const std::string &file, const std::list<EnvVariable> &variables);
    void writeUnsetScript(FILE *file, bool close, const std::list<EnvVariable> &variables);

    void populateListFromVariableNode(const std::string &projectName, JT::ObjectNode *variableNode, std::list<EnvVariable> &variables) const;
    void populateListFromEnvironmentNode(const std::string &projectName, JT::ObjectNode *environmentNode, std::list<EnvVariable> &variables) const;
    void populateListFromProjectNode(const std::string &projectName, JT::ObjectNode *projectNode, std::list<EnvVariable> &variables) const;
    bool clean_environment() const;

    const Configuration &m_configuration;
    const BuildEnvironment &m_build_environment;
    const JT::ObjectNode * const m_buildset_node;
    std::string m_to_project;
};

#endif
