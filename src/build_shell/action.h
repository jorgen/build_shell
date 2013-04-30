#ifndef ACTION_H
#define ACTION_H

#include "configuration.h"

class Action
{
public:
    Action(const Configuration &configuration);
    virtual ~Action();

    virtual bool execute() = 0;
protected:
    const Configuration &m_configuration;
};

#endif //ACTION_H
