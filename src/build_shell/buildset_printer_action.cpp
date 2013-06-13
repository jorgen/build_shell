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

#include "buildset_printer_action.h"

#include "buildset_tree_builder.h"
#include "buildset_tree_writer.h"

#include <unistd.h>

BuildsetPrinterAction::BuildsetPrinterAction(const Configuration &configuration)
    : Action(configuration)
    , m_build_environment(configuration)
{

}

bool BuildsetPrinterAction::execute()
{
    fprintf(stderr, "Printing\n");
    BuildsetTreeBuilder buildset_tree_builder(m_build_environment, m_configuration.buildsetFile());
    JT::ObjectNode *build_set = buildset_tree_builder.treeBuilder.rootNode();

    fprintf(stderr, "Printing 2\n");
    auto end_it = endIterator(build_set);
    for (auto it = startIterator(build_set); it != end_it; ++it) {
        JT::ObjectNode *project_node = it->second->asObjectNode();
        if (!project_node)
            continue;
        const std::string project_name = it->first.string();
        fprintf(stdout, "\nComponent: %s\n", project_name.c_str());
        BuildsetTreeWriter writer(m_build_environment, project_name,STDOUT_FILENO);
        writer.write(project_node);
        if (writer.error()) {
            fprintf(stderr, "Failed when writing tree for project: %s\n", project_name.c_str());
            return false;
        }
    }
    return true;
}

