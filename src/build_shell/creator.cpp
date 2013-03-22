#include "creator.h"

#include "configuration.h"

#include <sys/types.h>
#include <sys/dir.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>

Creator::Creator(Configuration *configuration)
{
    m_src_dir = configuration->srcDir();
    m_build_set_file_in = configuration->buildsetFile();
}

void Creator::writeNewBuildSet(std::string file)
{
    DIR *source_dir = opendir(m_src_dir.c_str());
    if (!source_dir)
        return;
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    fprintf(stderr, "Current dir: %s\n", cwd);

    if (chdir(m_src_dir.c_str()) != 0) {
        fprintf(stderr, "Could not change dir: %s\n", strerror(errno));
        return;
    }

    while (struct dirent *ent = readdir(source_dir)) {
        if (strncmp(".",ent->d_name, sizeof(".")) == 0 ||
            strncmp("..", ent->d_name, sizeof("..")) == 0)
            continue;
        struct stat buf;
        if (stat(ent->d_name, &buf) != 0) {
            fprintf(stderr, "Something whent wrong when stating file %s: %s\n",
                    ent->d_name, strerror(errno));
            continue;
        }
        if (S_ISDIR(buf.st_mode)) {
            chdir(ent->d_name);
            handleCurrentSrcDir();
            chdir(m_src_dir.c_str());
        }
    }

    //this should not fail ;)
    chdir(cwd);

}

void Creator::handleCurrentSrcDir()
{
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    fprintf(stderr, "Handling current source dir %s\n", cwd);

}

