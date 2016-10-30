#!/bin/bash

function usage()
{
    cat <<EOF
Usage: $script_name [GIT_PROJECT] [GIT_BRANCH] [BASE_COMMIT_NUM] [LOCAL_AUTO_BUILD]

This script runs auto-build on a specific project & branch
GIT_PROJECT:       Git project name
GIT_BRANCH:        Git Branch name
BASE_COMMIT_NUM:   Number of commit release start from
LOCAL_AUTO_BUILD:  Auto build file located in GIT. this file continue with build after cloning GIT

Examples:
$script_name ALVS master 200 ./verification/scripts/auto_build_local.sh

EOF
   exit 1
}


#######################################################################################

function parse_cmd()
{
    # check number of arguments
    test $# -ne 4 && usage
    
    # move arguments to variables
    git_project=$1
    git_branch=$2
    base_commit_num=$3
    local_auto_build=$4

    # print arguments
    echo ""    
    echo "========== Args:  =========="
    echo "Git project      = $git_project"
    echo "Git branch       = $git_branch"
    echo "Base commit num  = $base_commit_num"
    echo "Local auto-build = $local_auto_build"
    echo "======== END Args  ========="
}

echo "=========================== exec wrapper ================================"

parse_cmd $@

target_vm="gen-l-vrt-232-005"
echo "connecting to $target_vm"
ssh root@$target_vm "/mswg/release/nps/solutions/auto_build.sh $git_project $git_branch $base_commit_num $local_auto_build; exit"
if [ $? -eq 0 ]; then
    echo "Test passed"
    rc=0
else
    echo "Test Failed"
    rc=1
fi

echo "========================= END exec wrapper =============================="
exit $rc



