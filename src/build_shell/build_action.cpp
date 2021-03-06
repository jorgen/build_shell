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
#include "buildset_generator.h"
#include "available_builds.h"
#include "temp_file.h"
#include "pull_action.h"
#include "process.h"
#include "reset_path.h"

#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <fcntl.h>
#include <time.h>
#include <libgen.h>

#include <memory>
#include <algorithm>
#include <string>

class ProcessBuilder
{
public:
    ProcessBuilder(const Configuration &configuration)
        : configuration(configuration)
    { }
    const Configuration &configuration;
    std::string env_script;
    std::string project_name;
    std::string fallback;

    Process build() const
    {
        Process process(configuration);
        process.setEnvironmentScript(env_script);
        process.setProjectName(project_name);
        process.setFallback(fallback);
        return process;
    }
};

BuildAction::BuildAction(const Configuration &configuration)
    : Action(configuration)
    , m_build_environment(configuration)
    , m_buildset_tree_builder(m_build_environment, configuration.buildsetFile(), true, false)
{
    if (m_buildset_tree_builder.error()) {
        m_error = true;
        return;
    }

    m_buildset_tree = m_buildset_tree_builder.treeBuilder.rootNode();

    if (m_configuration.pullFirst()) {
        PullAction pull_action(configuration);
        if (pull_action.error()) {
            m_error = true;
            return;
        }
        m_error = !pull_action.execute();
    }
}


