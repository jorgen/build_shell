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
#include "child_process_io_handler.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <poll.h>

#include <vector>

ChildProcessIoHandler::ChildProcessIoHandler(const std::string &out_file)
    : m_out_file(-1)
    , m_stdout_pipe{-1,-1}
    , m_stderr_pipe{-1,-1}
    , m_error(true)
    , m_print_stdout(false)
    , m_print_stderr(true)
{
    if (!out_file.size())
        return;

    if (::pipe(m_stderr_pipe)) {
        fprintf(stderr, "Failed to open pipe for stderr redirection %s\n", strerror(errno));
        return;
    }

    if (::pipe(m_stdout_pipe)) {
        fprintf(stderr, "Failed to open pipe for stdout redirection %s\n", strerror(errno));
        return;
    }

    mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH;
    m_out_file = open(out_file.c_str(), O_WRONLY|O_APPEND|O_CREAT|O_TRUNC|O_CLOEXEC, mode);
    if (m_out_file < 0) {
        fprintf(stderr, "Failed to open file %s for stderr redirection %s\n", out_file.c_str(),
                strerror(errno));
        return;
    }

    m_error = false;
}

ChildProcessIoHandler::~ChildProcessIoHandler()
{
    if (m_stderr_pipe[1] >= 0) {
        close(m_stderr_pipe[1]);
    }
    if (m_stdout_pipe[1] >= 0) {
        close(m_stdout_pipe[1]);
    }

    m_thread.join();
}

void ChildProcessIoHandler::setupMasterProcessState()
{
    close(m_stderr_pipe[1]);
    close(m_stdout_pipe[1]);

    m_thread = std::thread(&ChildProcessIoHandler::run,this);
}

void ChildProcessIoHandler::setupChildProcessState()
{
    close(m_stdout_pipe[0]);
    close(STDOUT_FILENO);
    dup2(m_stdout_pipe[1], STDOUT_FILENO);

    close(m_stderr_pipe[0]);
    close(STDERR_FILENO);
    dup2(m_stderr_pipe[1], STDERR_FILENO);
}

static bool flushToFile(int file, char *buffer, ssize_t size)
{
    ssize_t written = 0;
    while (written < size) {
        ssize_t w = write(file, buffer, size);
        if (w < 0) {
            return false;
        }
        written+=w;
    }
    return true;
}

static void handle_events(const pollfd &poll_data, int out_file, bool print, int *active_connections)
{
    if (!poll_data.revents) {
        return;
    }
    if (poll_data.revents & POLLHUP) {
        (*active_connections)--;
    }

    if (poll_data.revents & POLLIN) {
        char in_buffer[1024];
        ssize_t r = read(poll_data.fd, in_buffer, sizeof in_buffer);

        if (r == 0) {
            (*active_connections)--;

        }
        if (!flushToFile(out_file, in_buffer, r))
            fprintf(stderr, "Failed to write to out_file %s\n", strerror(errno));
        if (print) {
            if (!flushToFile(STDOUT_FILENO, in_buffer, r))
                fprintf(stderr, "Failed to write to stderr %s\n", strerror(errno));
        }
    }
}

void ChildProcessIoHandler::run()
{
    pollfd poll_data[2];
    poll_data[0].fd = m_stdout_pipe[0];
    poll_data[0].events = POLLHUP | POLLIN | POLLRDNORM;
    poll_data[1].fd = m_stderr_pipe[0];
    poll_data[1].events = POLLHUP | POLLIN | POLLRDNORM;

    int active_connections = 2;

    while (active_connections > 0) {
        int poll_return = poll(poll_data, 2, -1);
        if (poll_return < 0) {
            fprintf(stderr, "ChildProcessIoHandler poll failed %s\n", strerror(errno));
            continue;
        } else {
            handle_events(poll_data[0], m_out_file, m_print_stdout, &active_connections);
            handle_events(poll_data[1], m_out_file, m_print_stderr, &active_connections);
        }
    }
}
