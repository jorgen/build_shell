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
#ifndef PULL_ACTION
#define PULL_ACTION

#include "action.h"
#include "tree_builder.h"

class PullAction : public Action
{
public:
    PullAction(const Configuration &configuration);
    ~PullAction();

    bool execute();

private:
    enum ScmAction {
        Pull,
        Clone
    };

    bool determinAndRunScmAction(const std::string &relPath, JT::ObjectNode *scmNode);
    bool runPullActionForScmNode(const std::string &name, ScmAction action, JT::ObjectNode *scmNode);
    TreeBuilder m_buildset_tree_builder;
    JT::ObjectNode *m_buildset_tree;
};

#endif
