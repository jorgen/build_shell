#include "arg.h"
#include "configuration.h"

#include <iostream>
#include <optionparser.h>

enum optionIndex {
    UNKNOWN,
    HELP,
    SRC_DIR,
    BUILD_DIR,
    INSTALL_DIR,
    BUILDSET_FILE
};

const option::Descriptor usage[] =
 {
  {UNKNOWN,       0,  "", "",              Arg::unknown,              "USAGE: build_shell [options] mode\n\n"
                                                                      "Modes\n"
                                                                      "  pull\t pulls sources specified in buildsetfile\n"
                                                                      "  create\t creates a new buildset file\n"
                                                                      "  build\t builds a buildset file\n\n"
                                                                      "Options:" },
  {HELP,          0, "h" , "help",         option::Arg::None,         "  --help, -h\tPrint usage and exit." },
  {SRC_DIR,       0, "s", "src-dir",       Arg::requiresDirectory,    "  --src-dir, -s  \tSource dir, where projects are cloned\v"
                                                                      "     and builds read their sources from." },
  {BUILD_DIR,     0, "b", "build-dir",     Arg::requiresDirectory,    "  --build-dir, -b     \tBuild dir, defaults to source dir. This is\v"
                                                                      "     where the projects will be built."},
  {INSTALL_DIR,   0, "i", "install-dir",   Arg::requiresDirectory,    "  --install-dir, -i   \tInstall dir, defaults to build dir. This is the directory\v"
                                                                      "     that will be used as prefix for projects."},
  {BUILDSET_FILE, 0, "f", "buildset-file", Arg::requiresExistingFile, "  --buildset-file -f  \tFile used as input for projects"},

  {UNKNOWN, 0,"" ,  ""   ,                 option::Arg::None,         "\nExamples:\n"
                                                                      "  build_shell --src-dir /some/file -f ../some/buildset_file pull\n"},
  {0,0,0,0,0,0}
 };

int main(int argc, char **argv)
{
    argc-=(argc>0); argv+=(argc>0);
    option::Stats  stats(usage, argc, argv);
    fprintf(stderr, "MAX BUFFER %d:%d\n", stats.options_max, stats.buffer_max);
    option::Option options[stats.options_max], buffer[stats.buffer_max];
    option::Parser parser(usage, argc, argv, options, buffer);

    if (parser.error()) {
        return 1;
    }

    if (options[HELP] || argc == 0) {
        option::printUsage(std::cout, usage);
        return 0;
    }

    if (!options[SRC_DIR]) {
        fprintf(stderr, "\nPlease specify the required argument src-dir\n");
        option::printUsage(std::cerr, usage);
        return 1;
    }

    if (!options[BUILDSET_FILE]) {
        fprintf(stderr, "\nPlease specify the required argument buildset-file\n");
        option::printUsage(std::cerr, usage);
        return 1;
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
            case BUILDSET_FILE:
                configuration.setBuildsetFile(opt.arg);
                break;
            case UNKNOWN:
                fprintf(stderr, "UNKNOWN!");
                // not possible because Arg::Unknown returns ARG_ILLEGAL
                // which aborts the parse with an error
                break;
        }
    }

    configuration.validate();

    if (!configuration.sane()) {
        option::printUsage(std::cerr,usage);
        return 1;
    } else {
        fprintf(stderr, "IT IS SANE!!!!!!!!!!!\n");
    }


    return 0;
}
