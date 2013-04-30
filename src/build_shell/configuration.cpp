#include "configuration.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <sstream>

static bool DEBUG_FIND_SCRIPT = getenv("BUILD_SHELL_DEBUG_FIND_SCRIPT") != 0;
static bool DEBUG_RUN_COMMAND = getenv("BUILD_SHELL_DEBUG_RUN_COMMAND") != 0;

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

const std::string &Configuration::srcDir() const
{
    return m_src_dir;
}

void Configuration::setBuildDir(const char *build_dir)
{
    m_build_dir = build_dir;
}

const std::string &Configuration::buildDir() const
{
    return m_build_dir;
}

void Configuration::setInstallDir(const char *install_dir)
{
    m_install_dir = install_dir;
}

const std::string &Configuration::installDir() const
{
    return m_install_dir;
}

void Configuration::setBuildsetFile(const char *buildset_file)
{
    m_buildset_file = buildset_file;
}

const std::string &Configuration::buildsetFile() const
{
    return m_buildset_file;
}

void Configuration::setBuildsetOutFile(const char *buildset_out_file)
{
    m_buildset_out_file = buildset_out_file;
}

const std::string &Configuration::buildsetOutFile() const
{
    return m_buildset_out_file;
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

const std::list<std::string> &Configuration::scriptSearchPaths() const
{
    if (!m_script_search_paths.size()) {
        Configuration *self = const_cast<Configuration *>(this);
        self->initializeScriptSearchPaths();
    }
    return m_script_search_paths;
}

int Configuration::runScript(const std::string script, const std::string &args) const
{
    std::stringstream script_command;
    std::string env_file = findBuildEnvFile();
    if (env_file.size()) {
        script_command << "source " << env_file << " && ";
    }

    std::string abs_script = findScript(script);
    if (!abs_script.size())
        return -1;
    script_command << abs_script;

    if (args.size()) {
        script_command << " " << args;
    }

    if (DEBUG_RUN_COMMAND)
        fprintf(stderr, "Executing script command %s\n", script_command.str().c_str());
    return system(script_command.str().c_str());
}

int Configuration::runScript(const std::string script, const std::list<std::string> &args) const
{
    std::stringstream arguments;
    for (auto it = args.begin(); it != args.end(); ++it) {
        if (it != args.begin())
            arguments << " ";
        arguments << *it;
    }
    return runScript(script, arguments.str());
}

std::string Configuration::findScript(const std::string script) const
{
    const std::list<std::string> searchPath = scriptSearchPaths();
    for (auto it = searchPath.begin(); it != searchPath.end(); ++it) {
        std::string full_script_path = *it;
        if (!full_script_path.back() != '/')
            full_script_path.append("/");
        full_script_path.append(script);
        if (DEBUG_FIND_SCRIPT)
            fprintf(stderr, "Looking for script %s\n", full_script_path.c_str());
        if (access(full_script_path.c_str(), R_OK) == 0) {
            if (DEBUG_FIND_SCRIPT)
                fprintf(stderr, "Found script %s\n", full_script_path.c_str());
            return full_script_path;
        }
    }
    return std::string();
}

std::string Configuration::findBuildEnvFile() const
{
    std::string build_env_file = m_build_dir;
    build_env_file.append("/build_shell/variables.env");
    if (access(build_env_file.c_str(), R_OK) == 0)
        return build_env_file;
    return std::string();
}

static std::string find_build_shell_script_dir(const std::string &path)
{
    if (!path.size())
        return std::string();
    std::string script_path = path;
    script_path.append("/build_shell/scripts");
    if (access(script_path.c_str(), R_OK) == 0) {
        char real_scripts_path[PATH_MAX+1];
        return realpath(script_path.c_str(), real_scripts_path);
    }
    return std::string();
}

void Configuration::initializeScriptSearchPaths()
{
    std::string build_dir_scritps = find_build_shell_script_dir(m_build_dir);
    if (build_dir_scritps.size())
        m_script_search_paths.push_back(build_dir_scritps);

    if (m_build_dir != m_src_dir) {
        std::string src_dir_scripts = find_build_shell_script_dir(m_src_dir);
        if (src_dir_scripts.size())
            m_script_search_paths.push_back(src_dir_scripts);
    }

    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;
    std::string home_config_scripts(homedir);
    home_config_scripts.append("/.config/build_shell/scripts");
    std::string abs_home_config_scripts =
        create_and_convert_to_abs(home_config_scripts.c_str());
    m_script_search_paths.push_back(abs_home_config_scripts);

    m_script_search_paths.push_back(SCRIPTS_PATH);
}

std::string Configuration::create_and_convert_to_abs(const std::string &dir)
{
    char path_max[PATH_MAX + 1];
    char *ptr = realpath(dir.c_str(), path_max);
    std::string ret_string;
    if (!ptr) {
        switch (errno) {
            case EACCES:
                fprintf(stderr, "Cant set src directory to %s. Access denided\n", dir.c_str());
                break;
            case ELOOP:
                fprintf(stderr, "Cant set src directory to %s. To many symbolic links, possible loop\n", dir.c_str());
                break;
            case ENOENT:
            case ENOTDIR:
                if (Configuration::recursive_mkdir(dir)) {
                    fprintf(stderr, "Unable to create directory %s Do you have sufficient permissions.\n", dir.c_str());
                } else {
                    ret_string = Configuration::create_and_convert_to_abs(dir);
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

bool Configuration::recursive_mkdir(const std::string &path)
{
    size_t first_slash;
    size_t current_pos = 0;

    std::list<std::string>dirs;

    if (path.at(0) == '/')
        dirs.push_back("/");

    while ((first_slash = path.find('/', current_pos)) != std::string::npos) {
        if (current_pos != first_slash) {
            dirs.push_back(path.substr(current_pos, first_slash - current_pos));
        }
        current_pos = first_slash + 1;
    }
    if (current_pos != path.size() -1) {
            dirs.push_back(path.substr(current_pos, first_slash - current_pos));
    }

    std::string intermediate_path;
    for (auto it = dirs.begin(); it != dirs.end(); ++it) {
        intermediate_path.append(*it);
        if (access(intermediate_path.c_str(), X_OK)) {
            if (mkdir(intermediate_path.c_str(), S_IRWXU)) {
                return false;
            }
        }
        if (intermediate_path.back() != '/')
            intermediate_path.append("/");
    }

    return true;
}

