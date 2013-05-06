#include "pull_action.h"

#include "json_tree.h"
#include "tree_writer.h"

#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include <assert.h>

PullAction::PullAction(const Configuration &configuration)
    : Action(configuration)
    , m_buildset_tree_builder(configuration.buildsetFile())
{
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

    for (auto it = m_buildset_tree->begin(); it != m_buildset_tree->end(); ++it) {
        JT::ObjectNode *project_node = it->second->asObjectNode();
        if (!project_node)
            continue;

        if (chdir(m_configuration.srcDir().c_str())) {
            fprintf(stderr, "Could not move into src dir:%s\n%s\n",
                    m_configuration.srcDir().c_str(), strerror(errno));
            return false;
        }

        project_node->insertNode(std::string("arguments"), arguments.get(), true);
        const std::string project_name = it->first.string();

        bool should_clone = false;
        bool should_pull = false;
        struct stat stat_buffer;
        if (stat(project_name.c_str(), &stat_buffer)) {
            if (errno == ENOENT) {
                should_clone = true;
            }
        }

        if (S_ISDIR(stat_buffer.st_mode)) {
            should_pull = true;
            chdir(project_name.c_str());
        }

        if (!should_clone && !should_pull) {
            fprintf(stderr, "Don't know how to handle: %s in pull mode. Is it a regular file? Skipping.\n",
                    project_name.c_str());
        }
        std::string temp_file;
        flushProjectNodeToTemporaryFile(project_name,project_node,temp_file);
        //have to remove the project node, so it will not be deleted multiple times
        JT::Node *removed_argnode = project_node->take("arguments");
        assert(removed_argnode);

        std::string mode = should_clone? "clone_" : "pull_";
        std::string primary_script = mode;
        primary_script.append(project_name);

        std::string fallback_script = mode;

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

        auto scripts = m_configuration.findScript(primary_script, fallback_script);
        if (!scripts.size()) {
            fprintf(stderr, "Could not find any scripts for primary_script: %s or fallback_script %s\n",
                    primary_script.c_str(), fallback_script.c_str());
            return false;
        }

        bool found = false;
        for (auto it = scripts.begin(); it != scripts.end(); ++it) {
            int exit_code = m_configuration.runScript(*it, temp_file.c_str());
            if (exit_code) {
                fprintf(stderr, "Error while running script %s\n", it->c_str());
                return false;
            }

            if (access(temp_file.c_str(), F_OK)) {
                if (errno == ENOENT) {
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            fprintf(stderr, "Failed to process pull action for %s\n", project_name.c_str());
            unlink(temp_file.c_str());
            return false;
        }
    }

    return true;
}

