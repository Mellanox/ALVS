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


# ------------------------------------------------------------------------------
function usage()
{
    cat <<EOF
Usage: $script_name [setup_num]
This script runs basic test for ALVS prohjetc like make (release, debug), coveriry, coding style, system level tests

Examples:
$script_name 5

EOF
   exit 1
}


# ------------------------------------------------------------------------------
function parse_cmd()
{
    # check number of arguments
    test $# -eq 0
    
    
    if [ $? -eq 0 ]; then
        echo "NOTE: No args given. Script wont run regression..."
        setup_num=0
        return
    fi


    test $# -ne 1 && usage
    
    # move arguments to variables
    setup_num=$1

    # print arguments
    echo ""    
    echo "========== Args:  =========="
    echo "setup_num      = $setup_num"
    echo "======== END Args  ========="
}


# ------------------------------------------------------------------------------
log_file="my_test.log"
function log()
{
    printf '%s\n' "$@" > $log_file
}


# ------------------------------------------------------------------------------
function start_script()
{
    echo ""
    echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>"
    echo "> Start running $script_name"
    
    # init variables
    scripts_path="verification/scripts/"
    make_status=0
    exit_status=0
}

# ------------------------------------------------------------------------------
function exec_make()
{
    echo "running make script"
    echo "==================="
    $scripts_path"make_all.sh" all deb
    rc=$?
    if [ $rc -ne 0 ]; then
        echo 'ERROR: Make script failed'
        make_status=1
        exit_status=1
    fi
}

# ------------------------------------------------------------------------------
function exec_coverity()
{
    echo "running coverity script"
    echo "======================="
    $scripts_path"coverity.sh"
    rc=$?
    if [ $rc -ne 0 ]; then
        echo 'ERROR: Coveriry script failed'
        exit_status=1
    fi
}

# ------------------------------------------------------------------------------
function exec_coding_style()
{
    echo "running coding style script"
    echo "==========================="
    $scripts_path"coding_style.sh"
    rc=$?
    if [ $rc -ne 0 ]; then
        echo 'ERROR: Coding style script failed'
        exit_status=1
    fi
}

# ------------------------------------------------------------------------------
function exec_regression()
{
    if [ $setup_num -eq 0 ]; then
        return
    fi

    echo "running Regression"
    echo "=================="
    pwd
    # check if make passed    
    if [ $make_status -ne 0 ]; then
        echo 'NOTE: make failed. regression wont run. '
        return
    fi
    
    # run regression
    $scripts_path"../testing/system_level/regression_run.py" -s $setup_num -l push_regression -e true
    rc=$?
    if [ $rc -ne 0 ]; then
        echo 'ERROR: Regression failed'
        exit_status=1
    fi
}

# ------------------------------------------------------------------------------
function exit_script()
{
    echo ""
    echo "< End running $script_name"
    echo "<<<<<<<<<<<<<<<<<<<<<<<<<<<<"

    exit $exit_status
}


#########################################
#                                       #
#              Main                     #
#                                       #
#########################################
function main()
{
    # parse arguments
	parse_cmd $@
	
    # init & print start
    start_script

	# Run compilation test
    exec_make

    # Run coverity test
    exec_coverity

    # Run coding style test
    exec_coding_style

    # run regression
    exec_regression
    
    # exit with status & print exit
    exit_script
}

main $@
