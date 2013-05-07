#ifndef ENV_SCRIPT_BUILDER_H
#define ENV_SCRIPT_BUILDER_H

#include <string>
#include <memory>
#include <map>
#include <list>

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
    EnvScriptBuilder();

    void addProjectNode(JT::ObjectNode *project_root);

    void writeSetScript(const std::string &file);
    void writeSetScript(const std::string &file_name, FILE *file, bool close);

    void writeUnsetScript(const std::string &file);
    void writeUnsetScript(FILE *file, bool close);

private:
    std::map<std::string, std::list<EnvVariable>>  m_root_env;
};

#endif
