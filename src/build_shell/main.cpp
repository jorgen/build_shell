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

#include "arg.h"
#include "configuration.h"
#include "create_action.h"
#include "pull_action.h"
#include "build_action.h"
#include "available_builds.h"

#include <vector>
#include <iostream>

#include "../3rdparty/optionparser/src/optionparser.h"

enum optionIndex {
    UNKNOWN,
    HELP,
    SRC_DIR,
    BUILD_DIR,
    INSTALL_DIR,
    BUILDSET,
    BUILDSET_OUT,
    RESET_SHA,
    BUILD_FROM,
    ONLY_ONE,
    CONTINUE,
    PULL_FIRST,
    CLEAN,
    DEEP_CLEAN,
    SKIP_CONFIGURE,
    NO_REGISTER
};

const option::Descriptor usage[] =
 {
  {UNKNOWN,       0,  "", "",                 Arg::unknown,                 "USAGE: build_shell [options] mode\n\n"
                                                                            "Modes\n"
                                                                            "  pull\t pulls sources specified in buildsetfile\n"
                                                                            "  create\t creates a new buildset file\n"
                                                                            "  build\t builds a buildset file\n\n"
                                                                            "Options:" },
  {HELP,          0, "h" , "help",            option::Arg::None,            "  --help, -h\tPrint usage and exit." },
  {SRC_DIR,       0, "s", "src-dir",          Arg::requiresArg,             "  --src-dir, -s  \tSource dir, where projects are cloned\v"
                                                                            "     and builds read their sources from." },
  {BUILD_DIR,     0, "b", "build-dir",        Arg::requiresArg,             "  --build-dir, -b     \tBuild dir, defaults to source dir. This is\v"
                                                                            "     where the projects will be built."},
  {INSTALL_DIR,   0, "i", "install-dir",      Arg::requiresArg,             "  --install-dir, -i   \tInstall dir, defaults to build dir. This is the directory\v"
                                                                            "     that will be used as prefix for projects."},
  {BUILDSET,      0, "f", "buildset",         Arg::requiresExistingFile,    "  --buildset -f  \tFile used as input for projects"},
  {BUILDSET_OUT,  0, "o", "buildset-out",     Arg::requiresNonExistingFile, "  --buildset-out -o  \tFile used for creating buildset file"},
  {RESET_SHA,     0, "" , "reset-sha",        option::Arg::None,            "  --reset-sha \tIn pull mode reset to sha"},
  {BUILD_FROM,    0, "" , "from",             Arg::requiresArg,             "  --from \tDoesn't perform the build steps for projectes before the mentioned project"},
  {ONLY_ONE,      0, "" , "only-one",         option::Arg::None,            "  --only-one \tOnly build one project"},
  {CONTINUE,      0, "" , "continue",         option::Arg::None,            "  --continue \tDisables only-one\n"
                                                                            "             \t    This is usefull when using the --from option as the option enables"
                                                                            "             \t    --only-one by default"},
  {PULL_FIRST,    0, "" , "pull-first",       option::Arg::None,            "  --pull-first \tPull/Clone before building"},
  {CLEAN,         0, "" , "clean",            option::Arg::None,            "  --clean \tRun clean scripts"},
  {DEEP_CLEAN,    0, "" , "deep-clean",       option::Arg::None,            "  --deep-clean\tRemove build directory if differentg from source dir\n"
                                                                            "              \t    and run deep clean script on source. This option discard clean"},
  {SKIP_CONFIGURE,0, "" , "skip-configure",   option::Arg::None,            "  --skip-configure \tSkipping the configure step when building"},
  {NO_REGISTER,   0, "" , "no-register",      option::Arg::None,            "  --no-register    \tDon't register the build"},

  {UNKNOWN, 0,"" ,  ""   ,                    option::Arg::None,            "\nExamples:\n"
                                                                            "  build_shell --src-dir /some/file -f ../some/buildset_file pull\n"},
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

    if (options[HELP] || argc == 0) {
        option::printUsage(std::cout, usage);
        return 0;
    }

    Configuration configuration;

    if (parser.nonOptionsCount() == 1) {
        std::string mode = parser.nonOption(0);
        if (mode == "pull") {
            configuration.setMode(Configuration::Pull, mode);
        } else if (mode == "create") {
            configuration.setMode(Configuration::Create, mode);
        } else if (mode == "build") {
            configuration.setMode(Configuration::Build, mode);
        } else if (mode == "rebuild") {
            configuration.setMode(Configuration::Rebuild, mode);
        } else if (mode == "available") {
            AvailableBuilds available(configuration);
            available.printAvailable();
            return 0;
        } else if (mode == "build_shell_dev_build") {
#ifdef JSONMOD_PATH
            return 0;
#endif
            return 1;
        }

    } else {
        fprintf(stderr, "\nFailed to recognize mode\n\n");
        option::printUsage(std::cerr, usage);
        return 1;
    }

    for (int i = 0; i < parser.optionsCount(); ++i)
    {
        option::Option& opt = buffer[i];
        switch (opt.index())
        {
            case HELP:
                // not possible, because handled further above and exits the program
            case SRC_DIR:
                configuration.setSrcDir(opt.arg);
                break;
            case BUILD_DIR:
                configuration.setBuildDir(opt.arg);
                break;
            case INSTALL_DIR:
                configuration.setInstallDir(opt.arg);
                break;
            case BUILDSET:
                configuration.setBuildsetFile(opt.arg);
                break;
            case BUILDSET_OUT:
                configuration.setBuildsetOutFile(opt.arg);
                break;
            case RESET_SHA:
                configuration.setResetToSha(true);
                break;
            case BUILD_FROM:
                configuration.setBuildFromProject(opt.arg);
                break;
            case ONLY_ONE:
                configuration.setOnlyOne(true);
                break;
            case CONTINUE:
                configuration.setOnlyOne(false);
                break;
            case PULL_FIRST:
                configuration.setPullFirst(true);
                break;
            case CLEAN:
                configuration.setClean(true);
                break;
            case DEEP_CLEAN:
                configuration.setDeepClean(true);
                break;
            case SKIP_CONFIGURE:
                configuration.setConfigure(false);
                break;
            case NO_REGISTER:
                configuration.setRegisterBuild(false);
                break;
            case UNKNOWN:
                fprintf(stderr, "UNKNOWN!");
                // not possible because Arg::Unknown returns ARG_ILLEGAL
                // which aborts the parse with an error
                break;
        }
    }

    configuration.validate();

    Action *action;
    if (!configuration.sane()) {
        option::printUsage(std::cerr,usage);
        return 1;
    } else {
        switch (configuration.mode()) {
            case Configuration::Create:
                action = new CreateAction(configuration);
                break;
            case Configuration::Pull:
                action = new PullAction(configuration);
                break;
            case Configuration::Build:
                action = new BuildAction(configuration);
                break;
            default:
                action = new CreateAction(configuration);
                break;
        }
    }

    bool error = action->error();
    if (error) {
        fprintf(stderr, "Failure to initialize action object. Not executing\n");
    } else {
        error = !action->execute();
    }

    delete action;

    return error;
}
