/*
 * Copyright © 2014 Jørgen Lind

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
#ifndef BUILDSET_CONVERTER_H
#define BUILDSET_CONVERTER_H

#include <string>

#include "json_tree.h"

class BuildsetConverter
{
public:
    BuildsetConverter(const std::string &inputFile);

    void convert();

private:
    void readMoreData(const char *);
    void processToken(JT::Token &token);
    void flushBuffer(JT::Serializer *serializer);

    const std::string &m_input_file;
    int m_input_filedesc;

    JT::Tokenizer m_tokenizer;
    char m_in_buffer[4096];
    bool m_no_more_data;
    bool m_at_root;

    JT::ObjectNode *m_root_object;
    JT::ArrayNode *m_projects_array;
    JT::ObjectNode *m_project_object;
};

#endif //BUILDSET_CONVERTER_H
