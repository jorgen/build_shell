#include "tree_builder.h"

#include "json_tree.h"

TreeBuilder::TreeBuilder(const std::string &file)
    : m_node(0)
    , m_file_name(file)
    , m_mapped_file(m_file_name)
{
    if (!m_file_name.size()) {
        return;
    }

    const char *data = static_cast<const char *>(m_mapped_file.map());
    if (!data)
        return;
    JT::TreeBuilder tree_builder;
    tree_builder.create_root_if_needed = true;
    auto tree_build = tree_builder.build(data, m_mapped_file.size());
    if (tree_build.second == JT::Error::NoError) {
        m_node = tree_build.first->asObjectNode();
    }

    if (!m_node)
        delete tree_build.first;
}

TreeBuilder::~TreeBuilder()
{
    delete m_node;
}

JT::ObjectNode *TreeBuilder::rootNode() const
{
    return m_node;
}

JT::ObjectNode *TreeBuilder::takeRootNode()
{
    JT::ObjectNode *retNode = m_node;
    m_node = 0;
    return retNode;
}
