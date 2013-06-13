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

#ifndef BUILDSET_TREE_WRITER_H
#define BUILDSET_TREE_WRITER_H

#include "tree_writer.h"
#include "build_environment.h"
#include "transformer_state.h"

class BuildsetTreeWriter : public TreeWriter
{
public:
    BuildsetTreeWriter(const BuildEnvironment &buildEnvironment, const std::string &project, const std::string &file);
    BuildsetTreeWriter(const BuildEnvironment &buildEnvironment, const std::string &project, int file, bool closeFileOnDestruct = false);

private:
    const JT::Token &token_transformer(const JT::Token &next_token);

    const BuildEnvironment &m_build_environment;
    TransformerState m_transformer_state;
    std::function<const JT::Token&(const JT::Token &)> m_token_transformer;
};

#endif
