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
#include "mmapped_file.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>

#include <errno.h>

MmappedReadFile::MmappedReadFile()
{
}

MmappedReadFile::MmappedReadFile(const std::string &file)
{
    setFile(file);
}

MmappedReadFile::~MmappedReadFile()
{
    unmap();
    setFile(-1);
}

void MmappedReadFile::setFile(int file)
{
    if (m_file_to_map >= 0) {
        unmap();
        if (m_close_file) {
            close(m_file_to_map);
        }
    }

    m_file_to_map = file;
}

void MmappedReadFile::setFile(const std::string &file)
{
    if (!file.size()) {
        setFile(-1);
        m_close_file = false;
        return;
    }

    int file_desc = open(file.c_str(), O_RDONLY | O_CLOEXEC);
    if (file_desc < 0) {
        fprintf(stderr, "Failed to open file for readonly %s\n%s\n",
                file.c_str(), strerror(errno));
    } else {
        setFile(file_desc);
        m_close_file = true;
    }

}

void *MmappedReadFile::map()
{
    if (m_data)
        return m_data;

    if (m_file_to_map < 0)
        return 0;

    struct stat file_stat;
    if (fstat(m_file_to_map, &file_stat) < 0) {
        fprintf(stderr, "Something whent wrong when statting file MMappedReadFile::map\n%s\n",
                strerror(errno));
        return 0;
    }
    m_mapped_length = file_stat.st_size;

    m_data = mmap(0, m_mapped_length, PROT_READ, MAP_SHARED, m_file_to_map, 0);

    return m_data;
}

void *MmappedReadFile::remap()
{
    if (m_data) {
        unmap();
    }

    return map();
}

void MmappedReadFile::unmap()
{
    munmap(m_data, m_mapped_length);
}

void *MmappedReadFile::data()
{
    return m_data;
}

size_t MmappedReadFile::size()
{
    return m_mapped_length;
}

void MmappedReadFile::setCloseFileOnDestruct(bool close)
{
    m_close_file = close;
}