BuildAction::~BuildAction()
{
    if (m_error)
        return;

    TreeBuilder buildset_builder(m_configuration.currentBuildsetFile());
    buildset_builder.load();
    JT::ObjectNode *buildset =  buildset_builder.rootNode();

    BuildsetGenerator buildset_generator(m_configuration);
    if (!buildset_generator.updateBuildsetNode(buildset)) {
        fprintf(stderr, "Failed to generate buildset node, updating current buildset failed\n");
        return;
    }

    std::string build_shell_build_sets_dir;
    if (!Configuration::getAbsPath(m_configuration.buildShellMetaDir() + "/build_sets", true, build_shell_build_sets_dir)) {
        fprintf(stderr, "Failed to get build_sets dir\n");
        return;
    }
    time_t actual_time = time(0);
    struct tm *local_tm = localtime(&actual_time);
    std::string dateInFormat(20,'\0');
    snprintf(&dateInFormat[0], 20,"%04d-%02d-%02d-%02d:%02d:%02d",
             1900 + local_tm->tm_year, local_tm->tm_mon+1, local_tm->tm_mday,
             local_tm->tm_hour, local_tm->tm_min, local_tm->tm_sec);
    std::string stored_buildset = build_shell_build_sets_dir + "/" + dateInFormat.c_str() + std::string(".buildset");

    {
        TreeWriter writer(stored_buildset);
        writer.write(buildset);
        if (writer.error()) {
            fprintf(stderr, "Failed to write date stamped buildset\n");
        }
    }
    {
        TreeWriter writer(m_configuration.currentBuildsetFile());
        writer.write(buildset);
        if (writer.error()) {
            fprintf(stderr, "Failed to update current buildset\n");
        }
    }

    EnvScriptBuilder env_script_builder(m_configuration, m_build_environment, m_buildset_tree);
    env_script_builder.writeScripts(m_configuration.buildShellSetEnvFile(), m_configuration.buildShellUnsetEnvFile());
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

class PhaseReporter
{
public:
    PhaseReporter(const std::string &phase, const std::string &project)
        : m_project(project)
        , m_success(false)
    {
        m_phase = phase;
        std::transform(m_phase.begin(), m_phase.end(), m_phase.begin(), ::toupper);
    }
    ~PhaseReporter()
    {
        std::string status = m_success ? "SUCCESS" : "FAILED";
        std::string print_success = std::string("\n") +
            "************************************************************************\n"
            "\t\t " + m_phase + " " + status + ": " + m_project + "\n"
            "************************************************************************\n"
            "\n";
        fprintf(stdout, "%s", print_success.c_str());
    }

    void markSuccess() { m_success = true; }
private:
    std::string m_phase;
    const std::string m_project;
    bool m_success;
};

bool BuildAction::execute()
{
    PhaseReporter reporter("EXECUTING", "BUILD MODE");
    if (!m_buildset_tree || m_error)
        return false;

    ArgumentsCleanup argCleanup(m_buildset_tree);

    auto end_it = endIterator(m_buildset_tree);
    for (auto it = startIterator(m_buildset_tree); it != end_it; ++it) {
        JT::ObjectNode *project_node = it->second->asObjectNode();
        if (!project_node)
            continue;

        const std::string project_name = it->first.string();
        bool skip_project = project_node->booleanAt("default_skip") && m_configuration.buildFromProject() != project_name;
        if (skip_project)
            continue;

        if (!handlePrebuild(project_name, project_node))
            return false;

        const std::string &project_build_path = project_node->stringAt("arguments.build_path");
        const std::string &project_build_system = project_node->stringAt("arguments.build_system");

        const std::string &move_to_dir = project_build_path.size() ? project_build_path : m_configuration.buildDir();

        if (chdir(move_to_dir.c_str())) {
            fprintf(stderr, "Failed to move into directory %s to build\n", project_build_path.c_str());
            m_error = true;
            return false;
        }
        if (!handleBuildForProject(project_name, project_build_system, project_node)) {
            return false;
        }

        if (chdir(move_to_dir.c_str())) {
            fprintf(stderr, "Failed to move into directory %s to do post build scripts\n", project_build_path.c_str());
            m_error = true;
            return false;
        }
        {
            Process process(m_configuration);
            process.setEnvironmentScript(m_configuration.buildShellSetEnvFile());
            process.setPhase("post_build");
            process.setProjectName(project_name);
            process.setFallback(project_build_system);
            process.setProjectNode(project_node, &m_build_environment);
            process.setPrint(true);
            process.setScriptHasToExist(false);
            if (!process.run()) {
                return false;
            }
        }

        if (m_configuration.onlyOne())
            break;
    }
    reporter.markSuccess();
    return true;
}

bool BuildAction::handlePrebuild(const std::string &project_name, JT::ObjectNode *project_node)
{
    if (chdir(m_configuration.buildDir().c_str())) {
        fprintf(stderr, "Could not move into build dir:%s\n%s\n",
                m_configuration.buildDir().c_str(), strerror(errno));
        m_error = true;
        return false;
    }

    std::string project_src_path = m_configuration.srcDir() + "/" + project_name;
    std::string project_build_path;
    bool dont_shadow = project_node->booleanAt("no_shadow");
    if (dont_shadow) {
        project_build_path = project_src_path;
    } else {
        project_build_path = m_configuration.buildDir() + "/" + project_name;
    }

    if (access(project_src_path.c_str(), X_OK|R_OK) && project_node->objectNodeAt("scm")) {
        fprintf(stderr, "Problem accessing source path: %s for project %s. Running pull action\n",
                project_src_path.c_str(), project_name.c_str());
        {
            Configuration clone_conf = m_configuration;
            clone_conf.setBuildFromProject(project_name);
            clone_conf.setOnlyOne(true);
            PullAction pull_action(clone_conf);
            if (pull_action.error()) {
                m_error = true;
                return false;
            }
            m_error = !pull_action.execute();
            if (m_error)
                return false;
        }
        if (access(project_src_path.c_str(), X_OK|R_OK)) {
            fprintf(stderr, "Problem accessing source path: %s for project %s. Can not complete build\n",
                    project_src_path.c_str(), project_name.c_str());
            m_error = true;
            return false;
        }
    }

    if (access(project_build_path.c_str(), X_OK|R_OK) && access(project_src_path.c_str(), X_OK|R_OK) == 0) {
        bool failed_mkdir = true;
        if (errno == ENOENT) {
            if (!mkdir(project_build_path.c_str(),S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)) {
                failed_mkdir = false;
            }
        }
        if (failed_mkdir) {
            fprintf(stderr, "Failed to verify build path %s for project %s. Can not complete build\n",
                    project_build_path.c_str(), project_name.c_str());
            m_error = true;
            return false;
        }
    }

    JT::ObjectNode *arguments = new JT::ObjectNode();
    arguments->addValueToObject("install_path", m_configuration.installDir(), JT::Token::String);

    if (project_node->objectNodeAt("scm")) {
        Configuration::BuildSystem build_system = Configuration::findBuildSystem(project_src_path.c_str());
        if (build_system == Configuration::MerSource) {
            std::string tmp_src_path = project_src_path;
            std::string base_name = basename(&tmp_src_path[0]);
            std::string project_mer_src_path = project_src_path + "/" + base_name;
            build_system = Configuration::findBuildSystem(project_mer_src_path);
            if (build_system != Configuration::NotRecognizedBuildSystem) {
                std::string tmp_build_path;
                Configuration::getAbsPath(project_build_path + "/" + base_name, true, tmp_build_path);
                std::string tmp_src_path;
                Configuration::getAbsPath(project_mer_src_path, false, tmp_src_path);
                project_src_path = tmp_src_path;
                project_build_path = tmp_build_path;
            }
        }
        std::string build_system_string = Configuration::BuildSystemStringMap[build_system];
        arguments->addValueToObject("build_system", build_system_string, JT::Token::String);
    }

    bool should_chdir_to_build = false;
    if (access(project_src_path.c_str(), X_OK|R_OK) == 0) {
        arguments->addValueToObject("src_path", project_src_path, JT::Token::String);
        arguments->addValueToObject("build_path", project_build_path, JT::Token::String);
        should_chdir_to_build = true;
    }

    project_node->insertNode(std::string("arguments"), arguments, true);

    static const long num_cpu = sysconf( _SC_NPROCESSORS_ONLN );
    char cpu_buf[4];
    snprintf(cpu_buf, sizeof cpu_buf, "%ld", num_cpu);
    arguments->addValueToObject("cpu_count", cpu_buf, JT::Token::Number);
    JT::ObjectNode *env_variables = m_build_environment.copyEnvironmentTree();
    arguments->insertNode(std::string("environment"), env_variables);

    ProcessBuilder processBuilder(m_configuration);
    processBuilder.project_name = project_name;
    processBuilder.fallback = project_node->stringAt("arguments.build_system");

    if (m_configuration.clean()) {
        Process process = processBuilder.build();
        process.setPhase("clean");
        process.setProjectNode(project_node, &m_build_environment);
        process.setPrint(false);
        if (!process.run())
            return false;
    }

    if (m_configuration.deepClean() && project_node->objectNodeAt("scm")) {
        std::string scm_type = project_node->stringAt("scm.type");
        if (scm_type.size() == 0) {
            scm_type = "regular";
        }

        ResetPath resetPath;
        (void) resetPath;

        if (project_build_path.size() && project_build_path != project_src_path) {
            if (!Configuration::removeRecursive(project_build_path.c_str())) {
                fprintf(stderr, "Failed to remove build dir %s\n", project_build_path.c_str());
                m_error = true;
                return false;
            }
            Configuration::ensurePath(project_build_path);
        }

        if (project_src_path.size()) {
            if (chdir(project_src_path.c_str())) {
                fprintf(stderr, "Failed to change to source directory. Deep clean failed%s\n", strerror(errno));
                m_error = true;
                return false;
            } else {
                PhaseReporter reporter("deep-clean", project_name);
                Process process = processBuilder.build();
                process.setPhase("deep_clean");
                process.setFallback(scm_type);
                process.setProjectNode(project_node, &m_build_environment);
                process.setPrint(true);
                if (!process.run()) {
                    return false;
                }
                reporter.markSuccess();
            }
        }
    }

    if (should_chdir_to_build) {
        chdir(project_build_path.c_str());
    } else {
        chdir(m_configuration.buildDir().c_str());
    }

    {
        Process process(m_configuration);
        process.setPhase("pre_build");
        process.setProjectName(project_name);
        process.setProjectNode(project_node, &m_build_environment);
        process.setPrint(true);
        process.setScriptHasToExist(false);
        if (!process.run()) {
            fprintf(stderr, "Failed to run process\n");
            return false;
        }
    }

    return true;
}

bool BuildAction::handleBuildForProject(const std::string &projectName, const std::string &buildSystem, JT::ObjectNode *projectNode)
{
    TempFile temp_file(projectName + "_env");
    EnvScriptBuilder env_script_builder(m_configuration, m_build_environment, m_buildset_tree);
    env_script_builder.setToProject(projectName);
    env_script_builder.writeSetScript(temp_file);
    temp_file.close();

    ProcessBuilder processBuilder(m_configuration);
    processBuilder.project_name = projectName;
    processBuilder.env_script = temp_file.name();
    processBuilder.fallback = buildSystem;

    std::unique_ptr<JT::ObjectNode> temp_pointer(nullptr);
    JT::ObjectNode *project_node = projectNode;

    if (m_configuration.configure()) {
        PhaseReporter reporter("configure", projectName);
        Process process = processBuilder.build();
        process.setPhase("configure");
        process.setProjectNode(project_node, &m_build_environment);
        process.setPrint(true);
        if (!process.run()) {
            return false;
        }
        reporter.markSuccess();
    }

    if (m_configuration.build()) {
        {
            PhaseReporter reporter("build", projectName);
            Process process = processBuilder.build();
            process.setPhase("build");
            process.setProjectNode(project_node, &m_build_environment);
            process.setPrint(false);
            if (!process.run()) {
                return false;
            }
            reporter.markSuccess();
        }
        if (m_configuration.install() && project_node->nodeAt("no_install") == nullptr) {
            PhaseReporter reporter("install", projectName);
            Process process = processBuilder.build();
            process.setPhase("install");
            process.setProjectNode(project_node, &m_build_environment);
            process.setPrint(false);
            if (!process.run()) {
                return false;
            }
            reporter.markSuccess();
        }
    }

   return true;
}


