#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <string>

class Configuration
{
public:
    explicit Configuration();

    void setInputFile(const char *input);
    bool hasInputFile() const;
    const std::string &inputFile() const;

    void setProperty(const char *property);
    bool hasProperty() const;
    const std::string &property() const;

    void setValue(const char *value);
    bool hasValue() const;
    const std::string &value()const;

    void setInline(bool shouldInline);
    bool hasInlineSet() const;

    void setCompactPrint(bool compactPrint);
    bool compactPrint() const;

    void setCreateObject(bool create);
    bool createObject() const;

    bool sane() const;
private:
    std::string m_input_file;
    std::string m_property;
    std::string m_value;

    bool m_inline;
    bool m_compact;
    bool m_create_object;
};

#endif //CONFIGURATION_H
