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
#include "tree_writer.h"

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

TreeWriter::TreeWriter(const std::string &file)
    : m_error(false)
    , m_close_file(true)
{
    mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH;
    m_file = open(file.c_str(), O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC, mode);
    if (m_file < 0) {
        m_error = true;
        fprintf(stderr, "Failed to open file %s : %s\n", file.c_str(), strerror(errno));
        return;
    }
}
TreeWriter::TreeWriter(int file, bool closeFileOnDestruct)
    : m_file(file)
    , m_error(false)
    , m_close_file(closeFileOnDestruct)
{
}

TreeWriter::~TreeWriter()
{
    if (m_close_file)
        close(m_file);
}

bool TreeWriter::error() const { return m_error; }

void TreeWriter:: registerTokenTransformer(std::function<const JT::Token&(const JT::Token &)> token_transformer)
{
    m_token_transformer = token_transformer;
}
void TreeWriter::write(const JT::ObjectNode *root)
{
    if (m_error) {
        fprintf(stderr, "Error when writing node to file\n");
        return;
    }

    std::function<void(JT::Serializer *)> callback =
        std::bind(&TreeWriter::requestFlush, this, std::placeholders::_1);

    JT::TreeSerializer serializer;
    serializer.registerTokenTransformer(m_token_transformer);
    JT::SerializerOptions options = serializer.options();
    options.setPretty(true);
    serializer.setOptions(options);
    serializer.addRequestBufferCallback(callback);
    m_error = !serializer.serialize(root);

    if (m_error) {
        fprintf(stderr, "Failed to serialize token\n");
        return;
    }

    if (serializer.buffers().size()) {
        JT::SerializerBuffer serializer_buffer = serializer.buffers().front();
        writeBufferToFile(serializer_buffer);
    }

    if (m_error) {
        fprintf(stderr, "Failed to serialize token\n");
        return;
    }

    if (::write(m_file,"\n",1) != 1) {
        fprintf(stderr, "Failed to write newline to file\n");
        m_error = true;
    }
}

void TreeWriter::setSerializeTransformer(std::function<const JT::Token&(const JT::Token &)> transformer)
{
    m_token_transformer = transformer;
}

void TreeWriter::writeBufferToFile(const JT::SerializerBuffer &buffer)
{
    if (m_error)
        return;
    size_t written = ::write(m_file,buffer.buffer,buffer.used);
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
