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
#include "tree_builder.h"

#include "json_tree.h"

TreeBuilder::TreeBuilder(const std::string &file,
            std::function<void(JT::Token *next_token)> token_transformer)
    : m_node(nullptr)
    , m_file_name(file)
    , m_mapped_file(m_file_name)
    , m_token_transformer(token_transformer)
{
    if (!m_file_name.size()) {
        return;
    }

}

TreeBuilder::~TreeBuilder()
{
}

void TreeBuilder::load()
{
    const char *data = static_cast<const char *>(m_mapped_file.map());
    if (!data)
        return;
    JT::TreeBuilder tree_builder;
    tree_builder.create_root_if_needed = true;
    JT::Tokenizer tokenizer;
    tokenizer.allowNewLineAsTokenDelimiter(true);
    tokenizer.allowSuperfluousComma(true);
    tokenizer.addData(data, m_mapped_file.size());
    tokenizer.registerTokenTransformer(m_token_transformer);
    auto tree_build = tree_builder.build(&tokenizer);
    if (tree_build.second == JT::Error::NoError) {
        m_node.reset(tree_build.first->asObjectNode());
    }

    if (!m_node)
        delete tree_build.first;
}

JT::ObjectNode *TreeBuilder::rootNode() const
{
    return m_node.get();
}

JT::ObjectNode *TreeBuilder::takeRootNode()
{
    return m_node.release();
}

