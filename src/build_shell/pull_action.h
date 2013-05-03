#ifndef PULL_ACTION
#define PULL_ACTION

#include "action.h"
#include "tree_builder.h"

class PullAction : public Action
{
public:
    PullAction(const Configuration &configuration);
    ~PullAction();

    bool execute();

private:
    TreeBuilder m_buildset_tree_builder;
    JT::ObjectNode *m_buildset_tree;
};

#endif
