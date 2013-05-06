#include "build_action.h"

#include "tree_writer.h"
#include "create_action.h"

#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <fcntl.h>
#include <time.h>

static bool reversComp(const char *file_name,const std::string &extension)
{
    size_t file_name_len= strlen(file_name);
    if (file_name_len < extension.size())
        return false;
    const char *start_from = file_name + (file_name_len - extension.size());
    return strcmp(start_from, extension.c_str()) == 0;

}

static std::string autodetect_build_system(const std::string &source_path)
{
    std::string build_system;
    DIR *source_dir = opendir(source_path.c_str());
    if (!source_dir) {
        fprintf(stderr, "Failed to determin build_system, since its not possible to open source dir %s\n",
                source_path.c_str());
        return build_system;
    }
    while (struct dirent *ent = readdir(source_dir)) {
        if (strncmp(".",ent->d_name, sizeof(".")) == 0 ||
            strncmp("..", ent->d_name, sizeof("..")) == 0)
            continue;
        std::string filename_with_path = source_path + "/" + ent->d_name;
        struct stat buf;
        if (stat(filename_with_path.c_str(), &buf) != 0) {
            fprintf(stderr, "Something whent wrong when stating file %s: %s\n",
                    ent->d_name, strerror(errno));
            continue;
        }
        if (S_ISREG(buf.st_mode)) {
            if (reversComp(ent->d_name, ".pro")) {
                build_system = "qmake";
                break;
            } else if (reversComp(ent->d_name, "CMakeLists.txt")) {
                build_system = "cmake";
                break;
            } else if (reversComp(ent->d_name, "autogen.sh")) {
                build_system = "autotools";
                break;
            }
        }
    }
    closedir(source_dir);
    if (!build_system.size()) {
        build_system = "not_determind";
    }
    return build_system;
}

BuildAction::BuildAction(const Configuration &configuration)
    : Action(configuration)
    , m_buildset_tree_builder(configuration.buildsetFile())
{
    m_buildset_tree = m_buildset_tree_builder.rootNode();
    if (!m_buildset_tree) {
        fprintf(stderr, "Error loading buildset %s\n",
                configuration.buildsetFile().c_str());
        m_error = true;
    }

    if (chdir(m_configuration.buildDir().c_str())) {
        fprintf(stderr, "Could not move into build dir:%s\n%s\n",
                m_configuration.buildDir().c_str(), strerror(errno));
        m_error = true;
        return;
    }

    std::string build_shell_meta_dir;
    if (!Configuration::getAbsPath("build_shell", true, build_shell_meta_dir)) {
        m_error = true;
        return;
    }

    std::string build_env_file = build_shell_meta_dir + "/" + "build_env.json";
    if (access(build_env_file.c_str(), F_OK)) {
        JT::ObjectNode *root_env_node = new JT::ObjectNode();
        mode_t create_mode = S_IROTH | S_IRGRP | S_IRUSR | S_IWUSR;
        int build_env_desc= open(build_env_file.c_str(), O_RDWR | O_CLOEXEC | O_CREAT | O_EXCL, create_mode);
        if (build_env_desc < 0) {
            delete root_env_node;
            fprintf(stderr, "Failed to create build_env.json file: %s\n%s\n", build_env_file.c_str(), strerror(errno));
            m_error = true;
            return;
        }
        TreeWriter writer(build_env_desc,root_env_node,true);
        if (writer.error())
            m_error = true;
        return;
    }

    std::string build_shell_build_sets_dir;
    if (!Configuration::getAbsPath("build_shell/build_sets", true, build_shell_build_sets_dir)) {
        m_error = true;
        return;
    }

    time_t actual_time = time(0);
    struct tm *local_tm = localtime(&actual_time);
    std::string dateInFormat(20,'\0');
    snprintf(&dateInFormat[0], 20,"%02i%02i%02i%02i%02i%04i",
            local_tm->tm_sec, local_tm->tm_min, local_tm->tm_hour,
            local_tm->tm_mday, local_tm->tm_mon, local_tm->tm_year);

    CreateAction create_action(m_configuration, build_shell_build_sets_dir + dateInFormat + ".buildset");
    m_error = create_action.execute();
}

BuildAction::~BuildAction()
{
}

