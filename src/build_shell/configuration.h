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

    void validate();
    bool sane() const;

    const std::list<std::string> &scriptSearchPaths() const;

    std::vector<std::string> findScript(const std::string &script, const std::string &fallback) const;
    int runScript(const std::string &script, const std::string &arg) const;

    static int createTempFile(const std::string &project, std::string &tmp_file_name);
private:

    std::string findBuildEnvFile() const;
    void initializeScriptSearchPaths();
    static std::string create_and_convert_to_abs(const std::string &path);
    static bool recursive_mkdir(const std::string &path);

    Mode m_mode;
    std::string m_mode_string;
    std::string m_src_dir;
    std::string m_build_dir;
    std::string m_install_dir;
    std::string m_buildset_file;
    std::string m_buildset_out_file;
    bool m_reset_to_sha;

    std::list<std::string> m_script_search_paths;

    bool m_sane;
};

#endif //CONFIGURATION_H
