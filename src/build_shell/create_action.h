#ifndef CREATE_ACTION_H
#define CREATE_ACTION_H

#include "action.h"

class CreateAction : public Action
{
public:
    CreateAction(const Configuration &configuration);
    ~CreateAction();

    bool execute();

private:
    bool handleCurrentSrcDir();
};

#endif //CREATE_ACTION_H
