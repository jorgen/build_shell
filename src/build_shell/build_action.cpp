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
#include "build_action.h"

#include "tree_writer.h"
#include "create_action.h"
#include "available_builds.h"
#include "temp_file.h"
#include "pull_action.h"

#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <fcntl.h>
#include <time.h>

#include <memory>

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
            std::string d_name = ent->d_name;
            if (reversComp(ent->d_name, ".pro")) {
                build_system = "qmake";
                break;
            } else if (d_name == "CMakeLists.txt") {
                build_system = "cmake";
                break;
            } else if (d_name == "configure.ac") {
                build_system = "autotools";
                break;
            }
        }
    }
    if (build_system == "autotools") {
        std::string autogen = source_path + "/" + "autogen.sh";
        if (access(autogen.c_str(), F_OK)) {
            build_system = "autoreconf";
        }
    }
    closedir(source_dir);
    if (!build_system.size()) {
        build_system = "not_determind";
    }
    return build_system;
}

static bool ensureFileExist(const std::string &file)
{
    if (access(file.c_str(), F_OK|W_OK)) {
        mode_t create_mode = S_IRWXU | S_IRWXG;
        int file_desc = open(file.c_str(), O_RDWR | O_CLOEXEC | O_CREAT | O_EXCL, create_mode);
        if (file_desc < 0) {
            fprintf(stderr, "Failed to create file: %s\n%s\n", file.c_str(), strerror(errno));
            return false;
        }
    }
    return true;
}
BuildAction::BuildAction(const Configuration &configuration)
    : Action(configuration)
    , m_buildset_tree_builder(configuration.buildsetFile())
    , m_env_script_builder(configuration)
{
    if (m_configuration.pullFirst()) {
        PullAction pull_action(configuration);
        if (pull_action.error()) {
            m_error = true;
            return;
        }
        m_error = !pull_action.execute();
    }

    if (m_error)
        return;

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
        fprintf(stderr, "Failed to get build_shell meta dir\n");
        m_error = true;
        return;
    }

    m_set_build_env_file = build_shell_meta_dir + "/" + "set_build_env.sh";
    m_unset_build_env_file = build_shell_meta_dir + "/" + "unset_build_env.sh";

    size_t last_build_dir_slash = m_configuration.buildsetFile().rfind('/');
    std::string build_name;
    if (last_build_dir_slash < m_configuration.buildsetFile().size()) {
        build_name = m_configuration.buildsetFile().substr(last_build_dir_slash + 1);
    } else {
        build_name = m_configuration.buildsetFile();
    }


    if (m_configuration.registerBuild()) {
        AvailableBuilds available_builds(m_configuration);
        available_builds.addAvailableBuild(m_configuration.buildDir(), m_set_build_env_file.c_str());
    }

    m_error = !ensureFileExist(m_set_build_env_file);
    if (m_error)
        return;
    m_error = !ensureFileExist(m_unset_build_env_file);
    if (m_error)
        return;

    std::string build_shell_build_sets_dir;
    if (!Configuration::getAbsPath("build_shell/build_sets", true, build_shell_build_sets_dir)) {
        fprintf(stderr, "Failed to get build_sets dir\n");
        m_error = true;
        return;
    }

    time_t actual_time = time(0);
    struct tm *local_tm = localtime(&actual_time);
    std::string dateInFormat(20,'\0');
    snprintf(&dateInFormat[0], 20,"%02d:%02d:%02d-%02d-%02d-%04d",
            local_tm->tm_hour, local_tm->tm_min, local_tm->tm_sec,
            local_tm->tm_mday, local_tm->tm_mon+1, 1900 + local_tm->tm_year);

    m_stored_buildset = build_shell_build_sets_dir + "/" + dateInFormat.c_str() + std::string(".buildset");
    CreateAction create_action(m_configuration, m_stored_buildset);
    m_error = !create_action.execute();
}


