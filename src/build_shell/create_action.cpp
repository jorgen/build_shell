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
#include "generate_action.h"

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

CreateAction::CreateAction(const Configuration &configuration, bool allowMissingVariables)
    : Action(configuration)
    , m_build_environment(configuration)
    , m_buildset_tree_builder(m_build_environment, configuration.buildsetFile())
    , m_buildset_tree(0)
{
    auto missing_variables = m_buildset_tree_builder.missingVariables();
    if (missing_variables.size()) {
        fprintf(stderr, "Buildset is missing variables:\n");
        for (auto it = missing_variables.begin(); it != missing_variables.end(); ++it) {
            fprintf(stderr, "\t - %s\n", it->c_str());
        }
        fprintf(stderr, "\nPlease use bs_variable to add variables to your buildset environment:\n");
        fprintf(stderr, "\t bs_variable \"some_variable\" \"some_value\"\n");
        fprintf(stderr, "or if you need to limit scope of the variable\n");
        fprintf(stderr, "\t bs_variable \"some_project\" \"some_variable\" \"some_value\"\n\n");
        if (!allowMissingVariables) {
            fprintf(stderr, "Build set mode: %s does not allow to proceed with undefined variablees\n\n", m_configuration.modeString().c_str());
            m_error = true;
            return;
        }
    }
    if (! m_buildset_tree_builder.error()) {
        m_buildset_tree = m_buildset_tree_builder.treeBuilder.rootNode();
    }
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

    EnvScriptBuilder env_script_builder(m_configuration, m_build_environment, m_buildset_tree);
    env_script_builder.writeScripts(m_set_build_env_file, m_unset_build_env_file);

    std::string current_buildset_name = m_configuration.buildDir() + "/build_shell/current_buildset";
    GenerateAction generate(m_configuration, m_buildset_tree, current_buildset_name);
    bool success = generate.execute();
    if (!success || generate.error()) {
        fprintf(stderr, "Failed to write current buildset to file %s\n", current_buildset_name.c_str());
        return;
    }
}

bool CreateAction::execute()
{
    return !m_error;
}

