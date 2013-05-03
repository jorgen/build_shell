#ifndef TREE_WRITER_H
#define TREE_WRITER_H

#include "json_tree.h"

class TreeWriter
{
public:
    TreeWriter(int file, JT::ObjectNode *root, bool closeFileOnDestruct = false);

    ~TreeWriter();

    bool error() const;

    void writeBufferToFile(const JT::SerializerBuffer &buffer);
    void requestFlush(JT::Serializer *serializer);

private:
    int m_file;
    char m_buffer[4096];
    bool m_error = false;
    bool m_close_file;
};

#endif
