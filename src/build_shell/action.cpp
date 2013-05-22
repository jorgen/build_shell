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
#include "action.h"

#include "json_tree.h"
#include "tree_writer.h"
#include "tree_builder.h"

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

Action::Action(const Configuration &configuration)
    : m_configuration(configuration)
    , m_error(false)
{
}

Action::~Action()
{
}

bool Action::error() const
{
    return m_error;
}

bool Action::flushProjectNodeToTemporaryFile(const std::string &project_name, JT::ObjectNode *node, std::string &file_flushed_to)
{
    int temp_file = Configuration::createTempFile(project_name, file_flushed_to);
    if (temp_file < 0) {
        fprintf(stderr, "Could not create temp file for project %s\n", project_name.c_str());
        return false;
    }
    TreeWriter writer(temp_file, node);
    if (writer.error()) {
        fprintf(stderr, "Failed to write project node to temporary file %s\n", file_flushed_to.c_str());
        return false;
    }
    return true;
}

bool Action::executeScript(const std::string &step,
                           const std::string &projectName,
                           const std::string &fallback,
                           JT::ObjectNode *projectNode,
                           JT::ObjectNode **returnedObjectNode)
{
    return executeScript("",step, projectName,fallback, projectNode, returnedObjectNode);
}
bool Action::executeScript(const std::string &env_script,
                           const std::string &step,
                           const std::string &projectName,
                           const std::string &fallback,
                           JT::ObjectNode *projectNode,
                           JT::ObjectNode **returnedObjectNode)
{
    std::string log_file_str = m_configuration.scriptExecutionLogPath() +  "/" + projectName + "_" + step + ".log";
    mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH;
    int log_file = open(log_file_str.c_str(), O_WRONLY|O_APPEND|O_CREAT|O_TRUNC|O_CLOEXEC, mode);
    if (log_file < 0) {
        fprintf(stderr, "Failed to open file %s for stderr redirection %s\n", log_file_str.c_str(),
                strerror(errno));
        return false;
    }

    bool returnVal = executeScript(env_script, step, projectName, fallback, log_file, projectNode, returnedObjectNode);

    close(log_file);

    return returnVal;
}

bool Action::executeScript(const std::string &env_script,
                           const std::string &step,
                           const std::string &projectName,
                           const std::string &fallback,
                           int log_file,
                           JT::ObjectNode *projectNode,
                           JT::ObjectNode **returnedObjectNode)
{
    bool return_val = true;
    *returnedObjectNode = 0;
    std::string temp_file;
    if (!flushProjectNodeToTemporaryFile(projectName, projectNode, temp_file))
        return false;
    std::string primary_script = step + "_" + projectName;
    std::string fallback_script = step + "_" + fallback;
    auto scripts = m_configuration.findScript(primary_script, fallback_script);
    bool temp_file_removed = false;
    for (auto it = scripts.begin(); it != scripts.end(); ++it) {
        int exit_code = m_configuration.runScript(env_script, (*it), temp_file, log_file);
        if (exit_code) {
            fprintf(stderr, "Script %s for project %s failed in execution\n", it->c_str(), projectName.c_str());
            return_val = false;
            break;
        }
        if (access(temp_file.c_str(), F_OK)) {
            temp_file_removed = true;
            fprintf(stderr, "The script removed the temporary input file, assuming failur\n");
            return_val = false;
            break;
        }
        TreeBuilder tree_builder(temp_file);
        JT::ObjectNode *root = tree_builder.rootNode();
        if (root) {
            if (root->booleanAt("arguments.propogate_to_next_script"))
                continue;
            *returnedObjectNode = tree_builder.takeRootNode();
        } else  {
            fprintf(stderr, "Failed to demarshal the temporary file returned from the script %s\n", (*it).c_str());
            return_val = false;
        }
        break;
    }
    if (!temp_file_removed) {
        unlink(temp_file.c_str());
    }
    return return_val;
}
