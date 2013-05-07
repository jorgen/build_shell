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
#include <libgen.h>

#include "json_tree.h"
#include "tree_writer.h"

CreateAction::CreateAction(const Configuration &configuration,
        const std::string &outfile)
    : Action(configuration)
    , m_tree_builder(configuration.buildsetFile())
{
    m_out_tree.reset(m_tree_builder.takeRootNode());

    if (!m_out_tree.get())
        m_out_tree.reset(new JT::ObjectNode());

    const std::string *actual_outfile = &outfile;
    if (!actual_outfile->size())
        actual_outfile = &configuration.buildsetOutFile();

    if (actual_outfile->size()) {
        m_out_file_name = *actual_outfile;
        mode_t create_mode = S_IROTH | S_IRGRP | S_IRUSR | S_IWUSR;
        m_out_file = open(m_out_file_name.c_str(), O_RDWR | O_CLOEXEC | O_CREAT | O_EXCL, create_mode);
        if (m_out_file < 0) {
            fprintf(stderr, "Failed to open buildset Out File %s\n%s\n",
                    m_out_file_name.c_str(), strerror(errno));
            m_error = true;
        }
    } else {
        m_out_file = STDOUT_FILENO;
    }
}

CreateAction::~CreateAction()
{
    if (m_out_file_name.size())
        close(m_out_file);
}


bool CreateAction::execute()
{
    if (m_error)
        return false;

    DIR *source_dir = opendir(m_configuration.srcDir().c_str());
    if (!source_dir) {
        fprintf(stderr, "Failed to open directory: %s\n%s\n",
                m_configuration.srcDir().c_str(), strerror(errno));
        return false;
    }
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

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
            if (chdir(ent->d_name)) {
                fprintf(stderr, "Failed to change into directory %s\n%s\n", ent->d_name, strerror(errno));
                return false;
            }
            if (!handleCurrentSrcDir()) {
                fprintf(stderr, "Failed to handle dir %s\n", ent->d_name);
                closedir(source_dir);
                return false;
            }
            chdir(m_configuration.srcDir().c_str());
        }
    }
    closedir(source_dir);

    //this should not fail ;)
    chdir(cwd);

    TreeWriter tree_writer(m_out_file, m_out_tree.get());
    return !tree_writer.error();
}


bool CreateAction::handleCurrentSrcDir()
{
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    std::string base_name = basename(cwd);

    JT::ObjectNode *root_for_dir = m_out_tree->objectNodeAt(base_name);
    if (!root_for_dir) {
        root_for_dir = new JT::ObjectNode();
    }

    JT::ObjectNode *scm_node = root_for_dir->objectNodeAt("scm");
    if (!scm_node) {
        JT::Property prop(JT::Token::String, JT::Data("scm",3,false));
        root_for_dir->insertNode(prop, new JT::ObjectNode(), true);
    }

    std::string primary = "state_";
    primary.append(base_name);
    std::string fallback = "state_";
    std::string postfix;

    if (access(".git", F_OK) == 0) {
        postfix = "git";
    } else if (access(".svn", F_OK) == 0) {
        postfix = "regular_dir";
    } else {
        postfix = "regular_dir";
    }
    fallback.append(postfix);


    auto scripts = m_configuration.findScript(primary, fallback);
    if (scripts.size() == 0) {
        fprintf(stderr, "Could not find any scripts matching primary: %s nor fallback: %s\n",
                primary.c_str(), fallback.c_str());
        return false;
    }
    bool script_success = false;
    std::string final_temp_file;
    for (auto it = scripts.begin(); it != scripts.end(); ++it) {
        std::string tmp_file;
        flushProjectNodeToTemporaryFile(base_name, root_for_dir, tmp_file);

        int exit_code = m_configuration.runScript("", *it, tmp_file);

        if (exit_code) {
            return true;
        }

        if (access(tmp_file.c_str(), F_OK) == 0) {
            script_success = true;
            final_temp_file = tmp_file;
            break;
        }
    }

    if (!script_success) {
        return false;
    }

    int final_temp_file_desc = open(final_temp_file.c_str(), O_RDONLY|O_CLOEXEC);
    if (final_temp_file_desc < 0) {
        fprintf(stderr, "Suspecting script for messing up temporary file: %s\n%s\n",
                final_temp_file.c_str(), strerror(errno));
        return false;
    }
    MmappedReadFile temp_mmap_file_handler;
    temp_mmap_file_handler.setFile(final_temp_file_desc);

    const char *temp_file_data = static_cast<const char *>(temp_mmap_file_handler.map());

    JT::TreeBuilder tree_builder;
    tree_builder.create_root_if_needed = true;
    auto tree_build = tree_builder.build(temp_file_data, temp_mmap_file_handler.size());
    if (tree_build.second == JT::Error::NoError) {
        JT::Property property(base_name);
        JT::ObjectNode *root_for_cwd_dir = tree_build.first->asObjectNode();
        m_out_tree->insertNode(property, root_for_cwd_dir, true);
    } else {
        fprintf(stderr, "Failed to understand output from %s\n", base_name.c_str());
    }
    close(final_temp_file_desc);
    unlink(final_temp_file.c_str());
    return true;
}

