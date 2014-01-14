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

CreateAction::CreateAction(const Configuration &configuration, bool allowMissingVariables)
    : Action(configuration)
    , m_build_environment(configuration)
{

    BuildsetTreeBuilder buildset_tree_builder(m_build_environment, configuration.buildsetFile(), true, allowMissingVariables);
    if (buildset_tree_builder.error()) {
        m_error = true;
        return;
    }

    m_buildset_tree.reset(buildset_tree_builder.treeBuilder.takeRootNode());

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

    if (m_configuration.registerBuild()) {
        AvailableBuilds available_builds(m_configuration);
        available_builds.addAvailableBuild(m_configuration.buildDir(), m_configuration.buildShellSetEnvFile());
    }
}

CreateAction::~CreateAction()
{
    if (m_error)
        return;

    EnvScriptBuilder env_script_builder(m_configuration, m_build_environment, m_buildset_tree.get());
    env_script_builder.writeScripts(m_configuration.buildShellSetEnvFile(), m_configuration.buildShellUnsetEnvFile());

    std::string current_buildset_name = m_configuration.buildDir() + "/build_shell/current_buildset";
    GenerateAction generate(m_configuration, m_buildset_tree.get(), current_buildset_name);
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

