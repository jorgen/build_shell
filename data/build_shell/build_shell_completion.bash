#!/bin/bash

bs()
{
    echo $@
}

bss()
{
    echo select $@
}

_build_shell_select()
{
    local cur opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    if [[ ${cur} != -* ]] ; then
        opts=$(build_shell available)
        COMPREPLY=( $(compgen -W "${opts}" ${cur}) )
        return 0
    fi
}

_build_shell()
{
    local cur prev opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

}

complete -F _build_shell bs
complete -F _build_shell_select bss

REAL_SCRIPT_FILE=${BASH_SOURCE[0]}
if [ -L ${BASH_SOURCE[0]} ]; then
    REAL_SCRIPT_FILE=$(readlink ${BASH_SOURCE[0]})
fi

BASE_SCRIPT_DIR="$( cd "$( dirname "$REAL_SCRIPT_FILE" )" && pwd )"
BUILD_SHELL_DEVEL_EXEC="$BASE_SCRIPT_DIR/../../src/build_shell/build_shell"
which build_shell 2>&1>/dev/null
if [[ $? != 0 ]]; then
    if [[ -x $BUILD_SHELL_DEVEL_EXEC ]]; then
        $BUILD_SHELL_DEVEL_EXEC build_shell_dev_build
        if [[ $? == 0 ]]; then
            BUILD_SHELL_DEVEL_EXEC_DIR="$( cd "$( dirname "$BUILD_SHELL_DEVEL_EXEC" )" && pwd )"
            export PATH=$BUILD_SHELL_DEVEL_EXEC_DIR:$PATH
        fi
    fi
fi

