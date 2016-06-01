#!/bin/bash

#######################################################################################
#
#                   Copyright by Mellanox Technologies, 2016
#
#  Project:         All NPS_SOLUTION git projects
#
#  Description:     This script copy relevant files & dirs to Black Duck server
#					for open source check
#
#  Notes:           
#
#######################################################################################


#######################################################################################

function set_global_variables()
{
    echo -e "\n====== Set variables ======="

	exit_status=0 
    
	# WA paths
    wa_path="/tmp/ALVS_Black_Duck"
 	
	echo "Directories:"
    echo "wa_path               = $wa_path"

    echo "==== END Set variables ====="
}

#######################################################################################

function create_wa()
{   
	echo -e "\n======== Creating WA directory ========="

    # check if folder already exist
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
        exit $exit_status
    fi

	curr_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
	alvs_dir="${curr_dir}/../../"

	version=$(grep -e "\"\$Revision: .* $\"" $alvs_dir/src/common/version.h | cut -d":" -f 2 | cut -d" " -f2)

	release_dir=$wa_path/ALVS_$version
	echo "Creating release dir: $release_dir"
	mkdir -p $release_dir
    if [ $? -ne 0 ]; then
        echo "ERROR: make dir ($release_dir) failed"
        exit_status=1
        exit $exit_status
    fi

	cp -r $alvs_dir/* $release_dir
	if [ $? -ne 0 ]; then
        echo "ERROR: copy ALVS content to dir ($release_dir) failed"
        exit_status=1
        exit $exit_status
    fi
  
	echo "======== End Creating WA directory ========="
}

#######################################################################################

function clean_wa_and_exit()
{ 
	echo -e "\n======= Clean WA and exit ========="
	
	rm -rf $wa_path
    if [ $? -eq 0 ]; then
        echo "WA ($wa_path) removed"
    else
        echo "WARNING: failed to remove WA: $wa_path"
    fi
	
	echo "====== END Clean WA and exit ======="
    exit $exit_status
}

#######################################################################################

function remove_redundant_files()
{ 
	echo -e "\n======= Removing redundant files ========="

	echo "Removing the following files & dirs in $release_dir:"
	echo "app_skeleton bin  build .cproject EZdk  EZide  .git  .gitignore .project  .settings  target  verification"
	rm -rf $release_dir/app_skeleton $release_dir/bin  $release_dir/build $release_dir/.cproject $release_dir/EZdk  $release_dir/EZide
	rm -rf $release_dir/.git  $release_dir/.gitignore $release_dir/.project  $release_dir/.settings  $release_dir/target  $release_dir/verification

	echo "====== END Removing redundant files ======="
}

#######################################################################################

function copy_to_black_duck_server()
{ 
	echo -e "\n=======  Copy to Bluck Duck server ========="
	
	sshpass -p basims11 scp -r $release_dir basims@sw-blackduck01:/storage/MLNX_source/

	echo "====== END  Copy to Bluck Duck server ======="
}

#######################################################################################

function run_black_duck()
{

    echo -e "\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
    echo "> Start script `basename $0`"
	echo -e "\nrunning under user: $USER"
	echo "$(date)"

	set_global_variables

	create_wa

	remove_redundant_files

	copy_to_black_duck_server

	clean_wa_and_exit

}

#######################################################################################

run_black_duck

