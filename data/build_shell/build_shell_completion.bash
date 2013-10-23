#!/bin/bash

bs_create()
{
    if [ ! -z $BUILD_SHELL_UNSET_ENV_FILE ]; then
        source $BUILD_SHELL_UNSET_ENV_FILE
    fi

    build_shell $@
}

bs()
{
    if [ -z $1 ]; then
        bs_name
        return 0
    fi

    local mode=$1
    local component=$2
    local flags="${*:3}"

    if [[ $mode == "create" ]]; then
        bs_create $@
        return $?
    elif [[ $mode == "status" ]]; then
	bs_name
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

    if [[ $mode == "rebuild" ]]; then
        mode="build"
        flags="$flags --skip-configure"
    fi

    if [[ $mode != "pull" ]] && [[ $mode != "build" ]] && [[ $mode != "status" ]] && [[ $mode != "print" ]] && [[ $mode != "print_env" ]] && [[ $mode != "correct-branch" ]]; then
        echo "unknown build shell mode $mode"
        return 1
    fi

    if [[ $component == -* ]]; then
        flags="$component $flags"
        component=""
    fi

    if [ ! -z $component ]; then
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

    if [ ! -z $component ]; then
        flags="--from $component $flags"
    fi

    if [ ! -z $BUILD_SHELL_SHOW_CMD ]; then
        echo "build_shell --no-register -s $BUILD_SHELL_SRC_DIR -b $BUILD_SHELL_BUILD_DIR -i $BUILD_SHELL_INSTALL_DIR -f $BUILD_SHELL_CURRENT_BUILDSET $mode $flags"
    fi

    build_shell --no-register -s $BUILD_SHELL_SRC_DIR -b $BUILD_SHELL_BUILD_DIR -i $BUILD_SHELL_INSTALL_DIR -f $BUILD_SHELL_CURRENT_BUILDSET $mode $flags

}

bs_name()
{
    if [ -z $BUILD_SHELL_BUILD_DIR ]; then
        echo "No build selected"
        return 0
    fi

    if [ -z $1 ]; then
        BUILD_SHELL_NAME=$(jsonmod -d %.% -p $BUILD_SHELL_BUILD_DIR%.%name $HOME/.config/build_shell/available_builds.json)
        if [ ! -z $BUILD_SHELL_NAME ]; then
            echo "Using $BUILD_SHELL_NAME with build_dir $BUILD_SHELL_BUILD_DIR"
        else
            echo "Using unnamed build in: $BUILD_SHELL_BUILD_DIR"
        fi
    else
        local new_name=$1
        local names=$(jsonmod -d %.% -p "%{*}%.%name" $HOME/.config/build_shell/available_builds.json)
        while read -r line; do
            if [[ $new_name == $line ]]; then
                echo "Trying to use name for buildset which is allready in use"
                return 1
            fi
        done <<< "$names"

        $(jsonmod -i -d %.% -p "$BUILD_SHELL_BUILD_DIR%.%name" -v $new_name $HOME/.config/build_shell/available_builds.json)
        BUILD_SHELL_NAME=$new_name

    fi

    return 0
}

bs_edit()
{
    if [ -z $BUILD_SHELL_BUILD_DIR ]; then
        echo "No build selected"
        return 1
    fi

    local BUILD_SHELL_CURRENT_BUILDSET="$BUILD_SHELL_BUILD_DIR/build_shell/current_buildset"
    if [ ! -e $BUILD_SHELL_CURRENT_BUILDSET ]; then
        echo "Missing current buildset file $BUILD_SHELL_CURRENT_BUILDSET"
        return 0
    fi

    if [ -z $EDITOR ]; then
        EDITOR="vi"
    fi

    $EDITOR $BUILD_SHELL_CURRENT_BUILDSET
    return $?
}

