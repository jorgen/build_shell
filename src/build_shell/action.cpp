#include "action.h"

#include "json_tree.h"
#include "tree_writer.h"

Action::Action(const Configuration &configuration)
    : m_configuration(configuration)
    , m_error(false)
{
}

Action::~Action()
{
}

bool Action::error() const
{
    return m_error;
}

bool Action::flushProjectNodeToTemporaryFile(const std::string &project_name, JT::ObjectNode *node, std::string &file_flushed_to)
{
    int temp_file = Configuration::createTempFile(project_name, file_flushed_to);
    if (temp_file < 0) {
        fprintf(stderr, "Could not create temp file for project %s\n", project_name.c_str());
        return false;
    }
    TreeWriter writer(temp_file, node);
    if (writer.error()) {
        fprintf(stderr, "Failed to write project node to temporary file %s\n", file_flushed_to.c_str());
        return false;
    }
    return true;
}
