#include "build_action.h"

#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

BuildAction::BuildAction(const Configuration &configuration)
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

BuildAction::~BuildAction()
{
}

bool BuildAction::execute()
{
    if (!m_buildset_tree || m_error)
        return false;

    for (auto it = m_buildset_tree->begin(); it != m_buildset_tree->end(); ++it) {
        JT::ObjectNode *project_node = it->second->asObjectNode();
        if (!project_node)
            continue;

        if (chdir(m_configuration.buildDir().c_str())) {
            fprintf(stderr, "Could not move into src dir:%s\n%s\n",
                    m_configuration.srcDir().c_str(), strerror(errno));
            return false;
        }

        const std::string project_name = it->first.string();
        std::string project_build_path = m_configuration.buildDir() + "/" + project_name;
        std::string project_src_path = m_configuration.srcDir() + "/" + project_name;

        if (access(project_src_path.c_str(), X_OK|R_OK)) {
            fprintf(stderr, "Problem accessing source path: %s for project %s. Can not complete build\n",
                    project_src_path.c_str(), project_name.c_str());
            return false;
        }

        if (access(project_build_path.c_str(), X_OK|R_OK)) {
            bool failed_mkdir = true;
            if (errno == ENOENT) {
                if (!mkdir(project_build_path.c_str(),S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)) {
                    failed_mkdir = false;
                }
            }
                if (failed_mkdir) {
                    fprintf(stderr, "Failed to verify build path %s for project %s. Can not complete build\n",
                            project_build_path.c_str(), project_name.c_str());
                    return false;
                }
        }

        JT::ObjectNode *arguments = new JT::ObjectNode();
        project_node->insertNode(std::string("arguments"), arguments, true);
        arguments->addValueToObject("src_path", project_src_path, JT::Token::String);
        arguments->addValueToObject("build_path", project_build_path, JT::Token::String);

        chdir(project_build_path.c_str());

        std::string temp_file;
        flushProjectNodeToTemporaryFile(project_name, project_node, temp_file);

    }

    return true;
}
