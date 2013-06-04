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

#include "buildset_tree_builder.h"

#include <string.h>

#include "json_tree.h"
#include "build_environment.h"

BuildsetTreeBuilder::BuildsetTreeBuilder(const Configuration &configuration, const std::string &file)
    : treeBuilder(file,
        std::bind(&BuildsetTreeBuilder::filterTokens, this, std::placeholders::_1))
    , m_build_environment(configuration)
{
    treeBuilder.load();
}

const std::set<std::string> BuildsetTreeBuilder::required_variables() const
{
    return m_required_variables;
}
void BuildsetTreeBuilder::filterTokens(JT::Token *next_token)
{
    JT::Token::Type value_type = next_token->value_type;
    if (value_type == JT::Token::String || value_type == JT::Token::Ascii) {
        auto variable_list = BuildEnvironment::findVariables(next_token->value.data, next_token->value.size);
        for (auto it = variable_list.begin(); it != variable_list.end(); ++it) {
            std::string variable(it->start,it->size);
            if (m_build_environment.staticVariables().find(variable) == m_build_environment.staticVariables().end())
                m_required_variables.insert(variable);
        }

    }
}
