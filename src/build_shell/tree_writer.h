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
#ifndef TREE_WRITER_H
#define TREE_WRITER_H

#include "json_tree.h"

class TreeWriter
{
public:
    TreeWriter(const std::string &file);
    TreeWriter(int file, bool closeFileOnDestruct = false);

    void write(JT::ObjectNode *root);

    void setSerializeTransformer(std::function<const JT::Token&(const JT::Token &)> transformer);

    ~TreeWriter();

    bool error() const;

private:
    void writeBufferToFile(const JT::SerializerBuffer &buffer);
    void requestFlush(JT::Serializer *serializer);

    int m_file;
    char m_buffer[4096];
    bool m_error;
    bool m_close_file;

    std::function<const JT::Token &(const JT::Token &)> m_transformer;
};

#endif
