#include "json_streamer.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <errno.h>
#include <string.h>

JsonStreamer::JsonStreamer(const Configuration &config)
    : m_config(config)
    , m_error(false)
    , m_print_subtree(false)
    , m_current_depth(-1)
    , m_last_matching_depth(-1)
{
    if (config.hasInputFile()) {
        m_input_file = open(m_config.inputFile().c_str(), O_RDONLY|O_CLOEXEC);

        if (m_input_file < 0) {
            fprintf(stderr, "%s\n", strerror(errno));
            m_error = true;
            return;
        }
        if (lseek(m_input_file, 0,SEEK_SET) < 0) {
            m_error = true;
            return;
        }
    } else {
        m_input_file = STDIN_FILENO;
    }

    if (config.hasInlineSet() && config.hasInputFile()) {
        m_tmp_output = config.inputFile();
        m_tmp_output.append("XXXXXX");
        m_output_file = mkstemp(&m_tmp_output[0]);
        if (m_output_file == -1) {
            fprintf(stderr, "%s\n", strerror(errno));
            m_error = true;
            return;
        }
    } else {
        m_output_file = STDOUT_FILENO;
    }

    if (!m_config.hasProperty()) {
        m_print_subtree = true;
    }

    createPropertyVector();

    std::function<void(JT::Serializer *)> callback=
        std::bind(&JsonStreamer::requestFlushOutBuffer, this, std::placeholders::_1);

    m_serializer.addRequestBufferCallback(callback);

    setStreamerOptions(m_config.compactPrint());
}

JsonStreamer::~JsonStreamer()
{
    if (m_config.hasInputFile()) {
        close(m_input_file);
    }
    if (m_config.hasInlineSet() && m_config.hasInputFile()) {
        fsync(m_output_file);
        close(m_output_file);
        if (!m_error) {
            rename(m_tmp_output.c_str(), m_config.inputFile().c_str());
        } else if (m_tmp_output.length()) {
            if (unlink(m_tmp_output.c_str())) {
                fprintf(stderr, "%s\n", strerror(errno));
            }
        }
    }
}

void JsonStreamer::requestFlushOutBuffer(JT::Serializer *serializer)
{
    JT::SerializerBuffer buffer = serializer->buffers().front();
    serializer->clearBuffers();

    writeOutBuffer(buffer);
    serializer->appendBuffer(buffer.buffer,buffer.size);
}

