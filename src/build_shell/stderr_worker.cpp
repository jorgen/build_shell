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
#include "stderr_worker.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <poll.h>


StdErrWorker::StdErrWorker(const std::string &out_file)
    : m_out_file(-1)
{
    m_stderr_pipe[0] = -1;
    m_stderr_pipe[1] = -1;
    m_stdout_pipe[0] = -1;
    m_stdout_pipe[1] = -1;

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
    m_out_file = open(out_file.c_str(), O_WRONLY|O_APPEND|O_CREAT|O_CLOEXEC, mode);
    if (m_out_file < 0) {
        fprintf(stderr, "Failed to open file %s for stderr redirection %s\n", out_file.c_str(),
                strerror(errno));
        return;
    }
}

void StdErrWorker::processPipe()
{
    m_thread = std::thread(&StdErrWorker::run,this);
}

void StdErrWorker::join()
{
    if (m_stderr_pipe[1] >= 0) {
        close(m_stderr_pipe[1]);
    }
    if (m_stdout_pipe[1] >= 0) {
        close(m_stdout_pipe[1]);
    }

    m_thread.join();
}

void StdErrWorker::closeStdOutReadPipe()
{
    if (m_stdout_pipe[0] >= 0) {
        close(m_stdout_pipe[0]);
        m_stdout_pipe[0] = -1;
    }
}

void StdErrWorker::closeStdOutWritePipe()
{
    if (m_stdout_pipe[1] >= 0) {
        close(m_stdout_pipe[1]);
        m_stdout_pipe[1] = -1;
    }
}

void StdErrWorker::closeStdErrReadPipe()
{
    if (m_stderr_pipe[0] >= 0) {
        close(m_stderr_pipe[0]);
        m_stderr_pipe[0] = -1;
    }
}

void StdErrWorker::closeStdErrWritePipe()
{
    if (m_stderr_pipe[1] >= 0) {
        close(m_stderr_pipe[1]);
        m_stderr_pipe[1] = -1;
    }
}

void StdErrWorker::setupRedirectStdOut()
{
    close(STDOUT_FILENO);
    dup2(m_stdout_pipe[1], STDOUT_FILENO);
}

void StdErrWorker::setupRedirectStdErr()
{
    close(STDERR_FILENO);
    dup2(m_stdout_pipe[1], STDOUT_FILENO);
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

class PollListener
{
    PollListener(bool streamToStdOut, int inPipe, int outFile)
        : m_stream_stdout(streamToStdOut)
        , m_in_pipe(inPipe)
        , m_out_file(outFile)
    { }

    void handle_events(short events, int *active_connections)
    {
        if (events & POLLHUP) {
            (*active_connections)--;
        }

        if (events & POLLIN) {
            char in_buffer[1024];
            ssize_t r = read(m_in_pipe, in_buffer, sizeof in_buffer);

            if (!flushToFile(m_out_file, in_buffer, r))
                fprintf(stderr, "Failed to write to out_file %s\n", strerror(errno));
            if (m_stream_stdout) {
                if (!flushToFile(STDERR_FILENO, in_buffer, r))
                    fprintf(stderr, "Failed to write to stderr %s\n", strerror(errno));
            }
        }
    }

private:
    bool m_stream_stdout;
    int m_in_pipe;
    int m_out_file;
};

void StdErrWorker::run()
{
    pollfd poll_data[2];
    //PollListener pollListener[] = {PollListener(false,m_stdout_pipe
    poll_data[0].fd = m_stdout_pipe[0];
    poll_data[0].events = POLLHUP | POLLIN;
    poll_data[1].fd = m_stdout_pipe[0];
    poll_data[1].events = POLLHUP | POLLIN;

    bool stdout_pipe_closed = false;
    bool stderr_pipe_closed = false;

    while (!stdout_pipe_closed || !stderr_pipe_closed) {
        int poll_return = poll(poll_data, sizeof poll_data, -1);
        if (poll_return < 0) {
            fprintf(stderr, "StdErrWorker poll failed %s\n", strerror(errno));
            continue;
        } else {
        }
    }
}
