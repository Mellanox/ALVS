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

log_file="my_test.log"

function log()
{
    printf '%s\n' "$@" > $log_file
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


function start_script()
{
    echo ""
    echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>>"
    echo "> Start running $script_name"
}

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

start_script
exit_status=0
scripts_path="verification/scripts/"

# Run compilation test
echo "running make script"
echo "==================="
$scripts_path"make_all.sh"
rc=$?
if [ $rc -ne 0 ]; then
    echo 'ERROR: Make script failed'
    exit_status=1
fi

# Run coverity test
echo "running coverity script"
echo "======================="
$scripts_path"coverity.sh"
rc=$?
if [ $rc -ne 0 ]; then
    echo 'ERROR: Coveriry script failed'
    exit_status=1
fi

exit_script

