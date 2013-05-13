#!/bin/bash

bs()
{
    if [ -z $1 ]; then
        if [ -z $BUILD_SHELL_BUILD_DIR ]; then
            echo "No build selected"
        else
            echo "Using build in: $BUILD_SHELL_BUILD_DIR"
        fi
        return 0
    fi

    if [ ! -n $BUILD_SHELL_BUILD_DIR ]; then
        echo "No build selected, please select a buildset with bss"
        return 0
    fi

    local BUILD_SHELL_CURRENT_BUILDSET="$BUILD_SHELL_BUILD_DIR/build_shell/current_buildset"
    if [ ! -e $BUILD_SHELL_CURRENT_BUILDSET ]; then
        echo "Missing current buildset file $BUILD_SHELL_CURRENT_BUILDSET"
        return 0
    fi

    local mode=$1
    local component=$2
    local flags="${*:3}"

    if [[ $mode != "pull" ]] && [[ $mode != "build" ]]; then
        echo "unknown build shell mode $mode"
        exit 1
    fi

    if [[ $component == -* ]]; then
        flags="$component $flags"
        $component=""
    fi

    if [ -n $component ]; then
        local current_buildset_file="$BUILD_SHELL_BUILD_DIR/build_shell/current_buildset"
        local opts=$(jsonmod $current_buildset_file -p "%{*}" -n)
        local found=false
        for project in $opts
        do
            if [[ $project == "$component" ]]; then
                found=true
                break
            fi
        done

        if ! $found; then
            echo "Not valid component: $component. Valid components are $opts"
            return 0
        fi
    fi

    if [ -n $component ]; then
        flags="--from $component $flags"
    fi

    build_shell --no-register -s $BUILD_SHELL_SRC_DIR -b $BUILD_SHELL_BUILD_DIR -i $BUILD_SHELL_INSTALL_DIR -f $BUILD_SHELL_CURRENT_BUILDSET $mode $flags

}

bss()
{
    local ENVIRONMENT=$1
    local CONFIG_DIR="$HOME/.config/build_shell"
    if [ -e $CONFIG_DIR/available_builds.json ]; then
        source $(jsonmod $CONFIG_DIR/available_builds.json -p $ENVIRONMENT.set_env_file)
    fi
}

_build_shell_select()
{
    local cur opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    if [ "$COMP_CWORD" -eq 1 ]; then
        opts=$(build_shell available)
        COMPREPLY=( $(compgen -W "${opts}" ${cur}) )
        return 0
    fi
}

_build_shell()
{
    if [ ! -n $BUILD_SHELL_BUILD_DIR ]; then
        return 0
    fi

    local cur opts
    COMPREPLY=()

    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    if [ "$COMP_CWORD" -eq 1 ]; then
        opts="pull build"
        COMPREPLY=( $(compgen -W "${opts}" ${cur}) )
        return 0
    fi

    if [ "$COMP_CWORD" -eq 2 ]; then
        local current_buildset_file="$BUILD_SHELL_BUILD_DIR/build_shell/current_buildset"
        opts=$(jsonmod $current_buildset_file -p "%{*}" -n)
        COMPREPLY=( $(compgen -W "${opts}" ${cur}) )
        return 0
    fi
}

build_shell_change_to_corresponding_dir()
{
    local cur_dir dest_dir cur_dest_dir
    if [[ $PWD == $1* ]]; then
        cur_dir=$1
        dest_dir=$2
    elif [[ $PWD == $2* ]]; then
        cur_dir=$2
        dest_dir=$1
    else
        echo "Could not find current_directory being inside build nor src"
        return 1
    fi

    cur_dest_dir=$(pwd | sed s%$cur_dir%$dest_dir%g)

    while [[ $cur_dest_dir != $dest_dir ]]; do
        if [ -d $cur_dest_dir ]; then
            cd $cur_dest_dir
            return 0
        fi
        cur_dest_dir=$(dirname "$cur_dest_dir")
    done

    cd $cur_dest_dir
    return 0;
}

