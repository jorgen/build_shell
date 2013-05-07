#ifndef BUILD_ACTION_H
#define BUILD_ACTION_H

#include "tree_builder.h"
#include "json_tree.h"
#include "action.h"
#include "env_script_builder.h"

class BuildAction : public Action
{
public:
    BuildAction(const Configuration &configuration);
    ~BuildAction();

    bool execute();
private:
    bool handleBuildForProject(const std::string &projectName, const std::string &buildSystem, JT::ObjectNode *projectNode, JT::ObjectNode **updatedProjectNode);
    TreeBuilder m_buildset_tree_builder;
    JT::ObjectNode *m_buildset_tree;
    std::string m_stored_buildset;
    std::string m_set_build_env_file;
    std::string m_unset_build_env_file;
    EnvScriptBuilder m_env_script_builder;
};

#endif
