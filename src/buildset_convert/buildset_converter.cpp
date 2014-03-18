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

#include "buildset_converter.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

BuildsetConverter::BuildsetConverter(const std::string &inputFile)
    : m_input_file(inputFile)
    , m_input_filedesc(0)
    , m_no_more_data(false)
    , m_at_root(true)
    , m_root_object(new JT::ObjectNode())
    , m_projects_array(new JT::ArrayNode())
    , m_project_object(nullptr)
{
    m_input_filedesc = open(m_input_file.c_str(), O_RDONLY|O_CLOEXEC);
    if (m_input_filedesc < 0) {
        fprintf(stderr, "%s\n", strerror(errno));
        return;
    }

    if (lseek(m_input_filedesc, 0,SEEK_SET) < 0) {
        fprintf(stderr, "%s\n", strerror(errno));
        return;
    }

    std::function<void(const char *)> callback =
        std::bind(&BuildsetConverter::readMoreData, this, std::placeholders::_1);
    m_tokenizer.registerRelaseCallback(callback);

    m_tokenizer.allowNewLineAsTokenDelimiter(true);
    m_tokenizer.allowSuperfluousComma(true);

    m_root_object->insertNode(std::string("projects"), m_projects_array);
}

void BuildsetConverter::convert()
{
    JT::Token token;
    JT::Error tokenizer_error;
    bool root_token = true;
    while ((tokenizer_error = m_tokenizer.nextToken(&token)) <= JT::Error::NeedMoreData) {
        if (tokenizer_error == JT::Error::NeedMoreData) {
            if (m_no_more_data)
                break;
        } else if (root_token) {
            root_token = false;
            continue;
        } else {
            processToken(token);
        }
    }

    char out_buffer[4096];
    JT::TreeSerializer serializer(out_buffer, 4096);
    JT::SerializerOptions options(true, true);
    serializer.setOptions(options);

    std::function<void(JT::Serializer *)> callback =
        std::bind(&BuildsetConverter::flushBuffer, this, std::placeholders::_1);
    serializer.addRequestBufferCallback(callback);
    serializer.serialize(m_root_object);
    flushBuffer(&serializer);
    fprintf(stdout, "\n");

}

void BuildsetConverter::processToken(JT::Token &token)
{
    if (token.value_type == JT::Token::ObjectEnd)
        return;
    if (token.value_type != JT::Token::ObjectStart) {
        fprintf(stderr, "%s : %s\n", std::string(token.name.data, token.name.size).c_str(),
                                     std::string(token.value.data, token.value.size).c_str());
        fprintf(stderr, "Expected project object start\n");
    }
    JT::Token propToken;
    propToken.value_type = token.name_type;
    propToken.value = token.name;
    JT::StringNode *projectName = new JT::StringNode(&propToken);

    JT::TreeBuilder builder;
    auto build_result = builder.build(&token, &m_tokenizer);
    if (build_result.second != JT::Error::NoError) {
        fprintf(stderr, "Failed to parse project node %s\n", projectName->string().c_str());
        return;
    }
    if (JT::ObjectNode *projectNode = build_result.first->asObjectNode()) {
        projectNode->insertNode(std::string("name"), projectName, false, true);
        m_projects_array->append(projectNode);
    } else {
        fprintf(stderr, "Unexpected parsed type of project node %s\n", projectName->string().c_str());
        return;
    }
}

void BuildsetConverter::flushBuffer(JT::Serializer *serializer)
{
    auto serializerBuffer = serializer->buffers().front();
    write(STDOUT_FILENO, serializerBuffer.buffer, serializerBuffer.used);
}

void BuildsetConverter::readMoreData(const char *)
{
    size_t bytes_read = read(m_input_filedesc, m_in_buffer, 4096);
    if (bytes_read) {
        m_tokenizer.addData(m_in_buffer, bytes_read);
    } else {
        m_no_more_data = true;
    }
}

