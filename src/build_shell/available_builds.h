#ifndef AVAILABLE_BUILDS_H
#define AVAILABLE_BUILDS_H

#include "tree_builder.h"
#include "configuration.h"

class AvailableBuilds
{
public:
    AvailableBuilds(const Configuration &configuration);
    ~AvailableBuilds();

    void printAvailable();
    void addAvailableBuild(const std::string name, const std::string set_env_file);

private:
    void flushTreeToFile(JT::ObjectNode *object) const;
    JT::ObjectNode *rootNode() const;
    const Configuration &m_configuration;
    std::string m_available_builds_file;
};

#endif
