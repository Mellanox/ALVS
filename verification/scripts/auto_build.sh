#!/bin/bash


#######################################################################################
#
#                   Copyright by Mellanox Technologies, 2016
#
#  Project:         All NPS_SOLUTION git projects
#
#  Description:     This script create release for NPS_SOLUTION projects
#
#  Notes:           runs on nighly build
#
#######################################################################################

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

#######################################################################################

function print_paths()
{
    echo "Directories:"
    echo "base_dir              = $base_dir"
    echo "branch_dir            = $branch_dir"
    echo "wa_path               = $wa_path"
    echo "git_dir               = $git_dir"
}

#######################################################################################

function set_global_variables()
{
    echo -e "\n====== Set variables ======="

    exit_status=0

	script_name=$(basename $0)
	script_dir=$(cd $(dirname $0) && pwd)

    # set paths
    base_dir="/mswg/release/nps/solutions/"
    
    # release paths
    branch_dir=${base_dir}${git_project}/${git_branch}/

    # WA paths
    wa_path="/root/auto_build_tmp/"${git_project}/${git_branch}/
    git_dir=$wa_path"$git_project/"

	print_paths

    echo "==== END Set variables ====="
}

#######################################################################################

function create_wa()
{   
	echo -e "\n======== $FUNCNAME ========="

    # choedk if folder already exist
    test -d $wa_path
    if [ $? -eq 0 ]; then
        echo "Warning: WA folder already exist ($wa_path). deleting folder..."
        rm -rf $wa_path
    fi
    
    # make tmp folder
    mkdir -p $wa_path
    if [ $? -ne 0 ]; then
        echo "ERROR: make dir ($wa_path) failed"
        exit_status=1
        print_end_script
        exit $exit_status
    fi
    
	echo "======== End: $FUNCNAME ========="
}

#######################################################################################

function clone_git()
{   
    echo -e "\n======== Clone GIT ========="

    cd $wa_path
    
    # clone git
    # TODO get URL
    git_url="http://l-gerrit.mtl.labs.mlnx:8080/$git_project"
    git clone $git_url
    if [ $? -eq 0 ]; then
        echo "git clone $git_url"
    else
        echo "ERROR: fail to clone $git_url"
        exit_status=1
        clean_wa_and_exit
    fi
    
    cd $git_dir
    git checkout $git_branch
    if [ $? -ne 0 ]; then
        echo "ERROR: fail to checkout clone $git_branch"
        exit_status=1
        clean_wa_and_exit
    fi
    
    echo "====== END Clone GIT ======="
	return $exit_status
}

#######################################################################################

function clean_wa()
{
    echo -e "\n======= Cleaning WA ========="

    rm -rf $wa_path
    if [ $? -eq 0 ]; then
        echo "WA ($wa_path) removed"
    else
        echo "WARNING: failed to remove WA: $wa_path"
    fi

    echo "====== END Cleaning WA ======="
}

#######################################################################################

function print_end_script()
{
    echo ""
    echo "< END script $script_name."
    echo "< exit status $exit_status"
    echo "<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
    echo ""
}

#######################################################################################

function clean_wa_and_exit()
{ 
	echo -e "\n======= Clean WA and exit ========="
    clean_wa
    print_end_script
	echo "====== END Clean WA and exit ======="
    exit $exit_status
}

#######################################################################################

function set_version_dir_variable()
{ 
    echo -e "\n======= Setting version_dir variable ========="

	cd $git_dir
	fixed_version=$(grep -e "\"\$Revision: .* $\"" src/common/version.h | cut -d":" -f 2 | cut -d" " -f2 | cut -d"." -f1-2| uniq)
	num_commits=$(git rev-list HEAD | wc -l)
	num_commits=$num_commits-$base_commit_num
	version="${fixed_version}.${num_commits}"
	version_dir=${branch_dir}"archive/"${version}/
	echo "version		= $version"
	echo "version_dir	= $version_dir"

	echo "====== END Setting version_dir variable ======="
}

#######################################################################################

function export_global_variables()
{ 
    echo -e "\n======= Export global variables ========="

    # Original args:
    export git_project
    export git_branch
    export base_commit_num
    
    # Directories:
    export base_dir
    export branch_dir
    export wa_path
    export git_dir

	echo "====== END Export global variables ========="
}


#######################################################################################

function unexport_global_variables()
{ 
    echo -e "\n======= Un-Export global variables ========="

    # Original args:
    unset git_project
    unset git_branch
    unset base_commit_num
    
    # Directories:
    unset base_dir
    unset branch_dir
    unset wa_path
    unset git_dir

	echo "====== END Un-Export global variables ========="
}


#######################################################################################

function run_auto_build()
{

    echo -e "\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
    echo "> Start script $script_name"
	echo -e "\nrunning under user: $USER"
	echo "$(date)"


	parse_cmd $@
	
	set_global_variables

    create_wa
	
	clone_git
	
	export_global_variables

    # run build from git & check status
	$local_auto_build 
	exit_status=$?
	if [ $exit_status -eq 0 ]; then
        echo "Local Auto-Build passed"
    else
        echo "Local Auto-Build failed !!!"
    fi

	unexport_global_variables

    clean_wa_and_exit

}

#######################################################################################

run_auto_build $@

