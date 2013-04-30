#include "create_action.h"

#include <sys/types.h>
#include <sys/dir.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>

CreateAction::CreateAction(const Configuration &configuration)
    : Action(configuration)
{

}

CreateAction::~CreateAction()
{

}

bool CreateAction::execute()
{
    DIR *source_dir = opendir(m_configuration.srcDir().c_str());
    if (!source_dir) {
        fprintf(stderr, "Failed to open directory: %s\n%s\n",
                m_configuration.srcDir().c_str(), strerror(errno));
        return false;
    }
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    fprintf(stderr, "Current dir: %s\n", cwd);

    if (chdir(m_configuration.srcDir().c_str()) != 0) {
        fprintf(stderr, "Could not change dir: %s\n%s\n",
                m_configuration.srcDir().c_str(),strerror(errno));
        return false;
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
            chdir(m_configuration.srcDir().c_str());
        }
    }

    //this should not fail ;)
    chdir(cwd);
    return true;
}

bool CreateAction::handleCurrentSrcDir()
{
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    if (access(".git", X_OK) == 0) {
        m_configuration.runScript("git.sh", "foobar");
    } else if (access(".svn", X_OK) == 0) {

    } else {

    }
    return true;
}

