#include "writer.h"
#include <fcntl.h>
#include <unistd.h>

Writer::Writer(std::string file)
    : m_file(0)
    , m_file_desc(-1)
{
    m_file_desc = ::open(file.c_str(), (O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC));
    if (m_file_desc >= 0) {
        m_file = fdopen(m_file_desc, "w");
        if (!m_file) {
            fprintf(stderr, "Failed to open filestream for file %s\n", file.c_str());
            exit(3);
        }
    } else {
        fprintf(stderr, "Failed to open file %s\n", file.c_str());
        exit(3);
    }
}

Writer::~Writer()
{
    if (m_file) {
        fclose(m_file);
        close(m_file_desc);
    }
}
