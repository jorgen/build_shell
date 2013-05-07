#ifndef TREE_BUILDER_H
#define TREE_BUILDER_H

#include <string>

#include "mmapped_file.h"

namespace JT {
    class ObjectNode;
}

class TreeBuilder
{
public:
    TreeBuilder(const std::string &file);
    ~TreeBuilder();

    JT::ObjectNode *rootNode() const;
    JT::ObjectNode *takeRootNode();

private:
    JT::ObjectNode *m_node;
    const std::string &m_file_name;
    MmappedReadFile m_mapped_file;

};

#endif //TREE_BUILDER_H
