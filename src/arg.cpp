#include "arg.h"

#include <iostream>
#include <stdio.h>

#include <sys/stat.h>

void Arg::printError(const char* msg1, const option::Option& opt, const char* msg2)
{
  fprintf(stderr, "%s", msg1);
  fwrite(opt.name, opt.namelen, sizeof(char), stderr);
  fprintf(stderr, "%s", msg2);

}

option::ArgStatus Arg::requiresDirectory(const option::Option &option, bool msg)
{
    //FIXME: validate that path is a valid path
    if (option.arg != 0)
        return option::ARG_OK;

    if (msg)
        printError("Option '", option, "' requires a valid directory path\n");

    return option::ARG_ILLEGAL;
}

option::ArgStatus Arg::requiresExistingFile(const option::Option &option, bool msg)
{
    struct stat stat_buf;
    stat(option.arg, &stat_buf);

    if (S_ISREG(stat_buf.st_mode)) {
        return option::ARG_OK;
    }

    if (msg)
        printError("Option '", option, "' requires an existing path\n");

    return option::ARG_ILLEGAL;
}

option::ArgStatus Arg::unknown(const option::Option &option, bool msg)
{
    if (msg)
        printError("Unknown option '", option, "'\n");
    return option::ARG_ILLEGAL;
}
