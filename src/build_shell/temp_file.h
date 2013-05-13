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
#ifndef TEMP_FILE_H
#define TEMP_FILE_H

#include <stdio.h>

#include <string>
#include <list>

class FileStream
{
public:
    FileStream()
        : file_stream(0)
        , mode("\0")
    { }

    FILE *file_stream;
    const char *mode;
};

class TempFile
{
public:
    TempFile(const std::string &name_base);
    ~TempFile();

    const std::string &name() const;
    FILE *fileStream(const char *mode);
    int fileDescriptor() const;

    void close();
    bool closed() const { return m_closed; }

private:
    std::string m_filename;
    bool m_closed;

    std::list<FileStream> m_streams;

    int m_file_descriptor;
};

#endif
