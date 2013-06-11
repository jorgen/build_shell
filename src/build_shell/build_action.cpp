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
#include "generate_action.h"
#include "available_builds.h"
#include "temp_file.h"
#include "pull_action.h"
#include "process.h"

#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <fcntl.h>
#include <time.h>

#include <memory>
#include <algorithm>
#include <string>

BuildAction::BuildAction(const Configuration &configuration)
    : CreateAction(configuration, false)
    , m_transformer_state(m_build_environment)
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

    m_token_transformer = std::bind(&BuildAction::token_transformer, this, std::placeholders::_1);
}


BuildAction::~BuildAction()
{
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
    snprintf(&dateInFormat[0], 20,"%04d-%02d-%02d-%02d:%02d:%02d",
             1900 + local_tm->tm_year, local_tm->tm_mon+1, local_tm->tm_mday,
             local_tm->tm_hour, local_tm->tm_min, local_tm->tm_sec);
    m_stored_buildset = build_shell_build_sets_dir + "/" + dateInFormat.c_str() + std::string(".buildset");
    GenerateAction generate_action(m_configuration, m_stored_buildset);
    m_error = !generate_action.execute();
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
    const std::string &m_project;
    bool m_success;
};

bool BuildAction::execute()
{
    if (!m_buildset_tree || m_error)
        return false;

    ArgumentsCleanup argCleanup(m_buildset_tree);

    if (!handlePrebuild())
        return false;

    auto end_it = endIterator(m_buildset_tree);
    for (auto it = startIterator(m_buildset_tree); it != end_it; ++it) {
        JT::ObjectNode *project_node = it->second->asObjectNode();
        if (!project_node)
            continue;

        const std::string project_name = it->first.string();
        const std::string &project_build_path = project_node->stringAt("arguments.build_path");
        const std::string &project_build_system = project_node->stringAt("arguments.build_system");

        PhaseReporter reporter("build", project_name);
        if (chdir(project_build_path.c_str())) {
            fprintf(stderr, "Failed to move into directory %s to build\n", project_build_path.c_str());
            m_error = true;
            return false;
        }
        if (!handleBuildForProject(project_name, project_build_system, project_node)) {
            return false;
        }

        reporter.markSuccess();
        if (m_configuration.onlyOne())
            break;
    }

    end_it = endIterator(m_buildset_tree);
    for (auto it = startIterator(m_buildset_tree); it != end_it; ++it) {
        JT::ObjectNode *project_node = it->second->asObjectNode();
        if (!project_node)
            continue;

        const std::string project_name = it->first.string();
        const std::string &project_build_path = project_node->stringAt("arguments.build_path");
        const std::string &project_build_system = project_node->stringAt("arguments.build_system");

        PhaseReporter reporter("post_build", project_name);
        if (chdir(project_build_path.c_str())) {
            fprintf(stderr, "Failed to move into directory %s to do post build scripts\n", project_build_path.c_str());
            m_error = true;
            return false;
        }
        {
            Process process(m_configuration);
            process.setEnvironmentScript(m_set_build_env_file);
            process.setPhase("post_build");
            process.setProjectName(project_name);
            process.setFallback(project_build_system);
            process.setProjectNode(project_node);
            process.setPrint(true);
            process.registerTokenTransformer(m_token_transformer);
            m_transformer_state.current_project = project_name;
            if (!process.run()) {
                return false;
            }
        }

        reporter.markSuccess();
        if (m_configuration.onlyOne())
            break;
    }

    return true;
}