bool BuildAction::execute()
{
    if (!m_buildset_tree || m_error)
        return false;

    long num_cpu = sysconf( _SC_NPROCESSORS_ONLN );
    char cpu_buf[4];
    snprintf(cpu_buf, sizeof cpu_buf, "%ld", num_cpu);
    for (auto it = m_buildset_tree->begin(); it != m_buildset_tree->end(); ++it) {
        JT::ObjectNode *project_node = it->second->asObjectNode();
        if (!project_node)
            continue;

        if (chdir(m_configuration.buildDir().c_str())) {
            fprintf(stderr, "Could not move into build dir:%s\n%s\n",
                    m_configuration.buildDir().c_str(), strerror(errno));
            return false;
        }

        const std::string project_name = it->first.string();
        std::string project_build_path = m_configuration.buildDir() + "/" + project_name;
        fprintf(stderr, "Project %s, full path %s\n", project_name.c_str(), project_build_path.c_str());
        std::string project_src_path = m_configuration.srcDir() + "/" + project_name;

        if (access(project_src_path.c_str(), X_OK|R_OK)) {
            fprintf(stderr, "Problem accessing source path: %s for project %s. Can not complete build\n",
                    project_src_path.c_str(), project_name.c_str());
            return false;
        }

        std::string build_system = autodetect_build_system(project_src_path.c_str());
        if (access(project_build_path.c_str(), X_OK|R_OK)) {
            bool failed_mkdir = true;
            if (errno == ENOENT) {
                if (!mkdir(project_build_path.c_str(),S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)) {
                    failed_mkdir = false;
                }
            }
                if (failed_mkdir) {
                    fprintf(stderr, "Failed to verify build path %s for project %s. Can not complete build\n",
                            project_build_path.c_str(), project_name.c_str());
                    return false;
                }
        }

        JT::ObjectNode *arguments = new JT::ObjectNode();
        project_node->insertNode(std::string("arguments"), arguments, true);
        arguments->addValueToObject("src_path", project_src_path, JT::Token::String);
        arguments->addValueToObject("build_path", project_build_path, JT::Token::String);
        arguments->addValueToObject("install_path", m_configuration.installDir(), JT::Token::String);
        arguments->addValueToObject("build_system", build_system, JT::Token::String);
        arguments->addValueToObject("cpu_count", cpu_buf, JT::Token::Number);

        chdir(project_build_path.c_str());

        if (!executeScript("pre_build", project_name, build_system, project_node))
            return false;
    }

    for (auto it = m_buildset_tree->begin(); it != m_buildset_tree->end(); ++it) {
        JT::ObjectNode *project_node = it->second->asObjectNode();
        if (!project_node)
            continue;
        const std::string project_name = it->first.string();
        const std::string &project_build_path = project_node->stringAt("arguments.build_path");
        const std::string &project_build_system = project_node->stringAt("arguments.build_system");

        if (chdir(project_build_path.c_str())) {
            fprintf(stderr, "Failed to move into directory %s to build\n", project_build_path.c_str());
            return false;
        }
        if (!handleBuildForProject(project_name, project_node, project_build_system)) {
            return false;
        }
    }

    for (auto it = m_buildset_tree->begin(); it != m_buildset_tree->end(); ++it) {
        JT::ObjectNode *project_node = it->second->asObjectNode();
        if (!project_node)
            continue;
        const std::string project_name = it->first.string();
        const std::string &project_build_path = project_node->stringAt("arguments.build_path");
        const std::string &project_build_system = project_node->stringAt("arguments.build_system");

        if (chdir(project_build_path.c_str())) {
            fprintf(stderr, "Failed to move into directory %s to do post build scripts\n", project_build_path.c_str());
            return false;
        }
        if (!executeScript("post_build", project_name, project_build_system, project_node)) {
            return false;
        }
    }
    return true;
}

bool BuildAction::handleBuildForProject(const std::string &projectName, JT::ObjectNode *projectNode, const std::string &buildSystem)
{
    if (m_configuration.clean()) {
        if (!executeScript("clean", projectName, buildSystem, projectNode))
            return false;
    }

    if (m_configuration.configure()) {
        if (!executeScript("configure", projectName, buildSystem, projectNode))
            return false;
    }

    if (m_configuration.build()) {
        if (!executeScript("configure", projectName, buildSystem, projectNode))
            return false;
    }

    return true;
}
