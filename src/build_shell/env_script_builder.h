#ifndef ENV_SCRIPT_BUILDER_H
#define ENV_SCRIPT_BUILDER_H

#include "configuration.h"

#include <string>
#include <memory>
#include <map>
#include <list>
#include <vector>

namespace JT {
    class ObjectNode;
}
class EnvVariable
{
public:
    EnvVariable()
    { }
    EnvVariable(bool overwrite, const std::string &value)
        : overwrite(overwrite)
        , value(value)
    {}

    bool operator<(const EnvVariable &other) const
    { return value < other.value; }

    bool overwrite = false;
    std::string value;
};

class EnvScriptBuilder
{
public:
    EnvScriptBuilder(const Configuration &configuration);
    ~EnvScriptBuilder();

    void addProjectNode(JT::ObjectNode *project_root);

    void writeSetScript(const std::string &file);
    void writeSetScript(const std::string &file_name, FILE *file, bool close);

    void writeUnsetScript(const std::string &file);
    void writeUnsetScript(FILE *file, bool close);

private:
    void substitue_variable_value(std::string &value_string, JT::ObjectNode *project_root) const;

    JT::ObjectNode *m_node;
    const Configuration &m_configuration;
    std::string m_out_file;
};

#endif