bool BuildAction::handlePrebuild()
{
    Process pre_build(m_configuration);
    pre_build.registerTokenTransformer(m_token_transformer);

    auto end_it = endIterator(m_buildset_tree);
    for (auto it = startIterator(m_buildset_tree); it != end_it; ++it) {
        JT::ObjectNode *project_node = it->second->asObjectNode();
        if (!project_node)
            continue;

        const std::string project_name = it->first.string();
        const std::string phase("pre_build");
        PhaseReporter reporter(phase, project_name);
        if (chdir(m_configuration.buildDir().c_str())) {
            fprintf(stderr, "Could not move into build dir:%s\n%s\n",
                    m_configuration.buildDir().c_str(), strerror(errno));
            m_error = true;
            return false;
        }

        std::string project_build_path = m_configuration.buildDir() + "/" + project_name;
        std::string project_src_path = m_configuration.srcDir() + "/" + project_name;
        m_transformer_state.current_project = project_name;

        if (access(project_src_path.c_str(), X_OK|R_OK)) {
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

        Configuration::BuildSystem build_system = Configuration::findBuildSystem(project_src_path.c_str());
        std::string build_system_string = Configuration::BuildSystemStringMap[build_system];
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
                    m_error = true;
                    return false;
                }
        }

        JT::ObjectNode *arguments = new JT::ObjectNode();
        project_node->insertNode(std::string("arguments"), arguments, true);
        arguments->addValueToObject("src_path", project_src_path, JT::Token::String);
        arguments->addValueToObject("build_path", project_build_path, JT::Token::String);
        arguments->addValueToObject("install_path", m_configuration.installDir(), JT::Token::String);
        arguments->addValueToObject("build_system", build_system_string, JT::Token::String);

        static const long num_cpu = sysconf( _SC_NPROCESSORS_ONLN );
        char cpu_buf[4];
        snprintf(cpu_buf, sizeof cpu_buf, "%ld", num_cpu);
        arguments->addValueToObject("cpu_count", cpu_buf, JT::Token::Number);
        JT::ObjectNode *env_variables = new JT::ObjectNode();
        arguments->insertNode(std::string("environment"), env_variables);

        chdir(project_build_path.c_str());

        {
            Process process(m_configuration);
            process.setPhase(phase);
            process.setProjectName(project_name);
            process.setProjectNode(project_node);
            process.setPrint(true);
            process.registerTokenTransformer(m_token_transformer);
            if (!process.run()) {
                fprintf(stderr, "Failed to run process\n");
                return false;
            }
        }

        reporter.markSuccess();

        if (m_configuration.onlyOne())
            break;
    }
    return true;
}

bool BuildAction::handleBuildForProject(const std::string &projectName, const std::string &buildSystem, JT::ObjectNode *projectNode)
{
    fprintf(stderr, "Processing buildstep for %s\n", projectName.c_str());
    TempFile temp_file(projectName + "_env");
    m_env_script_builder.writeSetScript(temp_file, projectName);
    temp_file.close();

    Process process(m_configuration);
    process.setEnvironmentScript(temp_file.name());
    process.setProjectName(projectName);
    process.setFallback(buildSystem);
    process.setLogFile(projectName + "_build.log", false);
    process.registerTokenTransformer(m_token_transformer);

    m_transformer_state.current_project = projectName;

    std::unique_ptr<JT::ObjectNode> temp_pointer(nullptr);
    JT::ObjectNode *project_node = projectNode;

    if (m_configuration.clean()) {
        process.setPhase("clean");
        process.setProjectNode(project_node);
        process.setPrint(false);
        if (!process.run())
            return false;
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
                fprintf(stderr, "Failed to change to source directory. Deep clean failed%s\n", strerror(errno));
                m_error = true;
                return false;
            } else {
                process.setPhase("deep_clean");
                process.setProjectNode(project_node);
                process.setPrint(false);
                if (!process.run()) {
                    return false;
                }
            }
        }

        chdir(cwd.c_str());
    }

    if (m_configuration.configure()) {
        process.setPhase("configure");
        process.setProjectNode(project_node);
        process.setPrint(true);
        if (!process.run()) {
            return false;
        }
    }

    if (m_configuration.build()) {
        process.setPhase("build");
        process.setProjectNode(project_node);
        process.setPrint(false);
        if (!process.run()) {
            return false;
        }

        if (m_configuration.install() && project_node->nodeAt("no_install") == nullptr) {
            process.setPhase("install");
            process.setProjectNode(project_node);
            process.setPrint(false);
            if (!process.run()) {
                return false;
            }
        }
    }

   return true;
}

const JT::Token &BuildAction::token_transformer(const JT::Token &next_token)
{
    if (BuildEnvironment::findVariables(next_token.value.data, next_token.value.size).size()) {
        m_transformer_state.cacheAndExpandToken(next_token);
        return m_transformer_state.temp_token;
    }

    return next_token;
}

