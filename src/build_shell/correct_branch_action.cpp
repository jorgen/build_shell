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
#include "correct_branch_action.h"

#include "json_tree.h"
#include "tree_writer.h"
#include "process.h"

#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <memory>

#include <assert.h>

CorrectBranchAction::CorrectBranchAction(const Configuration &configuration)
    : Action(configuration)
    , m_buildset_tree_builder(configuration.buildsetFile())
{
    m_buildset_tree_builder.load();
    m_buildset_tree = m_buildset_tree_builder.rootNode();
    if (!m_buildset_tree) {
        fprintf(stderr, "Error loading buildset %s\n",
                configuration.buildsetFile().c_str());
        m_error = true;
    }
}

CorrectBranchAction::~CorrectBranchAction()
{
}

bool CorrectBranchAction::execute()
{
    if (!m_buildset_tree || m_error)
        return false;

    std::unique_ptr<JT::ObjectNode> arguments(new JT::ObjectNode());

    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    auto end_it = endIterator(m_buildset_tree);
    for (auto it = startIterator(m_buildset_tree); it != end_it; ++it) {
        JT::ObjectNode *project_node = it->second->asObjectNode();
        if (!project_node)
            continue;

        if (chdir(m_configuration.srcDir().c_str())) {
            fprintf(stderr, "Could not move into src dir:%s\n%s\n",
                    m_configuration.srcDir().c_str(), strerror(errno));
            return false;
        }

        project_node->insertNode(std::string("arguments"), arguments.get(), true);
        const std::string &project_name = it->first.string();

        bool success = handleProjectNode(m_configuration, project_name, project_node);

        //have to remove the project node, so it will not be deleted multiple times
        JT::Node *removed_argnode = project_node->take("arguments");
        assert(removed_argnode);

        if (!success) {
            m_error = true;
            return false;
        }
    }

    return true;
}

bool CorrectBranchAction::handleProjectNode(const Configuration &configuration, const std::string &project_name, JT::ObjectNode *project_node)
{
    if (chdir(project_name.c_str())) {
        fprintf(stderr, "Failed to move into directory %s : %s\n",
                project_name.c_str(), strerror(errno));
        return false;
    }


    std::string fallback_script;
    std::string mode = "correct_branch";
    JT::StringNode *string_node = project_node->stringNodeAt("scm.type");
    if (string_node) {
        if (string_node->string() == "git") {
            fallback_script.append("git");
        } else {
            fallback_script.append("regular_dir");
        }
    } else {
        fallback_script.append("regular_dir");
    }

    Process process(configuration);
    process.setPhase(mode);
    process.setProjectName(project_name);
    process.setFallback(fallback_script);
    process.setProjectNode(project_node);
    process.setPrint(true);
    return  process.run(nullptr);
}

