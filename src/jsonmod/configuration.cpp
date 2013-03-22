#include "configuration.h"

#include <limits.h>
#include <stdlib.h>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>

#include <iostream>

#ifdef __MACH__
#include <errno.h>
#else
#include <error.h>
#endif //__MACH__

Configuration::Configuration()
{

}

void Configuration::setInputFile(const char *input)
{
    m_input_file=input;
}

bool Configuration::hasInputFile() const
{
    return m_input_file.size() != 0;
}

const std::string &Configuration::inputFile() const
{
    return m_input_file;
}

void Configuration::setProperty(const char *property)
{
    m_property = property;
}

bool Configuration::hasPropertyFile() const
{
    return m_property.size() != 0;
}

const std::string &Configuration::property() const
{
    return m_property;
}

void Configuration::setValue(const char *value)
{
    m_value = value;
}

bool Configuration::hasValue() const
{
    return m_value.size() != 0;
}

const std::string &Configuration::value() const
{
    return m_value;
}

void Configuration::setInline(bool shouldInline)
{
    m_inline = shouldInline;
}

bool Configuration::hasInlineSet() const
{
    return m_inline;
}

void Configuration::setShouldPrettyPrint(bool shouldPrettyPrint)
{
    m_human = shouldPrettyPrint;
}

bool Configuration::shouldPrettyPrint() const
{
    return m_human;
}

bool Configuration::sane() const
{
    return true;
}
