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

#ifndef BUILDSETTREEBUILDER_H
#define BUILDSETTREEBUILDER_H

#include "tree_builder.h"

#include <set>

#include "build_environment.h"
#include "transformer_state.h"

class BuildsetTreeBuilder
{
public:
    BuildsetTreeBuilder(const BuildEnvironment &buildEnv, const std::string &file, bool print, bool allowMissingVariables);

    bool error() const;
    TreeBuilder treeBuilder;

    const std::set<std::string> &required_variables() const;
    bool has_missing_variables() const;

    void printMissingVariablesMessage();
private:
    void filterTokens(JT::Token *next_token);
    std::set<std::string> m_required_variables;
    std::list<std::string> m_missing_variables;
    const BuildEnvironment &m_build_environment;
    TransformerState m_transformer_state;
    bool m_print;
    bool m_allow_missing_variables;
    bool m_error;
};

#endif
