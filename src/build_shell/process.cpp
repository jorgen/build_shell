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

#include "buildset_tree_writer.h"
#include "tree_builder.h"
#include "child_process_io_handler.h"

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>

#include <assert.h>

static bool DEBUG_EXEC_SCRIPT = getenv("BUILD_SHELL_DEBUG_EXEC_SCRIPT") != 0;

Process::Process(const Configuration &configuration)
    : m_configuration(configuration)
    , m_build_environment(0)
    , m_log_file(-1)
    , m_close_log_file(false)
    , m_print(false)
    , m_script_has_to_exist(true)
    , m_project_node(0)
{
}

Process::~Process()
{
    if (m_close_log_file && m_log_file >= 0) {
        close(m_log_file);
    }
}

class LogFileResetter
{
public:
    LogFileResetter(Process &process)
        : process(process)
        , resetLog(false)
    {

    }

    ~LogFileResetter()
    {
        if(resetLog)
            process.setLogFile("");
    }
    Process &process;
    bool resetLog;
};

bool Process::run(JT::ObjectNode **returnedObjectNode)
{
    if (returnedObjectNode)
        *returnedObjectNode = 0;

    if (!m_project_name.size() || !m_phase.size()) {
        fprintf(stderr, "project name  or phase is empty when executing command: %s : %s\n",
                m_project_name.c_str(), m_phase.c_str());
        return false;
    }

    bool return_val = true;

    LogFileResetter resetter(*this);
    if (m_log_file < 0 && Configuration::isDir(m_configuration.buildShellMetaDir())) {
        Configuration::ensurePath(m_configuration.scriptExecutionLogDir());
        std::string log_file_str = m_configuration.scriptExecutionLogDir() +  "/" + m_project_name + "_" + m_phase + ".log";
        setLogFile(log_file_str, false);
        resetter.resetLog = true;
    }

    std::string temp_file;

    if (!flushProjectNodeToTemporaryFile(m_project_name, m_project_node, temp_file))
        return false;
    std::string primary_script = m_phase + "_" + m_project_name;
    std::string fallback_script = m_fallback.size() ? m_phase + "_" + m_fallback : "";

    auto scripts = m_configuration.findScript(primary_script, fallback_script);

    if (m_script_has_to_exist && scripts.size() == 0) {
        fprintf(stderr, "Could not find mandatory script for: \"%s\" with fallback \"%s\"\n",
                primary_script.c_str(), fallback_script.c_str());
        return false;
    }

    bool temp_file_removed = false;

    std::string arguments =  m_project_name + " " + temp_file;
    for (auto it = scripts.begin(); it != scripts.end(); ++it) {
        int exit_code = runScript(m_environement_script, (*it), arguments,  m_log_file);
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
        if (returnedObjectNode) {
            TreeBuilder tree_builder(temp_file);
            tree_builder.load();
            JT::ObjectNode *root = tree_builder.rootNode();
            if (root) {
                if (root->booleanAt("arguments.propogate_to_next_script"))
                    continue;
                *returnedObjectNode = tree_builder.takeRootNode();
            } else  {
                fprintf(stderr, "Failed to demarshal the temporary file returned from the script %s\n", (*it).c_str());
                return_val = false;
            }
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
    if (m_close_log_file && m_log_file >= 0) {
        close(m_log_file);
    }

    m_log_file_str = logFile;
    if (!m_log_file_str.size()) {
        m_close_log_file = false;
        return;
    }

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

void Process::setProjectNode(JT::ObjectNode *projectNode, BuildEnvironment *buildEnv)
{
    m_build_environment = buildEnv;
    m_project_node = projectNode;
}

void Process::setPrint(bool print)
{
    m_print = print;
}

void Process::setScriptHasToExist(bool exist)
{
    m_script_has_to_exist = exist;
}

bool Process::flushProjectNodeToTemporaryFile(const std::string &project_name, const JT::ObjectNode *node, std::string &file_flushed_to) const
{
    int temp_file = m_configuration.createTempFile(project_name, file_flushed_to);
    if (temp_file < 0) {
        fprintf(stderr, "Could not create temp file for project %s\n", project_name.c_str());
        return false;
    }
    TreeWriter *writer;
    if (m_build_environment) {
        writer = new BuildsetTreeWriter(*m_build_environment, m_project_name, temp_file);
    } else {
        writer = new TreeWriter(temp_file);
    }
    writer->write(node);
    if (writer->error()) {
        fprintf(stderr, "Failed to write project node to temporary file %s\n", file_flushed_to.c_str());
        return false;
    }
    delete writer;
    return true;
}

int Process::runScript(const std::string &env_script,
                       const std::string &script,
                       const std::string &args,
                       int redirect_out_to) const
{
    if (!script.size())
        return -1;

    std::string pre_script_command;
    if (env_script.size()) {
        pre_script_command += std::string("source ") + env_script + " && ";
    }
    std::string env_file = m_configuration.findBuildEnvFile();
    if (env_file.size()) {
        pre_script_command.append("source ");
        pre_script_command.append(env_file);
        pre_script_command.append(" && ");
    }

    std::string post_script_command = " ";
    post_script_command.append(args);

    std::string script_command = pre_script_command + script + post_script_command;

    int exit_code = exec_script(script_command, redirect_out_to);
    return exit_code;
}
int Process::exec_script(const std::string &command, int redirect_out_to) const
{
    if (DEBUG_EXEC_SCRIPT)
        fprintf(stderr, "executing command %s\n", command.c_str());
    ChildProcessIoHandler childProcessIoHandler(m_phase, m_project_name, redirect_out_to);
    childProcessIoHandler.setPrintStdOut(m_print);

    pid_t process = fork();

    if (process) {
        int child_status;
        pid_t wpid;

        childProcessIoHandler.setupMasterProcessState();

        do {
            wpid = wait(&child_status);
        } while(wpid != process);
        return WEXITSTATUS(child_status);
    } else {
        childProcessIoHandler.setupChildProcessState();
        execlp("bash", "bash", "-c", command.c_str(), nullptr);
        fprintf(stderr, "Failed to execute %s : %s\n", command.c_str(), strerror(errno));
        exit(1);
    }
    assert(false);
    return 0;
}

