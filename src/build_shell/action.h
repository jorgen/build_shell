#ifndef ACTION_H
#define ACTION_H

#include "configuration.h"

namespace JT {
    class ObjectNode;
}

class Action
{
public:
    Action(const Configuration &configuration);
    virtual ~Action();

    virtual bool execute() = 0;

    bool error() const;

protected:
    static bool flushProjectNodeToTemporaryFile(const std::string &project_name, JT::ObjectNode *node, std::string &file_flushed_to);
    const Configuration &m_configuration;
    bool m_error;
};

#endif //ACTION_H
