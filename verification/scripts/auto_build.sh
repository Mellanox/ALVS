#!/bin/bash


#######################################################################################
#
#                   Copyright by Mellanox Technologies, 2016
#
#  Project:         ALVS
#
#  Description:     This script prepare git working area for running basic test
#                   defined in git
#
#  Notes:           runs nighly build
#
#######################################################################################


script_name=$(basename $0)
script_dir=$(cd $(dirname $0) && pwd)

#folder definition:
BASE_FOLDER='/mswg/release/nps/solutions/'

#TODO change back
is_using_ssh=0


function error_exit()
{
    echo $@ 1>&2
    exit 1
}


function usage()
{
    cat <<EOF
Usage: $script_name [PROJECT_NAME] [BRANCH]
This script runs auto-build on a specific project & branch
PROJECT_NAME:           project name
BRANCH:                 branch version

Examples:
$script_name ALVS 1.0

EOF
   exit 1
}


function print_start_script()
{
    echo ""
    echo "============================"
    echo "=       Start script       ="
    echo "============================"
}


function init_variables()
{
    # init exit status to failure
    exit_status=1 

    # init is_stable to true (no errors)
    is_stable=1
    
    # pre-command for realease folder
    nps_sw_user_command="sudo -u nps_sw_release"
}

function parse_cmd()
{
    # check number of arguments
    test $# -ne 2 && usage
    
    # move arguments to variables
    project_params=$1
    branch=$2

    # print arguments
    echo ""    
    echo "========== Args:  =========="
    echo "Project name   = $project_params"
    echo "branch         = $branch"
    echo "======== END Args  ========="
}


function set_global_variables()
{
    echo ""    
    echo "====== Set variables ======="
    # set paths
    base_dir=$BASE_FOLDER
    version=`date +%Y-%m-%d-%H-%M`
    
    # release paths
    branch_dir=$base_dir$project_params/$branch/
    version_dir=$branch_dir"archive/"$version/
    
    last_release_link=$branch_dir"last_release"
    last_stable_link=$branch_dir"last_stable"
    
    # WA paths
    if [ $is_using_ssh -eq 0 ]; then
        echo "WARNNING: not using SSH"
        wa_path=$branch_dir"auto_build_tmp/"
        git_dir=$wa_path"$project_params/"
    else
        wa_path="/root/auto_build_tmp/"
        git_dir=$wa_path"$project_params/"
    fi

    # print paths
    echo ""
    echo "Directories:"
    echo "base_dir              = $base_dir"
    echo "branch_dir            = $branch_dir"
    echo "version_dir           = $version_dir"
    echo "wa_path               = $wa_path"
    echo "git_dir               = $git_dir"

    echo ""
    echo "Links:"
    echo "last_release_link     = $last_release_link"
    echo "last_stable_link      = $last_stable_link"

    echo "==== END Set variables ====="
}


function clone_git()
{
    echo ""    
    echo "======== Clone GIT ========="

  
    # choedk if folder already exist
    test -d $wa_path
    if [ $? -eq 0 ]; then
        echo "Warning: WA folder already exist ($wa_path). deleting folder..."
        $nps_sw_user_command rm -rf $wa_path
    fi
    
    # make tmp folder
    $nps_sw_user_command  mkdir -p $wa_path
    if [ $? -ne 0 ]; then
        echo "ERROR: make dir ($wa_path) failed"
        print_end_script
        exit $retval        
    fi
    cd $wa_path
    
    # clone git
    # TODO get URL
    git_url='http://l-gerrit.mtl.labs.mlnx:8080/ALVS'
    $nps_sw_user_command git clone $git_url
    if [ $? -eq 0 ]; then
        echo "git clone $git_url"
    else
        echo "ERROR: fail to clone $git_url"
        exit_status=$retval
        clean_wa_and_exit

    fi

    echo "====== END Clone GIT ======="
}


function perform_git_tests()
{
    cd $git_dir
    
    ./verification/scripts/make_all.sh
    if [ $? -eq 0 ]; then
        echo "Version didnt change. Version $version"
        is_stable=0
        clean_wa_and_exit    
    fi
    
    copy_bin_and_logs_files
}

function copy_bin_and_logs_files()
{
    echo ""    
    echo "=== Copy bin & logs files ==="

    # create dest folder (if not exist
    test -d $version_dir
    if [ $? -eq 1 ]; then
        echo "Creating folder $version_dir"
        $nps_sw_user_command mkdir -p $version_dir
    fi


    # Copy bin files
    test -d $git_dir"bin/"
    if [ $? -eq 0 ]; then
        $nps_sw_user_command cp -a $git_dir"bin/." $version_dir
    fi

    
    # Copy log files
    $nps_sw_user_command cp $git_dir"*.log" $version_dir
    if [ $? -eq 1 ]; then
        echo "WARNNING: copy log files failed"
    fi


    echo "= END Copy bin files & logs ="
}


function update_links()
{
    echo ""    
    echo "======= Update links ========"

    # update last build
    echo "vestion dir:       $version_dir"
    echo "Last release link: $last_release_link"
    echo "Last stable link:  $last_stable_link"

    
    $nps_sw_user_command ln -sfn $version_dir $last_release_link

    # set last stable
    if [ $is_stable -eq 1 ]; then
        $nps_sw_user_command ln -sfn $version_dir $last_stable_link
    else
        echo "Warning: last stable link didnt updated"
    fi

    echo "===== END Update links ======"
}

function clean_wa()
{
    echo ""    
    echo "======= Cleaning WA ========="

    $nps_sw_user_command rm -rf $wa_path
    if [ $? -eq 0 ]; then
        echo "WA ($wa_path) removed"
    else
        echo "WARNING: failed to remove WA: $wa_path"
    fi

    echo "====== END Cleaning WA ======="
}

function print_end_script()
{
    echo ""
    echo "============================"
    echo "=        END script        ="
    echo "============================"
    echo ""
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

echo "running under user: $USER"
print_start_script


init_variables


parse_cmd $@
set_global_variables

clone_git

# create dest folder (if not exist
test -d $version_dir
if [ $? -eq 0 ]; then
    echo "Version didnt change. Version $version"
    exit_status=0
    clean_wa_and_exit    
fi


perform_git_tests
update_links


# TODO: move to ZIP

# Clean working area & exit
exit_status=0
clean_wa_and_exit

$SHELL

