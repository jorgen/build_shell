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
#include "generate_action.h"
#include "buildset_generator.h"

#include <sys/types.h>
#include <sys/dir.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <libgen.h>

#include "json_tree.h"
#include "tree_writer.h"
#include "process.h"

GenerateAction::GenerateAction(const Configuration &configuration)
    : Action(configuration)
{
}


GenerateAction::~GenerateAction()
{
}

class LogFileHandler
{
public:
    LogFileHandler(const std::string &logFile)
        : error(false)
    {
        mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH;
        log_file = open(logFile.c_str(), O_WRONLY|O_APPEND|O_CREAT|O_TRUNC|O_CLOEXEC, mode);
        if (log_file < 0) {
            fprintf(stderr, "Failed to open file %s for stderr redirection %s\n", logFile.c_str(),
                    strerror(errno));
            error = true;;
        }
    }

    ~LogFileHandler()
    {
        if (!error) {
            close(log_file);
        }
    }

    int log_file;
    bool error;
};

bool GenerateAction::execute()
{
    TreeBuilder tree_builder(m_configuration.buildsetFile());
    tree_builder.load();

    std::unique_ptr<JT::ObjectNode> out_tree(tree_builder.takeRootNode());

    if (!out_tree)
        out_tree.reset(new JT::ObjectNode());

    BuildsetGenerator generator(m_configuration);
    if (!generator.updateBuildsetNode(out_tree.get())) {
        fprintf(stderr, "Failed to update buildset node\n");
        m_error = true;
        return false;
    }

    std::string out_file_name;
    std::string out_file_desc_name;
    int out_file = STDOUT_FILENO;

    if (m_configuration.buildsetOutFile().size()) {
        out_file_name = m_configuration.buildsetOutFile();
        out_file_desc_name = out_file_name + ".tmp";
        mode_t create_mode = S_IROTH | S_IRGRP | S_IRUSR | S_IWUSR;
        out_file = open(out_file_desc_name.c_str(), O_RDWR | O_CLOEXEC | O_CREAT | O_TRUNC, create_mode);
        if (out_file < 0) {
            fprintf(stderr, "Failed to open buildset Out File %s\n%s\n",
                    out_file_name.c_str(), strerror(errno));
            m_error = true;
            return true;
        }
    }

    TreeWriter tree_writer(out_file);
    tree_writer.write(out_tree.get());
    if (tree_writer.error()) {
        m_error = true;
        return false;
    }

    if (out_file_desc_name.size()) {
        close(out_file);
        if (!error()) {
            if (rename(out_file_desc_name.c_str(), out_file_name.c_str())) {
                fprintf(stderr, "Failed to rename generated buildset %s to %s : %s\n",
                        out_file_desc_name.c_str(), out_file_name.c_str(),
                        strerror(errno));
            }
        } //dont unlink as the failed generated file can be used for debugging
    }
    return true;
}


