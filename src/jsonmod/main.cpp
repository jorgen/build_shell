#include "arg.h"
#include "configuration.h"

#include <iostream>
#include <vector>
#include "../3rdparty/optionparser/src/optionparser.h"

enum optionIndex {
    UNKNOWN,
    HELP,
    FILE_SRC,
    INLINE,
    PROPERTY,
    HUMAN,
    VALUE
};

const option::Descriptor usage[] =
 {
  {UNKNOWN,       0,  "", "",                 Arg::unknown,              "USAGE: jsonmod [options][file]\n\n"
                                                                         "\n"
                                                                         "If file is omitted, then input is read from standard in\n\n"
                                                                         "Options:" },
  {HELP,          0, "?", "help",             option::Arg::None,         "  --help, -?\tPrint usage and exit." },
  {INLINE,        0, "i", "inline",           option::Arg::None,         "  --inline, -i  \tInline modifies the input file." },
  {PROPERTY,      0, "p", "property",         Arg::requiresValue,        "  --property, -p \tProperty to retrun/modify." },
  {VALUE,         0, "v", "value",            Arg::requiresValue,        "  --value, -v\tValue to update property with."},
  {HUMAN,         0, "h", "pretty",           option::Arg::None,         "  --pretty, -h\tOutput in human readable format."},
  {UNKNOWN, 0,"" ,  ""   ,                    option::Arg::None,         "\nExamples:\n"
                                                                         "  jsonmod -i -p \"foo\" -v 43, /some/file \n"},
  {0,0,0,0,0,0}
 };

int main(int argc, char **argv)
{
    argc-=(argc>0); argv+=(argc>0);
    option::Stats  stats(usage, argc, argv);
    std::vector<option::Option> options(stats.options_max);
    std::vector<option::Option> buffer(stats.buffer_max);
    option::Parser parser(usage, argc, argv, options.data(), buffer.data());

    if (parser.error()) {
        return 1;
    }

    if (options[HELP] || argc == 0) {
        option::printUsage(std::cout, usage);
        return 0;
    }

    for (int i = 0; i < parser.nonOptionsCount(); ++i)
             std::cout << "Non-option #" << i << ": " << parser.nonOption(i) << "\n";
    if (parser.nonOptionsCount() > 0) {

    }

    Configuration configuration;

    if (parser.nonOptionsCount() == 1) {
        configuration.setInputFile(parser.nonOption(0));
    }
    for (int i = 0; i < parser.optionsCount(); ++i)
    {
        option::Option& opt = buffer[i];
        switch (opt.index())
        {
            case HELP:
                // not possible, because handled further above and exits the program
            case FILE_SRC:
                configuration.setInputFile(opt.arg);
                break;
            case INLINE:
                configuration.setInline(true);
                break;
            case PROPERTY:
                configuration.setProperty(opt.arg);
                break;
            case HUMAN:
                configuration.setShouldPrettyPrint(true);
                break;
            case VALUE:
                configuration.setValue(opt.arg);
                break;
            case UNKNOWN:
                fprintf(stderr, "UNKNOWN!");
                // not possible because Arg::Unknown returns ARG_ILLEGAL
                // which aborts the parse with an error
                break;
        }
    }

    if (!configuration.sane()) {
        option::printUsage(std::cerr,usage);
        return 1;
    } else {
        fprintf(stderr, "IT IS SANE!!!!!!!!!!!\n");
    }

    return 0;
}
