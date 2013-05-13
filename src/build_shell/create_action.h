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
#ifndef CREATE_ACTION_H
#define CREATE_ACTION_H

#include "action.h"
#include "tree_builder.h"
#include <memory>
namespace JT {
    class ObjectNode;
}

class CreateAction : public Action
{
public:
    CreateAction(const Configuration &configuration,
            const std::string &outfile = std::string());
    ~CreateAction();

    bool execute();

private:
    bool handleCurrentSrcDir();

    TreeBuilder m_tree_builder;
    std::unique_ptr<JT::ObjectNode> m_out_tree;
    std::string m_out_file_name;
    int m_out_file;
};

#endif //CREATE_ACTION_H
