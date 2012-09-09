#include "configuration.h"

#include <limits.h>
#include <stdlib.h>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>

#include <iostream>

#include <error.h>

Configuration::Configuration()
    : m_mode(Invalid)
    , m_sane(false)
{
}

void Configuration::setMode(Mode mode, std::string mode_string)
{
    m_mode = mode;
    m_mode_string = mode_string;
}

Configuration::Mode Configuration::mode() const
{
    return m_mode;
}

std::string Configuration::modeString() const
{
    return m_mode_string;
}

void Configuration::setSrcDir(const char *src_dir)
{
    m_src_dir = src_dir;
}

std::string Configuration::srcDir() const
{
    return m_src_dir;
}

void Configuration::setBuildDir(const char *build_dir)
{
    m_build_dir = build_dir;
}

std::string Configuration::buildDir() const
{
    return m_build_dir;
}

void Configuration::setInstallDir(const char *install_dir)
{
    m_install_dir = install_dir;
}

std::string Configuration::installDir() const
{
    return m_install_dir;
}

void Configuration::setBuildsetFile(const char *buildset_file)
{
    m_buildset_file = buildset_file;
}

std::string Configuration::buildsetFile() const
{
    return m_buildset_file;
}

void Configuration::validate()
{
    m_sane = false;

    if (m_mode == Invalid)
        return;
    //we know we have a buildset file as its a required argument
    if (access(m_buildset_file.c_str(), F_OK)) {
        fprintf(stderr, "Unable to access buildset file %s", m_buildset_file.c_str());
        return;
    }

    //we know we have src dir as its a required argument
    if ( m_mode != Pull) {
        if (access(m_src_dir.c_str(), W_OK)) {
            fprintf(stderr, "Unable to access src dir %s, which is required for %s\n\n",
                    m_src_dir.c_str(), m_mode_string.c_str());
            return;
        }
    }
    m_src_dir = Configuration::create_and_convert_to_abs(m_src_dir.c_str());
    if (!m_src_dir.length()) {
        return;
    }

    if (m_build_dir.length()) {
        m_build_dir = Configuration::create_and_convert_to_abs(m_build_dir.c_str());
        if (!m_build_dir.length()) {
            return;
        }
    } else {
        m_build_dir = m_src_dir;
    }

    if (m_install_dir.length()) {
        m_install_dir = Configuration::create_and_convert_to_abs(m_install_dir.c_str());
        if (!m_install_dir.length()) {
            return;
        }
    } else {
        m_install_dir = m_build_dir;
    }

    m_sane = true;
}

bool Configuration::sane() const
{
    return m_sane;
}

std::string Configuration::create_and_convert_to_abs(const char *src_dir)
{
    char path_max[PATH_MAX + 1];
    char *ptr = realpath(src_dir, path_max);
    std::string ret_string;
    if (!ptr) {
        switch (errno) {
            case EACCES:
                fprintf(stderr, "Cant set src directory to %s. Access denided\n", src_dir);
                break;
            case ELOOP:
                fprintf(stderr, "Cant set src directory to %s. To many symbolic links, possible loop\n", src_dir);
                break;
            case ENOENT:
            case ENOTDIR:
                if (Configuration::recursive_mkdir(src_dir)) {
                    fprintf(stderr, "Unable to create directory %s Do you have sufficient permissions.\n", src_dir);
                } else {
                    ret_string = Configuration::create_and_convert_to_abs(src_dir);
                }
                break;
            default:
                fprintf(stderr, "Unknown error when finding the absolute path of src dir: %s\n", strerror(errno));
                break;
        }
    } else {
        ret_string = ptr;
    }

    return ret_string;
}

int Configuration::recursive_mkdir(const char *path)
{
    char out_path[PATH_MAX +1];
    char *p;
    size_t len;

    strncpy(out_path, path, sizeof(out_path));
    len = strlen(out_path);
    if(out_path[len - 1] == '/')
        out_path[len - 1] = '\0';
    for(p = out_path; *p; p++)
        if(*p == '/') {
            *p = '\0';
            if(access(out_path, F_OK))
                if (mkdir(out_path, S_IRWXU))
                    return -1;
            *p = '/';
        }
    if(access(out_path, F_OK))         /* if path is not terminated with / */
        if (mkdir(out_path, S_IRWXU))
            return -1;
    return 0;
}

