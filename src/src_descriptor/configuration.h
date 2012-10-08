#include <string>

class Configuration
{
public:
    explicit Configuration();

    enum Mode {
        Invalid,
        Pull,
        Create,
        Build
    };

    void setMode(Mode mode, std::string mode_string);
    Mode mode() const;
    std::string modeString() const;

    void setSrcDir(const char *src_dir);
    std::string srcDir() const;
    void setBuildDir(const char *build_dir);
    std::string buildDir() const;
    void setInstallDir(const char *install_dir);
    std::string installDir() const;

    void setBuildsetFile(const char *buildset_file);
    std::string buildsetFile() const;

    void setBuildsetOutFile(const char *buildset_out_file);
    std::string buildsetOutFile() const;

    void validate();
    bool sane() const;
private:
    static std::string create_and_convert_to_abs(const char *src_dir);
    static int recursive_mkdir(const char *path);

    Mode m_mode;
    std::string m_mode_string;
    std::string m_src_dir;
    std::string m_build_dir;
    std::string m_install_dir;
    std::string m_buildset_file;
    std::string m_buildset_out_file;

    bool m_sane;
};

