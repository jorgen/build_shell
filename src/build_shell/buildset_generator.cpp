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

#include "buildset_generator.h"

#include "json_tree.h"
#include "process.h"
#include "reset_path.h"

#include <sys/types.h>
#include <sys/dir.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <libgen.h>

class LogFileHandler
{
public:
    LogFileHandler(const std::string &logFile)
        : error(false)
    {
        if (logFile.size()) {
            mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH;
            log_file = open(logFile.c_str(), O_WRONLY|O_APPEND|O_CREAT|O_TRUNC|O_CLOEXEC, mode);
            if (log_file < 0) {
                fprintf(stderr, "Failed to open file %s for stderr redirection %s\n", logFile.c_str(),
                        strerror(errno));
                error = true;;
            }
        } else {
            log_file = -1;
        }
    }

    ~LogFileHandler()
    {
        if (!error && log_file >= 0) {
            close(log_file);
        }
    }

    int log_file;
    bool error;
};

BuildsetGenerator::BuildsetGenerator(const Configuration &configuration)
    : m_configuration(configuration)
{
}

bool BuildsetGenerator::updateBuildsetNode(JT::ObjectNode *buildset)
{
    return updateBuildset(buildset);
}

JT::ObjectNode *BuildsetGenerator::createBuildsetNode()
{
    JT::ObjectNode *root = new JT::ObjectNode();
    if (updateBuildset(root))
        return root;
    delete root;
    return nullptr;
}

bool BuildsetGenerator::updateBuildset(JT::ObjectNode *buildset)
{
    std::string log_file;
    if (Configuration::isDir(m_configuration.scriptExecutionLogDir()))
        log_file = m_configuration.scriptExecutionLogDir() + "/buildset_creation.log";
    LogFileHandler log_file_handler(log_file);

    DIR *source_dir = opendir(m_configuration.srcDir().c_str());
    if (!source_dir) {
        fprintf(stderr, "Failed to open directory: %s\n%s\n",
                m_configuration.srcDir().c_str(), strerror(errno));
        return false;
    }

    ResetPath path;
    (void) path;

    if (chdir(m_configuration.srcDir().c_str()) != 0) {
        fprintf(stderr, "Could not change dir: %s\n%s\n",
                m_configuration.srcDir().c_str(),strerror(errno));
        return false;
    }

    while (struct dirent *ent = readdir(source_dir)) {
        if (strncmp(".",ent->d_name, sizeof(".")) == 0
                || strncmp("..", ent->d_name, sizeof("..")) == 0
                || strncmp("build_shell", ent->d_name, sizeof("build_shell")) == 0
                || strncmp("etc", ent->d_name, sizeof("etc")) == 0
                || strncmp("share", ent->d_name, sizeof("share")) == 0
                || strncmp("include", ent->d_name, sizeof("include")) == 0
                || strncmp("libexec", ent->d_name, sizeof("libexec")) == 0
                || strncmp("lib", ent->d_name, sizeof("lib")) == 0
                || strncmp("bin", ent->d_name, sizeof("bin")) == 0
                || strncmp("var", ent->d_name, sizeof("var")) == 0)
            continue;
        struct stat buf;
        if (stat(ent->d_name, &buf) != 0) {
            fprintf(stderr, "Something whent wrong when stating file %s: %s\n",
                    ent->d_name, strerror(errno));
            continue;
        }
        if (S_ISDIR(buf.st_mode)) {
            if (chdir(ent->d_name)) {
                fprintf(stderr, "Failed to change into directory %s\n%s\n", ent->d_name, strerror(errno));
                return false;
            }
            if (!handleCurrentSrcDir(buildset, log_file_handler.log_file)) {
                fprintf(stderr, "Failed to handle dir %s\n", ent->d_name);
                closedir(source_dir);
                return false;
            }
            chdir(m_configuration.srcDir().c_str());
        }
    }
    closedir(source_dir);

    return true;
}

bool BuildsetGenerator::handleCurrentSrcDir(JT::ObjectNode *buildset, int log_file)
{
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    std::string base_name = basename(cwd);
    if (log_file >= 0) {
        std::string log = std::string() +
            "***********************************************************\n"
            "\tLog for " + cwd + "\n"
            "***********************************************************\n"
            "\n";
        write(log_file, log.c_str(), log.size());
    }

    JT::ObjectNode *root_for_dir = buildset->objectNodeAt(base_name);
    if (!root_for_dir) {
        root_for_dir = new JT::ObjectNode();
    }

    JT::ObjectNode *scm_node = root_for_dir->objectNodeAt("scm");
    if (!scm_node) {
        JT::Property prop(std::string("scm"));
        root_for_dir->insertNode(prop, new JT::ObjectNode(), true);
    }

    std::string postfix;
    Configuration::ScmType scm_type = Configuration::findScmForCurrentDirectory();
    if (scm_type == Configuration::NotRecognizedScmType)
        postfix = "regular_dir";
    else
        postfix = Configuration::ScmTypeStringMap[scm_type];

    JT::ObjectNode *updated_node;
    bool script_success;
    {
        Process process(m_configuration);
        process.setPhase("generate");
        process.setProjectName(base_name);
        process.setFallback(postfix);
        process.setLogFile(log_file, false);
        process.setProjectNode(root_for_dir);
        process.setPrint(true);
        script_success = process.run(&updated_node);
    }

    if (!updated_node) {
        updated_node = new JT::ObjectNode();
    }

    buildset->insertNode(base_name, updated_node, true);

    return script_success;
}