BuildAction::~BuildAction()
{
    m_env_script_builder.writeScripts(m_set_build_env_file, m_unset_build_env_file,"");
    std::string stored_buildset_finished = m_stored_buildset + "_finished";
    CreateAction create_action(m_configuration,stored_buildset_finished);
    create_action.execute();
    std::string current_buildset_name = m_configuration.buildDir() + "/build_shell/current_buildset";
    TreeWriter current_tree_writer(current_buildset_name, m_buildset_tree);
    if (current_tree_writer.error()) {
        fprintf(stderr, "Failed to write current buildset to file %s\n", current_buildset_name.c_str());
        return;
    }
    TreeWriter finished(stored_buildset_finished, m_buildset_tree);

}

class ArgumentsCleanup
{
public:
    ArgumentsCleanup(JT::ObjectNode *root_node)
        : m_root_node(root_node)
    {

    }

    ~ArgumentsCleanup()
    {
        for (auto it = m_root_node->begin(); it != m_root_node->end(); ++it) {
            JT::ObjectNode *project_node = it->second->asObjectNode();
            if (!project_node)
                continue;
            delete project_node->take("arguments");
        }
    }

    JT::ObjectNode *m_root_node;
};

bool BuildAction::execute()
{
    if (!m_buildset_tree || m_error)
        return false;

    long num_cpu = sysconf( _SC_NPROCESSORS_ONLN );
    char cpu_buf[4];
    snprintf(cpu_buf, sizeof cpu_buf, "%ld", num_cpu);

    ArgumentsCleanup argCleanup(m_buildset_tree);

    bool found = !m_configuration.hasBuildFromProject();
    const std::string &build_from_project = m_configuration.buildFromProject();
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
        JT::ObjectNode *env_variables = new JT::ObjectNode();
        arguments->insertNode(std::string("environment"), env_variables);

        if (!found && build_from_project != it->first.string())
            continue;
        found = true;

        chdir(project_build_path.c_str());

        JT::ObjectNode *updated_project_node;
        if (!executeScript("", "pre_build", project_name, build_system, project_node, &updated_project_node)) {
            delete updated_project_node;
            return false;
        }
        if (updated_project_node) {
            m_buildset_tree->insertNode(it->first,updated_project_node, true);
        }

        if (m_configuration.onlyOne())
            break;
    }

    found = !m_configuration.hasBuildFromProject();
    for (auto it = m_buildset_tree->begin(); it != m_buildset_tree->end(); ++it) {
        JT::ObjectNode *project_node = it->second->asObjectNode();
        if (!project_node)
            continue;
        if (!found && build_from_project != it->first.string()) {
            m_env_script_builder.addProjectNode(it->first.string(), project_node);
            continue;
        }
        found = true;

        const std::string project_name = it->first.string();
        const std::string &project_build_path = project_node->stringAt("arguments.build_path");
        const std::string &project_build_system = project_node->stringAt("arguments.build_system");

        if (chdir(project_build_path.c_str())) {
            fprintf(stderr, "Failed to move into directory %s to build\n", project_build_path.c_str());
            return false;
        }
        JT::ObjectNode *updated_project_node;
        if (!handleBuildForProject(project_name, project_build_system, project_node, &updated_project_node)) {
            delete updated_project_node;
            return false;
        }
        if (updated_project_node) {
            m_buildset_tree->insertNode(it->first, updated_project_node,true);
            project_node = updated_project_node;
        }
        m_env_script_builder.addProjectNode(it->first.string(),project_node);

        if (m_configuration.onlyOne())
            break;
    }

    found = !m_configuration.hasBuildFromProject();
    for (auto it = m_buildset_tree->begin(); it != m_buildset_tree->end(); ++it) {
        JT::ObjectNode *project_node = it->second->asObjectNode();
        if (!project_node)
            continue;
        if (!found && build_from_project != it->first.string())
            continue;
        found = true;

        const std::string project_name = it->first.string();
        const std::string &project_build_path = project_node->stringAt("arguments.build_path");
        const std::string &project_build_system = project_node->stringAt("arguments.build_system");

        if (chdir(project_build_path.c_str())) {
            fprintf(stderr, "Failed to move into directory %s to do post build scripts\n", project_build_path.c_str());
            return false;
        }
        JT::ObjectNode *updated_project_node;
        if (!executeScript(m_set_build_env_file,"post_build", project_name, project_build_system, project_node, &updated_project_node)) {
            delete updated_project_node;
            return false;
        }
        if (updated_project_node) {
            m_buildset_tree->insertNode(it->first,updated_project_node, true);
        }

        if (m_configuration.onlyOne())
            break;
    }

    return true;
}

