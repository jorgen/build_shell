#include "available_builds.h"

#include "json_tree.h"
#include "tree_writer.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

AvailableBuilds::AvailableBuilds(const Configuration &configuration)
    : m_configuration(configuration)
    , m_available_builds_file(m_configuration.buildShellConfigPath() + "/available_builds.json")
{
    if (access(m_available_builds_file.c_str(), F_OK)) {
        JT::ObjectNode *root = new JT::ObjectNode();
        flushTreeToFile(root);
    }
}

AvailableBuilds::~AvailableBuilds()
{

}

void AvailableBuilds::printAvailable()
{
    JT::ObjectNode *root = rootNode();
    if (!root) {
        fprintf(stderr, "Could not find any json structure in  %s\n", m_available_builds_file.c_str());
        return;
    }
    bool altered = false;
    for (auto it = root->begin(); it != root->end(); ++it) {
        const std::string &setenv_file = it->second->stringAt("set_env_file");
        if (!setenv_file.size() || access(setenv_file.c_str(), F_OK)) {
            altered = true;
            delete root->take(it->first.string());
            continue;
        }

        fprintf(stdout, "%s ", it->first.string().c_str());
    }

    fprintf(stdout, "\n");

    if (altered) {
        flushTreeToFile(root);
    }

    delete root;
}

void AvailableBuilds::addAvailableBuild(const std::string name, const std::string set_env_file)
{
    JT::ObjectNode *root = rootNode();
    if (!root) {
        fprintf(stderr, "Could not find any json structure in  %s\n", m_available_builds_file.c_str());
        return;
    }
    JT::ObjectNode *build = new JT::ObjectNode();
    root->insertNode(name,build);
    build->addValueToObject("set_env_file", set_env_file, JT::Token::String);
    flushTreeToFile(root);
}

void AvailableBuilds::flushTreeToFile(JT::ObjectNode *root) const
{
    mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH;
    int out_file = open(m_available_builds_file.c_str(), O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC, mode);
    if (out_file < 0) {
        fprintf(stderr, "Failed to open available builds file %s, %s\n",
                m_available_builds_file.c_str(), strerror(errno));
    }
    TreeWriter writer(out_file, root, true);
    if (writer.error())
        fprintf(stderr, "Something whent wrong when writing to available builds file %s\n",
                m_available_builds_file.c_str());
}

JT::ObjectNode *AvailableBuilds::rootNode() const
{
    TreeBuilder tree(m_available_builds_file);
    return tree.takeRootNode();
}
