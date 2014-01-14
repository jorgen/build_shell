/*
 * Copyright © 2013 Jørgen Lind

 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.

 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
*/
#include "available_builds.h"

#include "json_tree.h"
#include "tree_writer.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <algorithm>

AvailableBuilds::AvailableBuilds(const Configuration &configuration)
    : m_configuration(configuration)
    , m_available_builds_file(m_configuration.buildShellConfigDir() + "/available_builds.json")
{
    Configuration::ensurePath(m_configuration.buildShellConfigDir());

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
            --it;
            continue;
        }

        const std::string &name = it->second->stringAt("name");
        if (name.size()) {
            fprintf(stdout, "%s\n", name.c_str());
        } else {
            fprintf(stdout, "%s\n", it->first.string().c_str());
        }
    }

    if (altered) {
        flushTreeToFile(root);
    }

    delete root;
}

void AvailableBuilds::printGetEnv(const std::string &identifier)
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
            --it;
            continue;
        }

        const std::string &name = it->second->stringAt("name");
        bool found = false;
        if (name.size()) {
            if (name == identifier)
                found = true;
        } else {
            if (it->first.string() == identifier)
                found = true;
        }

        if (found) {
            const std::string &setenv_file = it->second->stringAt("set_env_file");
            fprintf(stdout, "%s\n", setenv_file.c_str());
        }
    }

    if (altered) {
        flushTreeToFile(root);
    }

    delete root;
}

void AvailableBuilds::addAvailableBuild(const std::string path, const std::string set_env_file)
{
    JT::ObjectNode *root = rootNode();
    if (!root) {
        fprintf(stderr, "Could not find any json structure in  %s\n", m_available_builds_file.c_str());
        return;
    }
    JT::ObjectNode *build = root->objectNodeAt(path, "%$.$%");
    if (!build) {
        build = new JT::ObjectNode();
        root->insertNode(path,build);
    }
    build->addValueToObject("set_env_file", set_env_file, JT::Token::String, "%$.$%");
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
    TreeWriter writer(out_file, true);
    writer.write(root);
    if (writer.error())
        fprintf(stderr, "Something whent wrong when writing to available builds file %s\n",
                m_available_builds_file.c_str());
}

JT::ObjectNode *AvailableBuilds::rootNode() const
{
    TreeBuilder tree(m_available_builds_file);
    tree.load();
    return tree.takeRootNode();
}
