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

#include "print_environment_action.h"

#include "env_script_builder.h"
#include "buildset_tree_builder.h"

PrintEnvironmentAction::PrintEnvironmentAction(const Configuration &configuration)
    : Action(configuration)
{

}

bool PrintEnvironmentAction::execute()
{
    BuildEnvironment build_environment(m_configuration);
    BuildsetTreeBuilder buildset_tree_builder(build_environment, m_configuration.buildsetFile());
    if (buildset_tree_builder.error()) {
        fprintf(stderr, "Failed to parse buildset file %s\n", m_configuration.buildsetFile().c_str());
        return false;
    }

    JT::ObjectNode *build_set = buildset_tree_builder.treeBuilder.rootNode()->copy();

    if (!build_set) {
        fprintf(stderr, "Print Environment action: Failed to create buildset\n");
        return false;
    }

    EnvScriptBuilder scriptBuilder(m_configuration, build_environment, build_set);
    scriptBuilder.setToProject(m_configuration.buildFromProject());
    scriptBuilder.writeSetScript(stdout);

    return true;
}
