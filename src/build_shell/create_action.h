#ifndef CREATE_ACTION_H
#define CREATE_ACTION_H

#include "action.h"

namespace JT {
    class ObjectNode;
}

class CreateAction : public Action
{
public:
    CreateAction(const Configuration &configuration);
    ~CreateAction();

    bool execute();

private:
    bool handleCurrentSrcDir();

    JT::ObjectNode *m_out_tree;
};

#endif //CREATE_ACTION_H
