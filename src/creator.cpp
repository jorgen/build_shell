#include "creator.h"

#include "configuration.h"

#include <sys/types.h>
#include <sys/dir.h>

Creator::Creator(Configuration *configuration)
{
    m_src_dir = configuration->srcDir();
    m_build_set_file_in = configuration->buildsetFile();
}

void Creator::writeNewBuildSet(std::string file)
{
#ifdef _DARWIN_FEATURE_64_BIT_INODE
    fprintf(stderr, "WOOOOOOOOOT\n");
#else
    fprintf(stderr, "BAR\n");
#endif
    DIR *source_dir = opendir(m_src_dir.c_str());
    while (struct dirent *ent = readdir(source_dir)) {

    }

}

