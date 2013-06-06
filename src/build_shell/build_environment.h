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

#ifndef BUILD_ENVIRONMENT_H
#define BUILD_ENVIRONMENT_H

#include <set>
#include <string>
#include <list>

#include "configuration.h"

namespace JT {
    class ObjectNode;
}

struct Variable
{
    Variable()
        : start(0)
        , size(0)
    { }
    const char *start;
    size_t size;
};

class BuildEnvironment
{
public:
    BuildEnvironment(const Configuration &configuration);
    ~BuildEnvironment();

    std::string getVariable(const std::string &variable, const std::string &project) const;
    void setVariable(const std::string &variable, const std::string &value, const std::string &project = "");
    const std::set<std::string> &staticVariables() const;
    bool isStaticVariable(const std::string &variable) const;

    const std::string expandVariablesInString(const std::string &str, const std::string &project = "") const;

    bool canResolveVariable(const std::string &variable, const std::string &project) const;
    static const std::list<Variable> findVariables(const char *str, const size_t size);
private:
    const Configuration &m_configuration;
    const std::string m_environment_file;

    JT::ObjectNode *m_environment_node;
    std::set<std::string> m_static_variables;
};

#endif //BUILD_ENVIRONMENT_H
