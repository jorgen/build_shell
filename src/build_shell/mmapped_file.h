#ifndef MMAPPED_FILE
#define MMAPPED_FILE

#include <string>

class MmappedReadFile
{
public:
    MmappedReadFile();
    MmappedReadFile(const std::string &file);
    ~MmappedReadFile();

    void setFile(int file);
    void setFile(const std::string &file);

    void *map();
    void *remap();
    void unmap();

    void *data();
    size_t size();

    void setCloseFileOnDestruct(bool close);
private:
    int m_file_to_map = -1;
    void *m_data = 0;
    size_t m_mapped_length = 0;
    bool m_close_file = false;
};

#endif