bs_variable()
{
    if [ -z $BUILD_SHELL_BUILD_DIR ]; then
        echo "No build selected"
        return 1
    fi

    if [ -z $2 ]; then
        echo "bs_variable takes 2 or 3 arguments"
        echo "$ bs_variable \"variable_name\" \"variable_value\""
        echo "$ bs_variable \"project_name\" \"variable_name\" \"variable_value\""
        return 1
    fi

    local project_name
    local variable_name
    local variable_value
    local build_env_file="$BUILD_SHELL_BUILD_DIR/build_shell/build_environment.json"

    if [ -z $3 ]; then
        project_name="default"
        variable_name=$1
        variable_value=$2
    else
        project_name="projects%.%$1"
        variable_name=$2
        variable_value=$3
    fi

    variable_name="$project_name%.%$variable_name"

    jsonmod -i -d %.% -o "$project_name" "$build_env_file"
    jsonmod -i -d %.% -p "$variable_name" -v "$variable_value" "$BUILD_SHELL_BUILD_DIR/build_shell/build_environment.json"

    return $?
}

bss()
{
    if [ -z $1 ]; then
        bs_name
        return 0
    fi

    local ENVIRONMENT=$1
    local CONFIG_DIR="$HOME/.config/build_shell"
    if [ -e $CONFIG_DIR/available_builds.json ]; then
        env_set_file=$(build_shell get_env_file $ENVIRONMENT)
        if [ -z $env_set_file ]; then
            echo "Failed to find environment $ENVIRONMENT"
            return 1
        fi
        source $env_set_file
        BUILD_SHELL_NAME=$(jsonmod -d %.% -p $BUILD_SHELL_BUILD_DIR%.%name $HOME/.config/build_shell/available_builds.json)
    fi
}

_build_shell_select()
{
    local cur opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    if [ "$COMP_CWORD" -eq 1 ]; then
        opts=$(build_shell available)
        COMPREPLY=( $(compgen -W "${opts}" -- ${cur}) )
        return 0
    fi
}

_build_shell()
{
    if [ ! -n "$BUILD_SHELL_BUILD_DIR" ]; then
        return 0
    fi

    local cur opts
    COMPREPLY=()

    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    if [ "$COMP_CWORD" -eq 1 ]; then
        opts="pull build rebuild status print print_env"
        COMPREPLY=( $(compgen -W "${opts}" -- ${cur}) )
        return 0
    fi

    if [ "$COMP_CWORD" -eq 2 ] && [[ $cur != -* ]]; then
        local current_buildset_file="$BUILD_SHELL_BUILD_DIR/build_shell/current_buildset"
        opts=$(jsonmod $current_buildset_file -p "%{*}" -n)
        COMPREPLY=( $(compgen -W "${opts}" -- "${cur}") )
        return 0
    else
        opts="--skip-configure --skip-build --deep-clean --clean --continue --pull-first --print --correct-branch"
        COMPREPLY=( $(compgen -W "${opts}" -- "${cur}") )
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

bcd()
{
    if [ -z $BUILD_SHELL_BUILD_DIR ]; then
        echo "No build shell selected"
        return 0
    fi

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

_build_shell_bcd()
{
    if [ -z $BUILD_SHELL_BUILD_DIR ]; then
        return 0
    fi

    local cur prev opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    local oldpwd=$PWD
    if [ "$COMP_CWORD" -eq 1 ] && [[ $cur != -* ]]; then
        cd  $BUILD_SHELL_BUILD_DIR
    elif [ "$COMP_CWORD" -eq 2 ] && [[ $prev == "-" ]]; then
        cd $BUILD_SHELL_SRC_DIR
    fi
    if [ ! -z $oldpwd ]; then
        local relevant_dirs=$( compgen -d -- $cur )
        for dir in $relevant_dirs; do
            COMPREPLY+=( "$dir/" )
        done
        cd $oldpwd
    fi
    return 0
}

complete -F _build_shell bs
complete -F _build_shell_select bss
complete -o nospace -F _build_shell_bcd bcd

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

