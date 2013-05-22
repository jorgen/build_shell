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
#ifndef ACTION_H
#define ACTION_H

#include "configuration.h"

namespace JT {
    class ObjectNode;
}

class Action
{
public:
    Action(const Configuration &configuration);
    virtual ~Action();

    virtual bool execute() = 0;

    bool error() const;

protected:
    static bool flushProjectNodeToTemporaryFile(const std::string &project_name, JT::ObjectNode *node, std::string &file_flushed_to);

    bool executeScript(const std::string &step,
                       const std::string &projectName,
                       const std::string &fallback,
                       JT::ObjectNode *projectNode,
                       JT::ObjectNode **returnedObjectNode);
    bool executeScript(const std::string &env_script,
                       const std::string &step,
                       const std::string &projectName,
                       const std::string &fallback,
                       JT::ObjectNode *projectNode,
                       JT::ObjectNode **returnedObjectNode);
    bool executeScript(const std::string &env_script,
                       const std::string &step,
                       const std::string &projectName,
                       const std::string &fallback,
                       int log_file,
                       JT::ObjectNode *projectNode,
                       JT::ObjectNode **returnedObjectNode);
    const Configuration &m_configuration;
    bool m_error;
};

#endif //ACTION_H
