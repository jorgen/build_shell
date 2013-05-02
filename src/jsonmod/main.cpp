#include "arg.h"
#include "configuration.h"
#include "json_streamer.h"

#include <iostream>
#include <vector>
#include "../3rdparty/optionparser/src/optionparser.h"

enum optionIndex {
    UNKNOWN,
    HELP,
    FILE_SRC,
    INLINE,
    PROPERTY,
    COMPACT,
    VALUE
};

const option::Descriptor usage[] =
 {
  {UNKNOWN,       0,  "", "",                 Arg::unknown,              "USAGE: jsonmod [options][file]\n\n"
                                                                         "If file is omitted, then input is read from stdin\n"
                                                                         "Output is allways going to stdout if not the inline option is specified\n\n"
                                                                         "Options:" },
  {HELP,          0, "h", "help",             option::Arg::None,         "  --help, -?\tPrint usage and exit." },
  {INLINE,        0, "i", "inline",           option::Arg::None,         "  --inline, -i  \tInline modifies the input file." },
  {PROPERTY,      0, "p", "property",         Arg::requiresValue,        "  --property, -p \tProperty to retrun/modify." },
  {VALUE,         0, "v", "value",            Arg::requiresValue,        "  --value, -v\tValue to update property with."},
  {COMPACT,       0, "c", "compact",          option::Arg::None,         "  --compact, -h\tOutput in compact format."},
  {UNKNOWN, 0,"" ,  ""   ,                    option::Arg::None,         "\nExamples:\n"
                                                                         "  jsonmod -i -p \"foo\" -v 43, /some/file \n"},
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

    Configuration configuration;

    if (parser.nonOptionsCount() > 1) {
        fprintf(stderr, "Its not leagal to specify more than one input file\n");
        return 1;
    }

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
            case COMPACT:
                configuration.setCompactPrint(true);
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
        JsonStreamer streamer(configuration);
        if (streamer.error())
            return 2;
        streamer.stream();
        if (streamer.error())
            return 3;
    }

    return 0;
}
