#ifndef WRITER_H
#define WRITER_H

#include <string>
#include <cstdio>

class Writer
{
public:
    explicit Writer(std::string file);
    ~Writer();

private:
    FILE *m_file;
    int m_file_desc;
};

#endif //WRITER_H
