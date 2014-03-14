/*
 * Copyright © 2014 Jørgen Lind

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
#include "arg.h"

#include "optionparser.h"
#include "buildset_converter.h"

#include <vector>
#include <iostream>

#include <unistd.h>
enum optionIndex {
    UNKNOWN,
    HELP,
};

const option::Descriptor usage[] =
 {
  {UNKNOWN,       0,  "", "",                 Arg::unknown,              "USAGE: buildset_convert [options] file\n\n"
                                                                         "Output is allways going to stdout \n\n"
                                                                         "Options:" },
  {HELP,          0, "h", "help",             option::Arg::None,         "  --help, -?\t Print usage and exit." },
  {UNKNOWN, 0,"" ,  ""   ,                    option::Arg::None,         "\nExamples:\n"
                                                                         "  buildset_convert /some/file \n"},
  {0,0,0,0,0,0}
 };

int main(int argc, char **argv)
{
    argc-=(argc>0); argv+=(argc>0);
    option::Stats  stats(true, usage, argc, argv);
    std::vector<option::Option> options(stats.options_max);
    std::vector<option::Option> buffer(stats.buffer_max);
    option::Parser parser(true, usage, argc, argv, options.data(), buffer.data());

    if (parser.error()) {
        return 1;
    }

    if (options[HELP]) {
        option::printUsage(std::cout, usage);
        return 0;
    }

    if (parser.nonOptionsCount() != 1) {
        fprintf(stderr, "Its not leagal to not exactly specify on input file\n");
        option::printUsage(std::cout, usage);
        return 1;
    }

    for (int i = 0; i < parser.optionsCount(); ++i)
    {
        option::Option& opt = buffer[i];
        switch (opt.index())
        {
            case HELP:
                // not possible, because handled further above and exits the program
            case UNKNOWN:
                fprintf(stderr, "UNKNOWN!");
                // not possible because Arg::Unknown returns ARG_ILLEGAL
                // which aborts the parse with an error
                break;
        }
    }

    if (parser.nonOptionsCount() == 1) {
        int input_file_access = R_OK | F_OK;

        if (access(parser.nonOption(0),input_file_access)) {
            fprintf(stderr, "Error accessing input file\n%s\n\n", strerror(errno));
            option::printUsage(std::cout, usage);
            return 1;
        }
    }

    BuildsetConverter converter(parser.nonOption(0));
    converter.convert();

    return 0;
}
