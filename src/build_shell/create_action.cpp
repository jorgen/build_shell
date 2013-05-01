#include "create_action.h"

#include <sys/types.h>
#include <sys/dir.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "json_tree.h"

CreateAction::CreateAction(const Configuration &configuration)
    : Action(configuration)
{
    fprintf(stderr, "FEOWKFFWEKO");
    int m_buildset_file = open(m_configuration.buildsetFile().c_str(), O_RDONLY | O_CLOEXEC);
    if (m_buildset_file < 0) {
        fprintf(stderr, "Failed to open buildset file %s\n%s\n",
                m_configuration.buildsetFile().c_str(), strerror(errno));
    }

    struct stat buildset_file_stat;
    if (fstat (m_buildset_file ,&buildset_file_stat) < 0)
        fprintf(stderr, "Something whent wrong when getting file length for file %s\n%s\n",
                m_configuration.buildsetFile().c_str(), strerror(errno));
    void *buildset_file_data = mmap(0, buildset_file_stat.st_size, PROT_READ, MAP_SHARED, m_buildset_file, 0);

    JT::TreeBuilder tree_builder;
    tree_builder.create_root_if_needed = true;
    auto tree_build = tree_builder.build((char *)buildset_file_data,buildset_file_stat.st_size);

    if (tree_build.first) {
        m_out_tree = tree_build.first->asObjectNode();
    } else {
        m_out_tree = new JT::ObjectNode();
    }
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

    std::string tmp_file;
    int tmp_file_desc = Configuration::createTempFileFromCWD(tmp_file);

    int exit_code = 0;
    if (access(".git", X_OK) == 0) {
        exit_code = m_configuration.runScript("git_create.sh", tmp_file.c_str());
    } else if (access(".svn", X_OK) == 0) {

    } else {

    }

    close(tmp_file_desc);
    unlink(tmp_file.c_str());
    return exit_code == 0;
}

