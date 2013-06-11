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

#ifndef PROCESS_H
#define PROCESS_H

#include "configuration.h"
#include "json_tree.h"

#include <string>

class Process
{
public:
    Process(const Configuration &configuration);
    ~Process();

    bool run(JT::ObjectNode **returnedObjectNode = 0);

    void setEnvironmentScript(const std::string &environment_script);
    void setPhase(const std::string &phase);
    void setProjectName(const std::string &projectName);
    void setFallback(const std::string &fallback);

    void setLogFile(int logFile, bool closeFileOnDelete);
    void setLogFile(const std::string &logFile, bool append = false, bool closeFileOnDelete = true);

    void setProjectNode(JT::ObjectNode *projectNode);

    void registerTokenTransformer(std::function<const JT::Token&(const JT::Token &)> token_transformer);

    void setPrint(bool print);
private:
    bool flushProjectNodeToTemporaryFile(const std::string &project_name, JT::ObjectNode *node, std::string &file_flushed_to) const;
    int runScript(const std::string &env_script,
                  const std::string &script,
                  const std::string &args,
                  int redirect_out_to,
                  bool print) const;
    int exec_script(const std::string &command, int redirect_out_to, bool print) const;

    const Configuration &m_configuration;
    std::string m_environement_script;
    std::string m_phase;
    std::string m_project_name;
    std::string m_fallback;

    int m_log_file;
    std::string m_log_file_str;
    bool m_close_log_file;
    bool m_print;

    JT::ObjectNode *m_project_node;
    std::function<const JT::Token&(const JT::Token &)> m_token_transformer;
};

#endif
