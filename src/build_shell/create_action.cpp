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

#include "available_builds.h"
#include "tree_writer.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

static bool ensureFileExist(const std::string &file)
{
    if (access(file.c_str(), F_OK|W_OK)) {
        mode_t create_mode = S_IRWXU | S_IRWXG;
        int file_desc = open(file.c_str(), O_RDWR | O_CLOEXEC | O_CREAT | O_EXCL, create_mode);
        if (file_desc < 0) {
            fprintf(stderr, "Failed to create file: %s\n%s\n", file.c_str(), strerror(errno));
            return false;
        }
    }
    return true;
}

CreateAction::CreateAction(const Configuration &configuration)
    : Action(configuration)
    , m_buildset_tree_builder(configuration, configuration.buildsetFile())
    , m_env_script_builder(configuration, m_buildset_tree_builder.treeBuilder.rootNode())
{
    m_buildset_tree = m_buildset_tree_builder.treeBuilder.rootNode();
    if (!m_buildset_tree) {
        fprintf(stderr, "Error loading buildset %s\n",
                configuration.buildsetFile().c_str());
        m_error = true;
        return;
    }

    if (chdir(m_configuration.buildDir().c_str())) {
        fprintf(stderr, "Could not move into build dir:%s\n%s\n",
                m_configuration.buildDir().c_str(), strerror(errno));
        m_error = true;
        return;
    }

    std::string build_shell_meta_dir;
    if (!Configuration::getAbsPath("build_shell", true, build_shell_meta_dir)) {
        fprintf(stderr, "Failed to get build_shell meta dir\n");
        m_error = true;
        return;
    }

    m_set_build_env_file = build_shell_meta_dir + "/" + "set_build_env.sh";
    m_unset_build_env_file = build_shell_meta_dir + "/" + "unset_build_env.sh";

    if (m_configuration.registerBuild()) {
        AvailableBuilds available_builds(m_configuration);
        available_builds.addAvailableBuild(m_configuration.buildDir(), m_set_build_env_file.c_str());
    }

    m_error = !ensureFileExist(m_set_build_env_file);
    if (m_error)
        return;
    m_error = !ensureFileExist(m_unset_build_env_file);
    if (m_error)
        return;
}

CreateAction::~CreateAction()
{
    if (m_error)
        return;

    m_env_script_builder.writeScripts(m_set_build_env_file, m_unset_build_env_file,"");

    std::string current_buildset_name = m_configuration.buildDir() + "/build_shell/current_buildset";
    TreeWriter current_tree_writer(current_buildset_name);
    current_tree_writer.write(m_buildset_tree);
    if (current_tree_writer.error()) {
        fprintf(stderr, "Failed to write current buildset to file %s\n", current_buildset_name.c_str());
        return;
    }
}

bool CreateAction::execute()
{
    return !m_error;
}

