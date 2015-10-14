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
#include "pull_action.h"

#include "json_tree.h"
#include "tree_writer.h"
#include "process.h"
#include "correct_branch_action.h"

#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <memory>

#include <assert.h>

class RemoveArgumentNode
{
public:
    RemoveArgumentNode(JT::ObjectNode *to_contain_argument, JT::ObjectNode *arguments_node)
        : m_to_contain_argument(to_contain_argument)
        , m_argument_node(arguments_node)
    {
        m_to_contain_argument->insertNode(std::string("arguments"), arguments_node, true);
    }

    ~RemoveArgumentNode()
    {
        JT::Node *removed_argnode = m_to_contain_argument->take("arguments");
        assert(removed_argnode);
    }

    JT::ObjectNode *m_to_contain_argument;
    JT::ObjectNode *m_argument_node;
};

PullAction::PullAction(const Configuration &configuration)
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

PullAction::~PullAction()
{
}

bool PullAction::execute()
{
    if (!m_buildset_tree || m_error)
        return false;

    std::unique_ptr<JT::ObjectNode> arguments(new JT::ObjectNode());
    arguments->addValueToObject("reset_to_sha", "true", JT::Token::Bool);

    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    auto end_it = endIterator(m_buildset_tree);
    for (auto it = startIterator(m_buildset_tree); it != end_it; ++it) {
        JT::ObjectNode *project_node = it->second->asObjectNode();

        if (!project_node || !project_node->objectNodeAt("scm"))
            continue;

        RemoveArgumentNode remove_argument_handler(project_node, arguments.get());

        if (chdir(m_configuration.srcDir().c_str())) {
            fprintf(stderr, "Could not move into src dir:%s\n%s\n",
                    m_configuration.srcDir().c_str(), strerror(errno));
            return false;
        }

        const std::string &project_name = it->first.string();

        JT::ObjectNode *scm_node = project_node->objectNodeAt("scm");

        if (!determinAndRunScmAction( project_name, scm_node))
            return false;

        if (JT::ArrayNode *sub_repos = scm_node->arrayNodeAt("sub_repos")) {
            for (int i = 0; i < sub_repos->size(); i++) {
                if (JT::ObjectNode *sub_repo = const_cast<JT::ObjectNode *>(sub_repos->index(i)->asObjectNode())) {
                    const std::string &path = sub_repo->stringAt("path");
                    const std::string &name = sub_repo->stringAt("name");
                    if (path.size() == 0 || name.size() == 0) {
                        fprintf(stderr, "Missing name or path for sub_repo %s. Skipping\n", sub_repo->stringAt("url").c_str());
                        return false;;
                    }
                    if (chdir(std::string(m_configuration.srcDir() + "/" + project_name + "/" + path).c_str())) {
                        fprintf(stderr, "Could not move into src dir:%s\n%s\n",
                                m_configuration.srcDir().c_str(), strerror(errno));
                        return false;
                    }
                    fprintf(stderr, "found sub_repo %s\n", name.c_str());
                    if (!determinAndRunScmAction(name, sub_repo))
                        return false;
                }
            }
        }

    }

    return true;
}

bool PullAction::determinAndRunScmAction(const std::string &rel_path, JT::ObjectNode *scmNode)
{
        bool should_clone = false;
        bool should_pull = false;
        struct stat stat_buffer;
        if (stat(rel_path.c_str(), &stat_buffer)) {
                should_clone = true;
        } else if (S_ISDIR(stat_buffer.st_mode)) {
            should_pull = true;
            if (chdir(rel_path.c_str())) {
                char current_wd[PATH_MAX];
                if (getcwd(current_wd, sizeof current_wd) == 0)
                    fprintf(stderr, "failed to get cwd\n");
                fprintf(stderr, "Failed to move into directory %s/%s : %s\n",
                        current_wd, rel_path.c_str(), strerror(errno));
                m_error = true;
                return false;
            }
        }

        if (!should_clone && !should_pull) {
            fprintf(stderr, "Don't know how to handle: %s in pull mode. Is it a regular file? Skipping.\n",
                    rel_path.c_str());
        }

        ScmAction action = should_clone? Clone : Pull;
        return runPullActionForScmNode(rel_path, action, scmNode);
}

bool PullAction::runPullActionForScmNode(const std::string &name, PullAction::ScmAction action, JT::ObjectNode *scm_node)
{
        std::string fallback_script;
        std::string mode = action == Clone ? "clone" : "pull";
        JT::StringNode *string_node = scm_node->stringNodeAt("type");
        if (string_node) {
            if (string_node->string() == "git") {
                fallback_script.append("git");
            } else {
                fallback_script.append("regular_dir");
            }
        } else {
            fallback_script.append("regular_dir");
        }

        bool success;
        {
            Process process(m_configuration);
            process.setPhase(mode);
            process.setProjectName(name);
            process.setFallback(fallback_script);
            process.setProjectNode(scm_node);
            process.setPrint(true);
            success = process.run(nullptr);
        }

        if (success && m_configuration.correctBranch()) {
            success = CorrectBranchAction::handleProjectNode(m_configuration, name, scm_node);
        }

        return success;
}
