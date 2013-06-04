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
#include <map>
#include <list>
#include <set>

namespace JT {
    class ObjectNode;
}
class EnvVariable
{
public:
    EnvVariable()
        : overwrite(false)
    { }
    EnvVariable(bool overwrite, const std::string &value)
        : overwrite(overwrite)
        , value(value)
    {}

    bool overwrite;
    std::string value;

};

class EnvScriptBuilder
{
public:
    EnvScriptBuilder(const Configuration &configuration, JT::ObjectNode *buildset_node);
    ~EnvScriptBuilder();

    void writeSetScript(TempFile &tempFile, const std::string &toProject);
    void writeScripts(const std::string &setFileName, const std::string &unsetFileName, const std::string &toProject);

private:
    std::map<std::string, std::list<EnvVariable>> make_variable_map_up_until(const std::string &project_name) const;

    void writeSetScript(const std::string &unsetFileName, const std::map<std::string, std::list<EnvVariable>> &variables, FILE *file, bool close);
    void writeUnsetScript(const std::string &file, const std::map<std::string, std::list<EnvVariable>> &variables);
    void writeUnsetScript(FILE *file, bool close, const std::map<std::string, std::list<EnvVariable>> &variables);

    void populateMapFromEnvironmentNode(const std::string &projectName, JT::ObjectNode *environmentNode, std::map<std::string, std::list<EnvVariable>> &map) const;
    void populateMapFromProjectNode(const std::string &projectName, JT::ObjectNode *projectNode, std::map<std::string, std::list<EnvVariable>> &map) const;

    const Configuration &m_configuration;
    JT::ObjectNode *m_buildset_node;
    BuildEnvironment m_build_environment;
};

#endif
