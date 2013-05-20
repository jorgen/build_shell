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
        JT::Property prop(std::string("scm"));
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

    JT::ObjectNode *updated_node;
    bool script_success = executeScript("", "state", primary, postfix, root_for_dir, &updated_node);

    if (!updated_node) {
        updated_node = new JT::ObjectNode();
    }

    m_out_tree->insertNode(base_name, updated_node, true);

    return script_success;
}

