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
#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <string>
#include <list>
#include <vector>
#include <functional>

class Configuration
{
public:
    explicit Configuration();

    enum Mode {
        Invalid,
        Pull,
        Create,
        Build,
        Rebuild
    };

    void setMode(Mode mode, std::string mode_string);
    Mode mode() const;
    std::string modeString() const;

    void setSrcDir(const char *src_dir);
    const std::string &srcDir() const;
    void setBuildDir(const char *build_dir);
    const std::string &buildDir() const;
    void setInstallDir(const char *install_dir);
    const std::string &installDir() const;

    void setBuildsetFile(const char *buildset_file);
    const std::string &buildsetFile() const;

    void setBuildsetOutFile(const char *buildset_out_file);
    const std::string &buildsetOutFile() const;

    void setResetToSha(bool reset);
    bool resetToSha() const;

    void setClean(bool clean);
    bool clean() const;

    void setDeepClean(bool deepClean);
    bool deepClean() const;

    void setConfigure(bool configure);
    bool configure() const;

    void setBuild(bool build);
    bool build() const;

    void setInstall(bool install);
    bool install() const;

    void setBuildFromProject(const std::string &project);
    const std::string &buildFromProject() const;
    bool hasBuildFromProject() const;

    void setOnlyOne(bool one);
    bool onlyOne() const;

    void setPullFirst(bool pull);
    bool pullFirst() const;

    void setRegisterBuild(bool dontRegister);
    bool registerBuild() const;

    void validate();
    bool sane() const;

    const std::list<std::string> &scriptSearchPaths() const;

    std::vector<std::string> findScript(const std::string &script, const std::string &fallback) const;
    int runScript(const std::string &env_script, const std::string &script, const std::string &arg) const;

    const std::string &buildShellConfigPath() const;
    static int createTempFile(const std::string &project, std::string &tmp_file_name);

    static bool getAbsPath(const std::string &path, bool create, std::string &abs_path);
    static bool removeRecursive(const std::string &path);
private:

    std::string findBuildEnvFile() const;
    void initializeScriptSearchPaths();

    Mode m_mode;
    std::string m_mode_string;
    std::string m_src_dir;
    std::string m_build_dir;
    std::string m_install_dir;
    std::string m_buildset_file;
    std::string m_buildset_out_file;
    std::string m_build_from_project;
    std::string m_build_shell_config_path;
    bool m_reset_to_sha;
    bool m_clean_explicitly_set;
    bool m_clean;
    bool m_deep_clean_explicitly_set;
    bool m_deep_clean;
    bool m_configure_explicitly_set;
    bool m_configure;
    bool m_build_explicitly_set;
    bool m_build;
    bool m_install_explicitly_set;
    bool m_install;
    bool m_only_one_explicitly_set;
    bool m_only_one;
    bool m_pull_first;
    bool m_register;

    std::list<std::string> m_script_search_paths;

    bool m_sane;
};

#endif //CONFIGURATION_H