bool BuildAction::handleBuildForProject(const std::string &projectName, const std::string &buildSystem, JT::ObjectNode *projectNode, JT::ObjectNode **updatedProjectNode)
{
    fprintf(stderr, "Processing buildstep for %s\n", projectName.c_str());
    TempFile temp_file(projectName + "_env");
    m_env_script_builder.writeSetScript(temp_file, projectName);
    temp_file.close();

    std::unique_ptr<JT::ObjectNode> temp_pointer(nullptr);
    JT::ObjectNode *project_node = projectNode;
    JT::ObjectNode *updated_project_node = 0;
    *updatedProjectNode = 0;

    if (m_configuration.clean()) {
        if (!executeScript(temp_file.name(), "clean", projectName, buildSystem, project_node,&updated_project_node))
            return false;
    } else if (updated_project_node) {
        project_node = updated_project_node;
        temp_pointer.reset(project_node);
    }

    if (m_configuration.deepClean()) {
        std::string scm_type = projectNode->stringAt("scm.type");
        if (scm_type.size() == 0) {
            scm_type = "regular";
        }
        const std::string &build_path = projectNode->stringAt("arguments.build_path");
        const std::string &src_path = projectNode->stringAt("arguments.src_path");
        std::string cwd("\0");
        cwd.reserve(PATH_MAX);
        getcwd(&cwd[0], PATH_MAX);

        if (build_path.size() && build_path != src_path) {
            if (!Configuration::removeRecursive(build_path.c_str())) {
                fprintf(stderr, "Failed to remove build dir %s\n", build_path.c_str());
                m_error = true;
                return false;
            }
            std::string actual_build_path;
            Configuration::getAbsPath(build_path, true, actual_build_path);
        }

        if (src_path.size()) {
            if (chdir(src_path.c_str())) {
                fprintf(stderr, "Failed to change to source directory. Skipping deep clean %s\n", strerror(errno));
            } else {
                if (!executeScript(temp_file.name(), "deep_clean", projectName, scm_type, project_node, &updated_project_node)) {
                    return false;
                } else if (updated_project_node) {
                    project_node = updated_project_node;
                    temp_pointer.reset(project_node);
                }
            }
        }

        chdir(cwd.c_str());
    }

    if (m_configuration.configure()) {
        if (!executeScript(temp_file.name(), "configure", projectName, buildSystem, project_node, &updated_project_node)) {
            return false;
        } else  if (updated_project_node) {
            project_node = updated_project_node;
            temp_pointer.reset(project_node);
        }
    }

    if (m_configuration.build()) {
        if (!executeScript(temp_file.name(), "build", projectName, buildSystem, project_node, &updated_project_node)) {
            return false;
        } else if (updated_project_node) {
            project_node = updated_project_node;
            temp_pointer.reset(project_node);
        }

        if (m_configuration.install() && project_node->nodeAt("no_install") == nullptr) {
            if (!executeScript(temp_file.name(), "install", projectName, buildSystem, project_node,&updated_project_node)) {
                return false;
            } else if (updated_project_node){
                temp_pointer.reset(updated_project_node);
            }
        }
    }

   *updatedProjectNode = temp_pointer.release();

    return true;
}
