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
