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
#include "configuration.h"

#include "child_process_io_handler.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <libgen.h>
#include <dirent.h>
#include <assert.h>

#include <sstream>

static bool DEBUG_FIND_SCRIPT = getenv("BUILD_SHELL_DEBUG_FIND_SCRIPT") != 0;

Configuration::Configuration()
    : m_mode(Invalid)
    , m_reset_to_sha(false)
    , m_clean_explicitly_set(false)
    , m_clean(false)
    , m_deep_clean_explicitly_set(false)
    , m_deep_clean(false)
    , m_configure_explicitly_set(false)
    , m_configure(true)
    , m_build_explicitly_set(false)
    , m_build(true)
    , m_install_explicitly_set(false)
    , m_install(true)
    , m_only_one_explicitly_set(false)
    , m_only_one(false)
    , m_pull_first(false)
    , m_register(true)
    , m_sane(false)
{
#ifdef JSONMOD_PATH
    std::string old_path = getenv("PATH");
    std::string new_path = JSONMOD_PATH;
    new_path.append(":");
    new_path.append(old_path);
    setenv("PATH", new_path.c_str(),1);
#endif

    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;
    std::string config_path = std::string(homedir) + "/.config/build_shell";

    Configuration::getAbsPath(config_path, true, m_build_shell_config_path);
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
    m_buildset_config_path = std::string();
    m_script_log_path = std::string();
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
    if (!m_deep_clean) {
        m_clean_explicitly_set = true;
        m_clean = clean;
    }
}

bool Configuration::clean() const
{
    return m_clean;
}

void Configuration::setDeepClean(bool deepClean)
{
    m_deep_clean_explicitly_set = true;
    m_deep_clean = deepClean;
    m_clean = false;
    m_configure = true;
}

bool Configuration::deepClean() const
{
    return m_deep_clean;
}

void Configuration::setConfigure(bool configure)
{
    if (!m_deep_clean) {
        m_configure_explicitly_set = true;
        m_configure = configure;
    }
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

void Configuration::setInstall(bool install)
{
    m_install_explicitly_set = true;
    m_install = install;
}

bool Configuration::install() const
{
    return m_install;
}

void Configuration::setBuildFromProject(const std::string &project)
{
    m_build_from_project = project;
    if (!m_only_one_explicitly_set) {
        m_only_one = true;
    }
}
const std::string &Configuration::buildFromProject() const
{
    return m_build_from_project;
}

bool Configuration::hasBuildFromProject() const
{
    return m_build_from_project.size();
}

void Configuration::setOnlyOne(bool one)
{
    m_only_one_explicitly_set = true;
    m_only_one = one;
}

bool Configuration::onlyOne() const
{
    return m_only_one;
}

void Configuration::setPullFirst(bool pull)
{
    m_pull_first = pull;
}

bool Configuration::pullFirst() const
{
    return m_pull_first;
}

void Configuration::setRegisterBuild(bool registerBuild)
{
    m_register = registerBuild;
}

bool Configuration::registerBuild() const
{
    return m_register;
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
    if (m_mode != Generate && !m_buildset_file.size()) {
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
    bool should_create = m_mode == Pull || (m_mode == Build && m_pull_first);
    if (!Configuration::getAbsPath(m_src_dir, should_create, new_src_dir)) {
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

static int exec_script(const std::string &command, int redirect_out_to)
{
    fprintf(stderr, "executing command %s\n", command.c_str());
    ChildProcessIoHandler childProcessIoHandler(redirect_out_to);

    pid_t process = fork();

    if (process) {
        int child_status;
        pid_t wpid;

        childProcessIoHandler.setupMasterProcessState();

        do {
            wpid = wait(&child_status);
        } while(wpid != process);
        return WEXITSTATUS(child_status);
    } else {
        if (redirect_out_to >= 0) {
            childProcessIoHandler.setupChildProcessState();
        }
        execlp("bash", "bash", "-c", command.c_str(), nullptr);
        fprintf(stderr, "Failed to execute %s : %s\n", command.c_str(), strerror(errno));
        exit(1);
    }
    assert(false);
    return 0;
}

int Configuration::runScript(const std::string &env_script,
                             const std::string &script,
                             const std::string &args,
                             int redirect_out_to) const
{
    if (!script.size())
        return -1;

    std::string pre_script_command;
    if (env_script.size()) {
        pre_script_command += std::string("source ") + env_script + " && ";
    }
    std::string env_file = findBuildEnvFile();
    if (env_file.size()) {
        pre_script_command.append("source ");
        pre_script_command.append(env_file);
        pre_script_command.append(" && ");
    }

    std::string post_script_command = " ";
    post_script_command.append(args);

    std::string script_command = pre_script_command + script + post_script_command;

    return exec_script(script_command, redirect_out_to);
}

const std::string &Configuration::buildShellConfigPath() const
{
    return m_build_shell_config_path;
}

const std::string &Configuration::buildSetConfigPath() const
{
    if (m_build_dir.size() && !m_buildset_config_path.size()) {
        std::string config_path = m_build_dir + "/" + "build_shell";
        Configuration *self = const_cast<Configuration *>(this);
        Configuration::getAbsPath(config_path, true, self->m_buildset_config_path);
    }

    return m_buildset_config_path;
}

const std::string &Configuration::scriptExecutionLogPath() const
{
    if (m_build_dir.size() && !m_script_log_path.size()) {
        std::string log_path = m_build_dir + "/" + "build_shell/logs";
        Configuration *self = const_cast<Configuration *>(this);
        Configuration::getAbsPath(log_path, true, self->m_script_log_path);
    }
    return m_script_log_path;
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

    std::string home_config_scripts = m_build_shell_config_path + "/scripts";
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
        if (first_slash > current_pos) {
            std::string dir = path.substr(current_pos, first_slash - current_pos);
            if (!change_to_dir(dir,create))
                return false;
        }
        current_pos = first_slash + 1;
    }
    if (current_pos < path.size() -1) {
        std::string dir = path.substr(current_pos, first_slash - current_pos);
        if (dir.size() && !change_to_dir(dir,create))
            return false;
    }

    char cwd[PATH_MAX];
    getcwd(cwd, sizeof cwd);

    abs_path = cwd;

    return true;
}

bool Configuration::removeRecursive(const std::string &path)
{
    DIR *d = opendir(path.c_str());
    bool success = true;

    if (d) {
        struct dirent *p;

        while (success && (p=readdir(d)))
        {
            /* Skip the names "." and ".." as we don't want to recurse on them. */
            if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
            {
                continue;
            }

            struct stat statbuf;

            std::string child_file = path + "/" + p->d_name;

            if (!lstat(child_file.c_str(), &statbuf)
                && !S_ISLNK(statbuf.st_mode)
                && S_ISDIR(statbuf.st_mode))
            {
                success = removeRecursive(child_file);
            } else {
                success = (unlink(child_file.c_str()) == 0);
                if (!success)
                    fprintf(stderr, "unlinking failed on file: %s %s\n", child_file.c_str(), strerror(errno));
            }
        }

        closedir(d);
    }


    if (success) {
        success = rmdir(path.c_str()) == 0;
        if (!success)
            fprintf(stderr, "rmdir failed on: %s %s\n", path.c_str(), strerror(errno));
    }

    return success;
}

