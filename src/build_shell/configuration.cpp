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
#include <dirent.h>
#include <assert.h>

#include <sstream>

static bool DEBUG_FIND_SCRIPT = getenv("BUILD_SHELL_DEBUG_FIND_SCRIPT") != 0;

const char *Configuration::BuildSystemStringMap[BuildSystemSize] =
                                           { "autotools",
                                             "autoreconf",
                                             "cmake",
                                             "qmake",
                                             "mer_source",
                                             "not_recognized" };
const char *Configuration::ScmTypeStringMap[ScmTypeSize] =
                                      { "git",
                                        "svn",
                                        "not_recognized"};
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
    , m_print(false)
    , m_correct_branch(false)
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

    if (access("/dev/shm", R_OK|W_OK) == 0) {
        m_tmp_file_path = "/dev/shm";
    } else {
        m_tmp_file_path = "/tmp";
    }
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

void Configuration::setPrint(bool print)
{
    m_print = print;
}

bool Configuration::print() const
{
    return m_print;
}

void Configuration::setCorrectBranch(bool correctBranch)
{
    m_correct_branch = correctBranch;
}

bool Configuration::correctBranch() const
{
    return m_correct_branch;
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

    if (!m_src_dir.size() && m_mode == Status) {
        fprintf(stderr, "Status mode expects a source directory to be specified\n");
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
    bool should_create = m_mode == Pull || m_mode == Create || (m_mode == Build && m_pull_first);
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

int Configuration::createTempFile(const std::string &project, std::string &tmp_file) const
{
    std::string temp_file_prefix = tempFilePath() + "/build_shell_";
    tmp_file.reserve(temp_file_prefix.size() + project.size() + 8);
    tmp_file.append(temp_file_prefix);
    tmp_file.append(project);
    tmp_file.append("_XXXXXX");

    return mkstemp(&tmp_file[0]);
}

const std::string &Configuration::tempFilePath() const
{
    return m_tmp_file_path;
}

std::vector<std::string> Configuration::findScript(const std::string &primary, const std::string &fallback) const
{
    std::vector<std::string> retVec;
    const std::list<std::string> searchPath = scriptSearchPaths();
    for (int i = 0; i < 2; i++) {
        std::string script = i == 0 ? primary : fallback;
        if (script.size() == 0)
            continue;
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
    if (!path.size()) {
        return true;
    }

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

Configuration::ScmType Configuration::findScm(const std::string &path)
{
    if (access((path + "/.git").c_str(), F_OK) == 0) {
        return Git;
    } else if (access((path + "/.svn").c_str(), F_OK) == 0) {
        return Svn;
    }

    return NotRecognizedScmType;
}

Configuration::ScmType Configuration::findScmForCurrentDirectory()
{
    char current_wd[PATH_MAX];
    getcwd(current_wd, sizeof current_wd);
    return findScm(current_wd);
}

static bool reversComp(const char *file_name,const std::string &extension)
{
    size_t file_name_len= strlen(file_name);
    if (file_name_len < extension.size())
        return false;
    const char *start_from = file_name + (file_name_len - extension.size());
    return strcmp(start_from, extension.c_str()) == 0;

}

Configuration::BuildSystem Configuration::findBuildSystem(const std::string &path)
{
    Configuration::BuildSystem build_system = Configuration::NotRecognizedBuildSystem;
    DIR *source_dir = opendir(path.c_str());
    if (!source_dir) {
        fprintf(stderr, "Failed to determin build_system, since its not possible to open source dir %s\n",
                path.c_str());
        return build_system;
    }

    std::string tmp_path = path;
    std::string bname(basename(&tmp_path[0]));
    bool found_current_dir= false;
    bool found_upstream_dir = false;
    bool found_rpm_dir = false;
    while (struct dirent *ent = readdir(source_dir)) {
        if (strncmp(".",ent->d_name, sizeof(".")) == 0 ||
                strncmp("..", ent->d_name, sizeof("..")) == 0)
            continue;
        std::string filename_with_path = path + "/" + ent->d_name;
        struct stat buf;
        if (stat(filename_with_path.c_str(), &buf) != 0) {
            fprintf(stderr, "Something whent wrong when stating file %s: %s\n",
                    ent->d_name, strerror(errno));
            continue;
        }
        if (S_ISREG(buf.st_mode)) {
            std::string d_name = ent->d_name;
            if (reversComp(ent->d_name, ".pro")) {
                build_system = Configuration::QMake;
                break;
            } else if (d_name == "CMakeLists.txt") {
                build_system = CMake;
                break;
            } else if (d_name == "configure.ac") {
                build_system = AutoTools;
                break;
            }
        } else if (S_ISDIR(buf.st_mode)) {
            std::string d_name = ent->d_name;
            if (d_name == bname) {
                found_current_dir = true;
            } else if (d_name == "upstream") {
                found_upstream_dir = true;
            } else if (d_name == "rpm") {
                found_rpm_dir = true;
            }
        }
    }
    if (build_system == AutoTools) {
        std::string autogen = path + "/" + "autogen.sh";
        if (access(autogen.c_str(), F_OK)) {
            build_system = AutoReconf;
        }
    }
    if (build_system == Configuration::NotRecognizedBuildSystem
            && found_current_dir
            && found_upstream_dir
            && found_rpm_dir)
    {
        build_system = Configuration::MerSource;
    }
    closedir(source_dir);

    return build_system;
}

Configuration::BuildSystem Configuration::findBuildSystemForCurrentDirectory()
{
    char current_wd[PATH_MAX];
    getcwd(current_wd, sizeof current_wd);
    return findBuildSystem(current_wd);
}
