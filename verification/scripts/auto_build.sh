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

#######################################################################################

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

#######################################################################################

function print_paths()
{
    echo "Directories:"
    echo "base_dir              = $base_dir"
    echo "branch_dir            = $branch_dir"
    echo "wa_path               = $wa_path"
    echo "git_dir               = $git_dir"

    echo "Links:"
    echo "last_release_link     = $last_release_link"
    echo "last_stable_link      = $last_stable_link"
}

#######################################################################################

function set_global_variables()
{
    echo -e "\n====== Set variables ======="

    exit_status=0
    # init is_stable to true (no errors)
    is_stable=1
    # pre-command for realease folder
    nps_sw_user_command="sudo -u nps_sw_release"

	script_name=$(basename $0)
	script_dir=$(cd $(dirname $0) && pwd)

    # set paths
    base_dir="/mswg/release/nps/solutions/"
    
    # release paths
    branch_dir=${base_dir}${project_params}/${branch}/
    
    last_release_link=${branch_dir}"last_release"
    last_stable_link=${branch_dir}"last_stable"
    
    # WA paths
    wa_path="/root/auto_build_tmp/"
    git_dir=$wa_path"$project_params/"

	print_paths

    echo "==== END Set variables ====="
}

#######################################################################################

function clone_git()
{   
    echo -e "\n======== Clone GIT ========="

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
        return $exit_status
    fi
    cd $wa_path
    
    # clone git
    # TODO get URL
    git_url='http://l-gerrit.mtl.labs.mlnx:8080/ALVS'
    git clone $git_url
    if [ $? -eq 0 ]; then
        echo "git clone $git_url"
    else
        echo "ERROR: fail to clone $git_url"
        exit_status=1
        clean_wa_and_exit
		return $exit_status
    fi

    echo "====== END Clone GIT ======="
	return $exit_status
}

#######################################################################################

function create_release_dir()
{ 
	echo -e "\n======= Create release directory ========="
    
	test -d $version_dir
	if [ $? -eq 0 ]; then
		echo "Version didn't change. Version $version"
		exit_status=0
		clean_wa_and_exit    
	fi

	# create release folder
    echo "Creating folder $version_dir"
    $nps_sw_user_command mkdir -p $version_dir
    if [ $? -ne 0 ]; then
		echo "ERROR: can creating release folder ($version_dir)"
		exit_status=1
		clean_wa_and_exit    
    fi

	echo "====== END Create release directory ======="
}

#######################################################################################

function build_and_add_bins()
{
	echo -e "\n======== Build ALVS and add bin files ========="
	
    cd $git_dir
    
    ./verification/scripts/make_all.sh release
    if [ $? -ne 0 ]; then
        echo "Failed at make_all release in version $version!"
        is_stable=0
    fi
    	
    	
	# Copy bin directory before make clean
    test -d $git_dir"bin/"
    if [ $? -eq 0 ]; then
        sshpass -p nps_sw_release11 scp -r $git_dir"bin/" nps_sw_release@l-nps-006:$version_dir
        if [ $? -ne 0 ]; then
            echo "ERROR: failed to copy release bin files"
        fi
        #$nps_sw_user_command cp -a $git_dir"bin/" $version_dir
    fi

	./verification/scripts/make_all.sh debug
	if [ $? -ne 0 ]; then
        echo "ERROR: Failed at make_all debug in version $version!"
        is_stable=0
    fi

	test -d $git_dir"bin/"
    if [ $? -eq 0 ]; then
        sshpass -p nps_sw_release11 scp -r $git_dir"bin/." nps_sw_release@l-nps-006:$version_dir/bin
        if [ $? -ne 0 ]; then
            echo "ERROR: failed to copy debug bin files"
        fi
        #$nps_sw_user_command cp -a $git_dir"bin/." $version_dir/bin
    fi

	echo -e "\n======== END Build ALVS and add bin files ========="
}

#######################################################################################

function update_release_dir()
{
    echo -e "\n======== Update release directory ========"
  
  
    # Copy log files
    my_logs_files=$git_dir'*.log'
    sshpass -p nps_sw_release11 scp -r $my_logs_files nps_sw_release@l-nps-006:$version_dir
    if [ $? -ne 0 ]; then
        echo "WARNNING: copy log files failed"
    fi

	# create git.txt file which include last commit.
	git log -1 > "git.txt"
    if [ $? -ne 0 ]; then
        echo "WARNNING: Creating git.txt failed"
	else
        sshpass -p nps_sw_release11 scp -r $git_dir"git.txt" nps_sw_release@l-nps-006:$version_dir
        if [ $? -ne 0 ]; then
            echo "WARNNING: copy git.txt failed"
        fi
    fi

	# copy version.h
	sshpass -p nps_sw_release11 scp $git_dir/src/common/version.h nps_sw_release@l-nps-006:$version_dir/
    if [ $? -ne 0 ]; then
        echo "WARNNING: copy version.h failed"
    fi

	# create a tar file containing: bin files, git.txt & version.h
	cd $version_dir
	$nps_sw_user_command tar zcf ${version}.tar.gz bin/ version.h git.txt
    if [ $? -ne 0 ]; then
        echo "WARNNING: copy version.h failed"
    fi

	# removing redundant files & dirs
	$nps_sw_user_command rm -rf bin/ version.h git.txt
    if [ $? -ne 0 ]; then
        echo "WARNNING: removing redundant files & dirs failed"
    fi

    echo "======== END update release directory ========"
}

#######################################################################################

function update_links()
{
    echo -e "\n======= Update links ========"

    # update last build
    echo "vestion dir =			$version_dir"
    echo "Last release link = 	$last_release_link"
    echo "Last stable link =	$last_stable_link"

    
    $nps_sw_user_command ln -sfn $version_dir $last_release_link

    # set last stable
    if [ $is_stable -eq 1 ]; then
        $nps_sw_user_command ln -sfn $version_dir $last_stable_link
    else
        echo "Warning: last stable link didn't updated"
    fi

    echo "===== END Update links ======"
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
	fixed_version=$(grep -e "\"\$Revision: .* $\"" src/common/version.h | cut -d":" -f 2 | cut -d" " -f2 | cut -d"." -f1-2)
	num_commits=$(git rev-list HEAD | wc -l)
	version="${fixed_version}.${num_commits}"
	version_dir=${branch_dir}"archive/"${version}/
	echo "version		= $version"
	echo "version_dir	= $version_dir"

	echo "====== END Setting version_dir variable ======="
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

	clone_git
	if [ $? -ne 0 ]; then
		exit $exit_status
	fi

	set_version_dir_variable

	create_release_dir

	build_and_add_bins

	update_release_dir

	update_links

	clean_wa_and_exit

}

#######################################################################################

run_auto_build $@

