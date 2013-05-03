#include "pull_action.h"

#include "json_tree.h"
#include "tree_writer.h"

#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>


PullAction::PullAction(const Configuration &configuration)
    : Action(configuration)
    , m_buildset_tree_builder(configuration.buildsetFile())
{
    m_buildset_tree = m_buildset_tree_builder.takeRootNode();
    if (!m_buildset_tree) {
        fprintf(stderr, "Error loading buildset %s\n",
                configuration.buildsetFile().c_str());
        m_error = true;
    }
}

PullAction::~PullAction()
{
    delete m_buildset_tree;
}

static void addStringNodeToObject(const std::string &path, const std::string &value, JT::ObjectNode *root) {
    std::vector<std::string> path_vector;

    size_t pos = 0;
    while (pos < path.size()) {
        size_t new_pos = path.find('.', pos);
        path_vector.push_back(path.substr(pos, new_pos - pos));
        pos = new_pos;
        if (new_pos != std::string::npos)
            pos++;
    }

    JT::ObjectNode *last_node = root;
    for (size_t i = 0; i < path_vector.size(); i++) {
        if (i == path_vector.size() -1) {
            JT::Token token;
            token.value.data = value.c_str();
            token.value.size = value.size();
            token.value.temporary = true;
            JT::StringNode *string_node = new JT::StringNode(&token);
            JT::Property prop(JT::Token::String, JT::Data(path_vector[i].c_str(), path_vector[i].size(), true));
            last_node->insertNode(prop,string_node,true);
        }
    }
}

bool stop_propogate(const std::string &, const std::string &input_file)
{
    if (access(input_file.c_str(), F_OK)) {
        if (errno == ENOENT)
            return true;
    }
    return false;
}

bool PullAction::execute()
{
    if (!m_buildset_tree || m_error)
        return false;

    JT::ObjectNode *arguments = new JT::ObjectNode();
    addStringNodeToObject("reset_to_sha", "true", arguments);
    JT::Property arg_prop(JT::Token::String, JT::Data("\"arguments\"", sizeof "\"arguments\"" - 1, true));

    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    for (auto it = m_buildset_tree->begin(); it != m_buildset_tree->end(); ++it) {
        if (chdir(m_configuration.srcDir().c_str())) {
            fprintf(stderr, "Could not move into src dir:%s\n%s\n",
                    m_configuration.srcDir().c_str(), strerror(errno));
            return false;
        }

        JT::ObjectNode *project_node = (*it).second->asObjectNode();
        if (!project_node)
            continue;
        project_node->insertNode(arg_prop,arguments,true);
        std::string temp_file_name;
        const std::string project_name = (*it).first.string();

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

        int temp_file = Configuration::createTempFile((*it).first.string(), temp_file_name);
        if (temp_file < 0) {
            fprintf(stderr, "Could not create temp file for project %s\n", (*it).first.string().c_str());
            return false;
        }
        {
            TreeWriter writer(temp_file, project_node);
            if (writer.error()) {
                fprintf(stderr, "Failed to write project node to temporary file %s\n", temp_file_name.c_str());
                return false;
            }
        }
        //have to remove the project node, so it will not be deleted multiple times
        project_node->take("arguments");

        JT::StringNode *string_node = project_node->stringNodeAt("scm.type");
        std::string mode = should_clone? "clone_" : "pull_";
        std::string primary_script = mode;
        primary_script.append(project_name);

        std::string fallback_script = mode;
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
            int exit_code = m_configuration.runScript(*it, temp_file_name.c_str());
            if (exit_code) {
                fprintf(stderr, "Error while running script %s\n", it->c_str());
                return false;
            }

            if (access(temp_file_name.c_str(), F_OK)) {
                if (errno == ENOENT) {
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            fprintf(stderr, "Failed to process pull action for %s\n", project_name.c_str());
            unlink(temp_file_name.c_str());
            return false;
        }
    }

    return true;
}

