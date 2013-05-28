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
#ifndef CHILD_PROCESS_IO_HANDLER_H
#define CHILD_PROCESS_IO_HANDLER_H

#include <string>
#include <thread>

#include <poll.h>

class ChildProcessIoHandler
{
public:
    ChildProcessIoHandler(int out_file);
    ~ChildProcessIoHandler();

    void setupMasterProcessState();
    void setupChildProcessState();

    bool error() const { return m_error; }

    void printStdOut(bool print);
private:
    bool handle_events(const pollfd &poll_data,
                       int out_file,
                       bool print,
                       int *active_connections) const;
    void run();
    int m_out_file;
    int m_stdout_pipe[2];
    int m_stderr_pipe[2];
    bool m_error;
    bool m_print_stdout;
    bool m_print_stderr;
    std::thread m_thread;
    int m_roller_state;
    int m_rooler_active;
};

#endif

