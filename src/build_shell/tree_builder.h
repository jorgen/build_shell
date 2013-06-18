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
#ifndef TREE_BUILDER_H
#define TREE_BUILDER_H

#include <string>
#include <functional>
#include <memory>

#include "mmapped_file.h"

namespace JT {
    class ObjectNode;
    struct Token;
    class Tokenizer;
}

class TreeBuilder
{
public:
    TreeBuilder(const std::string &file,
            std::function<void(JT::Token *next_token)> token_transformer = nullptr);
    ~TreeBuilder();

    bool load();
    JT::ObjectNode *rootNode() const;
    JT::ObjectNode *takeRootNode();

private:
    std::unique_ptr<JT::ObjectNode> m_node;
    const std::string &m_file_name;
    MmappedReadFile m_mapped_file;
    std::function<void(JT::Token *next_token)> m_token_transformer;

};

#endif //TREE_BUILDER_H
