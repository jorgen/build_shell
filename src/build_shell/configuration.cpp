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
#include <libgen.h>

#include <sstream>

static bool DEBUG_FIND_SCRIPT = getenv("BUILD_SHELL_DEBUG_FIND_SCRIPT") != 0;
static bool DEBUG_RUN_COMMAND = getenv("BUILD_SHELL_DEBUG_RUN_COMMAND") != 0;

Configuration::Configuration()
    : m_mode(Invalid)
    , m_reset_to_sha(false)
    , m_clean_explicitly_set(false)
    , m_clean(true)
    , m_configure_explicitly_set(false)
    , m_configure(true)
    , m_build_explicitly_set(false)
    , m_build(true)
    , m_sane(false)
{
#ifdef JSONMOD_PATH
    std::string old_path = getenv("PATH");
    std::string new_path = JSONMOD_PATH;
    new_path.append(":");
    new_path.append(old_path);
    setenv("PATH", new_path.c_str(),1);
#endif

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

void Configuration::setResetToSha(bool reset)
{
    m_reset_to_sha = reset;
}

bool Configuration::resetToSha() const
{
    return m_reset_to_sha;
}

void Configuration::setClean(bool clean)
{
    m_clean_explicitly_set = true;
    m_clean = clean;
}

bool Configuration::clean() const
{
    return m_clean;
}

void Configuration::setConfigure(bool configure)
{
    m_configure_explicitly_set = true;
    m_configure = configure;
}

bool Configuration::configure() const
{
    return m_configure;
}

void Configuration::setBuild(bool build)
{
    m_build_explicitly_set = true;
    m_build = build;
}

bool Configuration::build() const
{
    return m_build;
}

void Configuration::validate()
{
    m_sane = false;

    if (m_mode == Invalid)
        return;

    if (m_buildset_file.size()) {
        if (access(m_buildset_file.c_str(), F_OK)) {
            fprintf(stderr, "Unable to access buildset file %s", m_buildset_file.c_str());
            return;
        }
        char buildset_realpath[PATH_MAX];
        if (realpath(m_buildset_file.c_str(), buildset_realpath)) {
            m_buildset_file = buildset_realpath;
        }
    }
    if (m_mode != Create && !m_buildset_file.size()) {
        fprintf(stderr, "All modes except for create expects a buildset file\n");
        return;
    }

    if (!m_src_dir.size() && m_buildset_file.size()) {
        std::string dirname_of_buildset = m_buildset_file;
        m_src_dir = dirname(&dirname_of_buildset[0]);
    }

    if (!m_src_dir.size()) {
        char current_wd[PATH_MAX];
        if (getcwd(current_wd, sizeof current_wd))
            m_src_dir = current_wd;
    }

    std::string new_src_dir;
    if (!Configuration::getAbsPath(m_src_dir, m_mode == Pull, new_src_dir)) {
        fprintf(stderr, "Failed to set src dir to %s. Is it a valid directory?\n",
                m_src_dir.c_str());
        return;
    }
    m_src_dir = std::move(new_src_dir);

    if (m_build_dir.length()) {
        std::string new_build_dir;
        if(!Configuration::getAbsPath(m_build_dir,true,new_build_dir)) {
            fprintf(stderr, "Failed to verify build dir path. Is it a valid directory name? %s\n",
                    m_build_dir.c_str());
            return;
        }
        m_build_dir = std::move(new_build_dir);
    } else {
        m_build_dir = m_src_dir;
    }

    if (m_install_dir.length()) {
        std::string new_install_dir;
        if (!Configuration::getAbsPath(m_install_dir, true, new_install_dir)) {
            fprintf(stderr, "Failed to verify install dir path. Is it a valid directory name? %s\n",
                    m_install_dir.c_str());
            return;
        }
        m_install_dir = std::move(new_install_dir);
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

int Configuration::runScript(const std::string &script, const std::string &args) const
{
    if (!script.size())
        return -1;

    std::string pre_script_command;
    std::string env_file = findBuildEnvFile();
    if (env_file.size()) {
        pre_script_command.append("source ");
        pre_script_command.append(env_file);
        pre_script_command.append(" && ");
    }

    std::string post_script_command = " ";
    post_script_command.append(args);

    std::string script_command = pre_script_command;
    script_command.append(script);
    script_command.append(post_script_command);

    if (DEBUG_RUN_COMMAND) {
        fprintf(stderr, "Executing script command %s\n", script_command.c_str());
    }
    return system(script_command.c_str());
}

int Configuration::createTempFile(const std::string &project, std::string &tmp_file)
{
    const char temp_file_prefix[] = "/tmp/build_shell_";
    tmp_file.reserve(sizeof temp_file_prefix + project.size() + 8);
    tmp_file.append(temp_file_prefix);
    tmp_file.append(project);
    tmp_file.append("_XXXXXX");

    return mkstemp(&tmp_file[0]);
}

std::vector<std::string> Configuration::findScript(const std::string &primary, const std::string &fallback) const
{
    std::vector<std::string> retVec;
    const std::list<std::string> searchPath = scriptSearchPaths();
    for (int i = 0; i < 2; i++) {
        std::string script = i == 0 ? primary : fallback;
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
                retVec.push_back(full_script_path);
            }
        }
    }

    if (DEBUG_FIND_SCRIPT) {
        fprintf(stderr, "found %zu scripts when looking for %s : %s\n", retVec.size(),
                primary.c_str(), fallback.c_str());
    }
    return retVec;
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
    std::string abs_home_config_scripts;
    if (Configuration::getAbsPath(home_config_scripts,true,abs_home_config_scripts))
        m_script_search_paths.push_back(abs_home_config_scripts);

    m_script_search_paths.push_back(SCRIPTS_PATH);
}

class ResetPath
{
public:
    ResetPath()
    {
        getcwd(initial_dir, sizeof initial_dir);
    }

    ~ResetPath()
    {
        chdir(initial_dir);
    }
    char initial_dir[PATH_MAX];
};

static bool change_to_dir(const std::string &dir, bool create)
{
    if (chdir(dir.c_str())) {
        if (!create)
            return false;
        if (errno == ENOENT) {
            if (mkdir(dir.c_str(),S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH))
                return false;
        }
        if (chdir(dir.c_str())) {
            return false;
        }
    }
    return true;
}

bool Configuration::getAbsPath(const std::string &path, bool create, std::string &abs_path)
{
    if (!path.size())
        return false;

    ResetPath resetPath;
    (void) resetPath;

    std::list<std::string>dirs;


    size_t first_slash;
    size_t current_pos = 0;

    if (path[0] == '/') {
        if (chdir("/")) {
            fprintf(stderr, "change dir into root %s\n", strerror(errno));
            return false;
        }
    }
    while ((first_slash = path.find('/', current_pos)) != std::string::npos) {
        if (current_pos != first_slash) {
            std::string dir = path.substr(current_pos, first_slash - current_pos);
            if (!change_to_dir(dir,create))
                return false;
        }
        current_pos = first_slash + 1;
    }
    if (current_pos != path.size() -1) {
        std::string dir = path.substr(current_pos, first_slash - current_pos);
        if (!change_to_dir(dir,create))
            return false;
    }

    char cwd[PATH_MAX];
    getcwd(cwd, sizeof cwd);

    abs_path = cwd;

    return true;
}

