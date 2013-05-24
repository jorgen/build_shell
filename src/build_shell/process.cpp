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

#include "process.h"

#include "tree_writer.h"
#include "tree_builder.h"

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

bool Process::flushProjectNodeToTemporaryFile(const std::string &project_name, JT::ObjectNode *node, std::string &file_flushed_to)
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

Process::Process(const Configuration &configuration)
    : m_configuration(configuration)
    , m_log_file(-1)
    , m_close_log_file(false)
    , m_project_node(0)
{
}

Process::~Process()
{
    if (m_close_log_file && m_log_file >= 0) {
        close(m_log_file);
    }
}

bool Process::run(JT::ObjectNode **returnedObjectNode)
{
    *returnedObjectNode = 0;

    if (!m_project_name.size() || !m_phase.size()) {
        fprintf(stderr, "project name  or phase is empty when executing command: %s : %s\n",
                m_project_name.c_str(), m_phase.c_str());
        return false;
    }

    bool return_val = true;

    if (m_log_file < 0) {
        std::string log_file_str = m_configuration.scriptExecutionLogPath() +  "/" + m_project_name + "_" + m_phase + ".log";
        setLogFile(log_file_str, false);
    }

    std::string temp_file;

    if (!flushProjectNodeToTemporaryFile(m_project_name, m_project_node, temp_file))
        return false;
    std::string primary_script = m_phase + "_" + m_project_name;
    std::string fallback_script = m_phase + "_" + m_fallback;

    auto scripts = m_configuration.findScript(primary_script, fallback_script);
    bool temp_file_removed = false;

    for (auto it = scripts.begin(); it != scripts.end(); ++it) {
        int exit_code = m_configuration.runScript(m_environement_script, (*it), temp_file, m_log_file);
        if (exit_code) {
            fprintf(stderr, "Script %s for project %s failed in execution\n", it->c_str(), m_project_name.c_str());
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

void Process::setEnvironmentScript(const std::string &environment_script)
{
    m_environement_script = environment_script;
}

void Process::setPhase(const std::string &phase)
{
    m_phase = phase;
}

void Process::setProjectName(const std::string &projectName)
{
    m_project_name = projectName;
}

void Process::setFallback(const std::string &fallback)
{
    m_fallback = fallback;
}

void Process::setLogFile(int logFile, bool closeFileOnDelete)
{
    if (m_close_log_file && m_log_file >= 0) {
        close(m_log_file);
    }
    m_log_file = logFile;
    m_log_file_str = std::string();
    m_close_log_file = closeFileOnDelete;
}

void Process::setLogFile(const std::string &logFile, bool append, bool closeFileOnDelete)
{
    if (m_close_log_file) {
        if (m_log_file >= 0) {
            close(m_log_file);
        }
    }
    m_log_file_str = logFile;
    m_close_log_file = closeFileOnDelete;

    int flags = O_WRONLY|O_CREAT|O_CLOEXEC;
    if (append) {
        flags = flags | O_APPEND;
    } else {
        flags = flags | O_TRUNC;
    }

    if (access(m_log_file_str.c_str(), F_OK) == 0) {
        m_log_file = open(m_log_file_str.c_str(), flags);
    } else {
        mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH;
        m_log_file = open(m_log_file_str.c_str(), flags, mode);
    }
    if (m_log_file < 0) {
        fprintf(stderr, "Failed to open log file %s : %s", m_log_file_str.c_str(),
                strerror(errno));
    }
}

void Process::setProjectNode(JT::ObjectNode *projectNode)
{
    m_project_node = projectNode;
}

