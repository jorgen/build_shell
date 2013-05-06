#ifndef BUILD_ACTION_H
#define BUILD_ACTION_H

#include "tree_builder.h"
#include "json_tree.h"
#include "action.h"

class BuildAction : public Action
{
public:
    BuildAction(const Configuration &configuration);
    ~BuildAction();

    bool execute();
private:
    bool handleBuildForProject(const std::string &projectName, JT::ObjectNode *projectNode, const std::string &buildSystem);
    TreeBuilder m_buildset_tree_builder;
    JT::ObjectNode *m_buildset_tree;
};

#endif
