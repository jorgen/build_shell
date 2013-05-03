#ifndef ACTION_H
#define ACTION_H

#include "configuration.h"

class Action
{
public:
    Action(const Configuration &configuration);
    virtual ~Action();

    virtual bool execute() = 0;

    bool error() const;

protected:
    const Configuration &m_configuration;
    bool m_error;
};

#endif //ACTION_H
