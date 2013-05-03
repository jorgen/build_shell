#include "tree_writer.h"

#include <unistd.h>
#include <string.h>
#include <errno.h>

TreeWriter::TreeWriter(int file, JT::ObjectNode *root, bool closeFileOnDestruct)
    : m_file(file)
    , m_error(false)
    , m_close_file(closeFileOnDestruct)
{
    std::function<void(JT::Serializer *)> callback =
        std::bind(&TreeWriter::requestFlush, this, std::placeholders::_1);

    JT::TreeSerializer serializer;
    JT::SerializerOptions options = serializer.options();
    options.setPretty(true);
    serializer.setOptions(options);
    serializer.addRequestBufferCallback(callback);
    m_error = !serializer.serialize(root);

    if (m_error)
        return;

    if (serializer.buffers().size()) {
        JT::SerializerBuffer serializer_buffer = serializer.buffers().front();
        writeBufferToFile(serializer_buffer);
    }

    if (m_error)
        return;

    if (write(file,"\n",1) != 1) {
        fprintf(stderr, "Failed to write newline to file\n");
        m_error = true;
    }
}

TreeWriter::~TreeWriter()
{
    if (m_close_file)
        close(m_file);
}

bool TreeWriter::error() const { return m_error; }

void TreeWriter::writeBufferToFile(const JT::SerializerBuffer &buffer)
{
    if (m_error)
        return;
    size_t written = write(m_file,buffer.buffer,buffer.used);
    if (written < buffer.used) {
        fprintf(stderr, "Error while writing to outbuffer: %s\n", strerror(errno));
        m_error = true;
    }
}

void TreeWriter::requestFlush(JT::Serializer *serializer)
{
    if (m_error)
        return;
    if (serializer->buffers().size()) {
        JT::SerializerBuffer serializer_buffer = serializer->buffers().front();
        serializer->clearBuffers();

        writeBufferToFile(serializer_buffer);
    }
    serializer->appendBuffer(m_buffer,sizeof m_buffer);
}
