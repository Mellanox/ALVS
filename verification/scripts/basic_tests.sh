#!/bin/bash


#######################################################################################
#
#                   Copyright by Mellanox Technologies, 2016
#
#  Project:         ALVS
#
#  Description:     This script runs basic test for ALVS prohjetc like make (release, debug),
#                   coveriry, etc'
#
#  Notes:           runs on each push and nighly build
#
#######################################################################################


script_name=$(basename $0)
script_dir=$(cd $(dirname $0) && pwd)

function error_exit()
{
    echo $@ 1>&2
    exit 1
}


function log()
{
    printf '%s\n' "$@" > my_test.log
}

function usage()
{
    cat <<EOF
Usage: $script_name
This script runs build on the current working area (release+debug)

Examples:
$script_name

EOF
   exit 1
}


function print_start_script()
{
    echo ""
    echo "============================"
    echo "= Start running $script_name"
    echo "============================"
}


function init_variables()
{
    # init exit status to failure
    exit_status=1 

    # init is_compiled to true (no errors)
    is_compiled=1
}

function compile_git()
{
    echo "Function: $FUNCNAME called"


    echo "perform: make clean"
    make clean > $make_clean_file
    if [ $? -ne 0 ]; then
        echo 'ERROR: make clean failed. for more details look at $make_clean_file'
        is_compiled=0
        return
    fi

    
    echo "perform: make $make_params"
    make $make_params > $make_log_file
    if [ $? -ne 0 ]; then
        echo 'ERROR make $make_params failed. for more details look at $make_log_file'
        is_compiled=0
        return
    fi
}


function make_release()
{
    echo "Function: $FUNCNAME called"

    # prepare compilation parameters
    make_params="all"
    make_clean_file='make_clean_release.log'
    make_log_file='make_release.log'

    # make
    compile_git

}

function make_debug()
{
    echo "Function: $FUNCNAME called"

    # set local variable & export
    DEBUG=TRUE
    export DEBUG
    
    # prepare compilation parameters
    make_params="all"
    make_clean_file='make_clean_debug.log'
    make_log_file='make_debug.log'

    # make
    compile_git

    # unset local variable
    unset DEBUG
}

function clean_wa()
{
    echo "Function: $FUNCNAME called"

    # TODO: implement
}

function print_end_script()
{
    echo "test 1"
    echo ""
    echo "============================"
    echo "= End running $script_name"
    echo "============================"
}


function clean_wa_and_exit()
{
    echo "Clean WA and exit"    

    clean_wa
    print_end_script
    exit $exit_status
}


#########################################
#                                       #
#              Main                     #
#                                       #
#########################################
trap "exit" INT TERM
trap "kill 0" EXIT

log "running under user: $USER"

print_start_script
init_variables

# make
make_release
make_debug

# Clean working area & exit
exit_status=0
clean_wa_and_exit