build_shell_cd_with_component()
{
    if [ -n $2 ]; then
        cd $1/$2
    else
        cd $1
    fi
}

bscd()
{
    local other=false
    local component=""
    if [[ $1 == "-" ]]; then
        other=true
        component=$2
    else
        component=$1
    fi

    if $other; then
        if [ -z $component ]; then
            if [[ $PWD == $BUILD_SHELL_BUILD_DIR* ]] || [[ $PWD == $BUILD_SHELL_SRC_DIR* ]] ; then
                build_shell_change_to_corresponding_dir $BUILD_SHELL_BUILD_DIR $BUILD_SHELL_SRC_DIR
                if [ $? -eq 0 ]; then
                    return 0
                fi
            fi
        fi
        build_shell_cd_with_component $BUILD_SHELL_SRC_DIR $component
        return 0
    fi

    build_shell_cd_with_component $BUILD_SHELL_BUILD_DIR $component
}

_build_shell_bscd()
{
    local cur prev opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    if [ "$COMP_CWORD" -eq 1 ] && [[ $cur != -* ]]; then
        opts=$(ls -d $BUILD_SHELL_BUILD_DIR/*/ | sed s%$BUILD_SHELL_BUILD_DIR%% | sed s%/%%g)
        COMPREPLY=( $(compgen -W "${opts}" ${cur}) )
        return 0
    elif [ "$COMP_CWORD" -eq 2 ] && [[ $prev == "-" ]]; then
        opts=$(ls -d $BUILD_SHELL_SRC_DIR/*/ | sed s%$BUILD_SHELL_SRC_DIR%% | sed s%/%%g)
        COMPREPLY=( $(compgen -W "${opts}" ${cur}) )
        return 0
    fi
    return 0
}

complete -F _build_shell bs
complete -F _build_shell_select bss
complete -F _build_shell_bscd bscd

BUILD_SHELL_REAL_SCRIPT_FILE=${BASH_SOURCE[0]}
if [ -L ${BASH_SOURCE[0]} ]; then
    BUILD_SHELL_REAL_SCRIPT_FILE=$(readlink ${BASH_SOURCE[0]})
fi

BUILD_SHELL_BASE_SCRIPT_DIR="$( cd "$( dirname "$BUILD_SHELL_REAL_SCRIPT_FILE" )" && pwd )"
BUILD_SHELL_DEVEL_EXEC="$BUILD_SHELL_BASE_SCRIPT_DIR/../../src/build_shell/build_shell"
BUILD_SHELL_DEVEL_JSONMOD_EXEC="$BUILD_SHELL_BASE_SCRIPT_DIR/../../src/jsonmod/jsonmod"
build_shell -h > /dev/null 2>&1
if [[ $? != 0 ]]; then
    if [[ -x $BUILD_SHELL_DEVEL_EXEC ]]; then
        $BUILD_SHELL_DEVEL_EXEC build_shell_dev_build
        if [[ $? == 0 ]]; then
            BUILD_SHELL_DEVEL_EXEC_DIR="$( cd "$( dirname "$BUILD_SHELL_DEVEL_EXEC" )" && pwd )"
            export PATH=$BUILD_SHELL_DEVEL_EXEC_DIR:$PATH
            if [[ -x $BUILD_SHELL_DEVEL_JSONMOD_EXEC ]]; then
                BUILD_SHELL_DEVEL_JSONMOD_EXEC_DIR="$( cd "$( dirname "$BUILD_SHELL_DEVEL_JSONMOD_EXEC" )" && pwd )"
                export PATH=$BUILD_SHELL_DEVEL_JSONMOD_EXEC_DIR:$PATH
            fi
        fi
    fi
fi
unset BUILD_SHELL_DEVEL_EXEC
unset BUILD_SHELL_DEVEL_EXEC_DIR
unset BUILD_SHELL_DEVEL_JSONMOD_EXEC
unset BUILD_SHELL_DEVEL_JSONMOD_EXEC_DIR
unset BUILD_SHELL_BASE_SCRIPT_DIR
unset BUILD_SHELL_REAL_SCRIPT_FILE

