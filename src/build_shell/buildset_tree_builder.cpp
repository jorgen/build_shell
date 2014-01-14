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

BuildsetTreeBuilder::BuildsetTreeBuilder(const BuildEnvironment &buildEnv, const std::string &file, bool print, bool allowMissingVariables)
    : treeBuilder(file,
        std::bind(&BuildsetTreeBuilder::filterTokens, this, std::placeholders::_1))
    , m_build_environment(buildEnv)
    , m_transformer_state(m_build_environment)
    , m_print(print)
    , m_allow_missing_variables(allowMissingVariables)
    , m_error(!treeBuilder.load())
{
    if (!m_allow_missing_variables && !m_missing_variables.empty()) {
        if (m_print) {
            fprintf(stderr, "Build set mode: %s does not allow to proceed with undefined variablees\n\n", buildEnv.configuration().modeString().c_str());
        }
        m_error = true;
    }
}

bool BuildsetTreeBuilder::error() const
{
    return m_error;
}

const std::set<std::string> &BuildsetTreeBuilder::required_variables() const
{
    return m_required_variables;
}

bool BuildsetTreeBuilder::has_missing_variables() const
{
    return !m_missing_variables.empty();
}

void BuildsetTreeBuilder::printMissingVariablesMessage()
{
    if (m_print && m_missing_variables.size()) {
        fprintf(stderr, "Buildset is missing variables:\n");
        for (auto it = m_missing_variables.begin(); it != m_missing_variables.end(); ++it) {
            fprintf(stderr, "\t - %s\n", it->c_str());
        }
        fprintf(stderr, "\nPlease use bss to select a build shell,\n");
        fprintf(stderr, "and then use bs_variable to add variables to your buildset environment:\n");
        fprintf(stderr, "\t bs_variable \"some_variable\" \"some_value\"\n");
        fprintf(stderr, "or if you need to limit scope of the variable\n");
        fprintf(stderr, "\t bs_variable \"some_project\" \"some_variable\" \"some_value\"\n\n");
    }
}

void BuildsetTreeBuilder::filterTokens(JT::Token *next_token)
{
    m_transformer_state.updateState(*next_token);

    JT::Token::Type value_type = next_token->value_type;
    if (value_type == JT::Token::String || value_type == JT::Token::Ascii) {
        auto variable_list = BuildEnvironment::findVariables(next_token->value.data, next_token->value.size);
        for (auto it = variable_list.begin(); it != variable_list.end(); ++it) {
            std::string variable(it->start,it->size);
            if (!m_build_environment.isStaticVariable(variable)){
                if (!m_build_environment.canResolveVariable(variable, m_transformer_state.current_project)) {
                    m_missing_variables.push_back(variable);
                }

                m_required_variables.insert(variable);
            }
        }

    }
    printMissingVariablesMessage();
}
