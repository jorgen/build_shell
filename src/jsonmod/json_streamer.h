#ifndef JSON_STREAMER_H
#define JSON_STREAMER_H

#include "json_tokenizer.h"
#include "configuration.h"

#include <string>
#include <vector>

class JsonStreamer
{
public:
    JsonStreamer(const Configuration &config);
    ~JsonStreamer();

    bool error() const { return m_error; }

    void stream();

    void requestFlushOutBuffer(JT::Serializer *);
private:
    enum MatchingState {
        LookingForMatch,
        Matching,
        NoMatch
    };
    void createPropertyVector();
    bool matchAtDepth(const JT::Data &data) const;
    void writeOutBuffer(const JT::SerializerBuffer &buffer);

    void setStreamerOptions(bool compact);

    const Configuration &m_config;

    JT::Tokenizer m_tokenizer;
    JT::Serializer m_serializer;

    int m_input_file;
    int m_output_file;
    std::string m_tmp_output;

    std::vector<std::string> m_property;

    bool m_error;
    bool m_print_subtree;

    size_t m_current_depth;
    size_t m_last_matching_depth;
};

#endif //JSON_STREAMER_H
