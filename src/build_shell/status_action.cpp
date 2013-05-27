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

#include "status_action.h"

#include "process.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

StatusAction::StatusAction(const Configuration &configuration)
    : Action(configuration)
    , m_tree_builder(configuration.buildsetFile())
{
   m_buildset_tree = m_tree_builder.rootNode();
   m_error = !m_buildset_tree;
}

bool StatusAction::execute()
{
    for (auto it = startIterator(m_buildset_tree); it != m_buildset_tree->end(); ++it) {
        JT::ObjectNode *project_node = it->second->asObjectNode();
        if (!project_node)
            continue;

        if (chdir(m_configuration.srcDir().c_str())) {
            fprintf(stderr, "Could not move into src dir:%s\n%s\n",
                    m_configuration.srcDir().c_str(), strerror(errno));
            return false;
        }

        const std::string project_name = it->first.string();

        struct stat stat_buffer;
        if (stat(project_name.c_str(), &stat_buffer)) {
            if (errno == ENOENT) {
                fprintf(stderr, "Failed to find source for %s. Cannot state\n", project_name.c_str());
                m_error = true;
                return false;
            }
        }

        if (!S_ISDIR(stat_buffer.st_mode)) {
            fprintf(stderr, "Project %s is not a directory. Cannot state it status\n", 
                    project_name.c_str());
            m_error = true;
            return false;
        }
        if (chdir(project_name.c_str())) {
            fprintf(stderr, "Status Action: Failed to move into directory %s : %s\n",
                    project_name.c_str(), strerror(errno));
            m_error = true;
            return false;
        }
        Configuration::ScmType scm_type = Configuration::findScmForCurrentDirectory();
        std::string fallback = Configuration::ScmTypeStringMap[scm_type];
        Process process(m_configuration);
        process.setPhase("status");
        process.setProjectName(project_name);
        process.setFallback(fallback);
        process.setProjectNode(project_node);
        process.setPrint(true);
        bool success = process.run(nullptr);

        if (!success)
            return false;
    }
    return true;
}