void JsonStreamer::stream()
{
    char in_buffer[4096];
    char out_buffer[4096];
    m_serializer.appendBuffer(out_buffer, sizeof out_buffer);
    ssize_t bytes_read;
    std::vector<bool> m_found_on_depth;
    while((bytes_read = read(m_input_file, in_buffer, 4096)) > 0) {
        m_tokenizer.addData(in_buffer,bytes_read, true);
        JT::Token token;
        JT::Error tokenizer_error;
        while ((tokenizer_error = m_tokenizer.nextToken(&token)) == JT::Error::NoError) {
            bool print_token = false;
            bool finished_printing_subtree = false;
            if (!m_print_subtree && m_current_depth - 1 == m_last_matching_depth) {
                switch (token.name_type) {
                    case JT::Token::String:
                        token.name.data++;
                        token.name.size -= 2;
                    case JT::Token::Ascii:
                        if (matchAtDepth(token.name)) {
                            m_last_matching_depth = m_current_depth;
                            if (m_last_matching_depth == m_property.size() - 1) {
                                print_token = true;
                                if (m_config.hasValue()) {
                                    token.value.data = m_config.value().c_str();
                                    token.value.size = m_config.value().size();
                                } else {
                                    token.name.data = "";
                                    token.name.size = 0;
                                    token.name_type = JT::Token::Ascii;
                                }
                            }
                            m_found_on_depth[m_current_depth] = true;
                        }
                        break;
                    default:
                        fprintf(stderr, "found invalid unrecognized type\n");
                        tokenizer_error = JT::Error::InvalidToken;
                        break;

                }
            }
            switch(token.value_type) {
                case JT::Token::ObjectStart:
                case JT::Token::ArrayStart:
                    m_current_depth++;
                    m_found_on_depth.push_back(false);
                    if (print_token) {
                        if (m_config.hasValue()) {
                            fprintf(stderr, "Its not possible to change the value of and object or array\n");
                            m_error = true;
                            return;
                            break;
                        }
                        m_print_subtree = true;
                        setStreamerOptions(true);
                    }
                    break;
                case JT::Token::ObjectEnd:
                case JT::Token::ArrayEnd:
                    if (m_last_matching_depth == m_current_depth -1
                            && !m_found_on_depth.back()) {
                        if (m_property.size() -1 == m_current_depth && m_config.hasValue()) {
                            JT::Token new_token;
                            new_token.name_type = JT::Token::String;
                            new_token.name.data = m_property.back().c_str();
                            new_token.name.size = m_property.back().size();
                            new_token.value_type = JT::Token::String;
                            new_token.value.data = m_config.value().c_str();
                            new_token.value.size = m_config.value().size();
                            m_serializer.write(new_token);
                        } else if (m_config.createObject()) {
                            for (size_t i = m_current_depth; i < m_property.size(); i++) {
                                JT::Token new_token;
                                new_token.name_type = JT::Token::String;
                                new_token.name.data = m_property[i].c_str();
                                new_token.name.size = m_property[i].size();
                                new_token.value_type = JT::Token::ObjectStart;
                                new_token.value.data = "{";
                                new_token.value.size = 1;
                                m_serializer.write(new_token);
                            }
                            for (size_t i = m_property.size(); i > m_current_depth; i--) {
                                JT::Token new_token;
                                new_token.name_type = JT::Token::Ascii;
                                new_token.name.data = "";
                                new_token.name.size = 0;
                                new_token.value_type = JT::Token::ObjectEnd;
                                new_token.value.data = "}";
                                new_token.value.size = 1;
                                m_serializer.write(new_token);
                            }
                        }
                    }

                    if (m_current_depth -1 == m_last_matching_depth)
                        m_last_matching_depth--;
                    if (m_print_subtree && m_last_matching_depth == m_current_depth) {
                        finished_printing_subtree = true;
                    }
                    m_found_on_depth.pop_back();
                    m_current_depth--;
                    break;
                default:
                    break;
            }

            if (print_token || m_print_subtree || m_config.hasValue() || m_config.createObject()) {
                m_serializer.write(token);
            }

            if (finished_printing_subtree) {
                m_print_subtree = false;
                print_token = true;
                setStreamerOptions(m_config.compactPrint());
            }


        }
        auto unflushed_out_buffers = m_serializer.buffers();
        for (auto it = unflushed_out_buffers.begin(); it != unflushed_out_buffers.end(); ++it) {
            writeOutBuffer(*it);
        }
        char new_line[] = "\n";
        write(m_output_file, new_line, sizeof new_line - 1);
        if (tokenizer_error != JT::Error::NeedMoreData
                && tokenizer_error != JT::Error::NoError) {
            fprintf(stderr, "Error while parsing json. %d\n", tokenizer_error);
            break;
        }
    }
    if (bytes_read < 0) {
        fprintf(stderr, "Error while reading input %s\n", strerror(errno));
        m_error = true;
    }
}

void JsonStreamer::createPropertyVector()
{
    const std::string &property = m_config.property();
    size_t pos = 0;
    while (pos < property.size()) {
        size_t new_pos = property.find('.', pos);
        m_property.push_back(property.substr(pos, new_pos - pos));
        pos = new_pos;
        if (new_pos != std::string::npos)
            pos++;
    }
}

bool JsonStreamer::matchAtDepth(const JT::Data &data) const
{
    if (m_current_depth >= m_property.size())
        return false;
    const std::string &property = m_property[m_current_depth];
    if (data.size != property.size())
        return false;
    if (memcmp(data.data, property.c_str(), data.size))
        return false;

    return true;
}

void JsonStreamer::writeOutBuffer(const JT::SerializerBuffer &buffer)
{
    size_t written = write(m_output_file,buffer.buffer,buffer.used);
    if (written < buffer.used) {
        fprintf(stderr, "Error while writing to outbuffer: %s\n", strerror(errno));
        m_error = true;
    }
}

void JsonStreamer::setStreamerOptions(bool compact)
{
    JT::SerializerOptions options = m_serializer.options();
    options.setPretty(!compact);
    options.skipDelimiter(!m_config.hasProperty() && !compact);
    m_serializer.setOptions(options);
}

