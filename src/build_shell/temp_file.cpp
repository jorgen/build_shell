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
#include "temp_file.h"

#include <unistd.h>
#include <string.h>

TempFile::TempFile(const std::string &name_base)
    : m_closed(false)
{
    const char temp_file_prefix[] = "/tmp/build_shell_";
    m_filename.reserve(sizeof temp_file_prefix + name_base.size() + 8);
    m_filename += temp_file_prefix + name_base + "_XXXXXX";

    m_file_descriptor = mkstemp(&m_filename[0]);
}

TempFile::~TempFile()
{
    unlink(m_filename.c_str());
}

const std::string &TempFile::name() const
{
    return m_filename;
}

FILE *TempFile::fileStream(const char *mode)
{
    if (m_closed)
        return 0;

    for (auto it = m_streams.begin(); it != m_streams.end(); ++it) {
        if (strcmp(it->mode, mode))
            return it->file_stream;
    }

    FileStream stream;
    stream.file_stream = fdopen(m_file_descriptor, mode);
    stream.mode = mode;
    m_streams.push_back(stream);
    return stream.file_stream;
}

int TempFile::fileDescriptor() const
{
    return m_file_descriptor;
}

void TempFile::close()
{
    if (!m_closed) {
        for (auto it = m_streams.begin(); it != m_streams.end(); ++it) {
            fclose(it->file_stream);
        }
        ::close(m_file_descriptor);
    }
}
