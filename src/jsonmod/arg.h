#ifndef BUILD_SHELL_ARG_H
#define BUILD_SHELL_ARG_H

#include "../3rdparty/optionparser/src/optionparser.h"

class Arg
{
public:

    static void printError(const char* msg1, const option::Option& opt, const char* msg2);
    static option::ArgStatus requiresValue(const option::Option &option, bool msg);
    static option::ArgStatus requiresExistingFile(const option::Option &option, bool msg);
    static option::ArgStatus requiresNonExistingFile(const option::Option &option, bool msg);
    static option::ArgStatus unknown(const option::Option &option, bool msg);
};

#endif //BUILD_SHELL_ARG_H
