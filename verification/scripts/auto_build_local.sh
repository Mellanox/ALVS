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


# global int variables
declare -i num_commits
declare -i base_commit_num


#######################################################################################

function print_paths()
{
    echo "Exported variables:"
    echo "Git project           = $git_project"
    echo "Git branch            = $git_branch"
    echo "Base commit num       = $base_commit_num"
    echo "base_dir              = $base_dir"
    echo "wa_path               = $wa_path"
    echo "git_dir               = $git_dir"
    
    echo "Directories:"
    echo "branch_dir            = $branch_dir"
    echo "build_products_path   = $build_products_path"

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

    # release paths
    branch_dir=${base_dir}${git_project}/${git_branch}/
    
    last_release_link=${branch_dir}"last_release"
    last_stable_link=${branch_dir}"last_stable"
    
    # WA paths
    build_products_path=$wa_path"products/"

	print_paths

    echo "==== END Set variables ====="
}


######################################################################################

function create_product_dir()
{ 
	echo -e "\n======== $FUNCNAME ========="

    # make products folder
    mkdir -p $build_products_path
    if [ $? -ne 0 ]; then
        exit_status=1
        echo "ERROR: make dir ($build_products_path) failed"
        print_end_script
        exit $exit_status
    fi
    
	echo "======== End: $FUNCNAME ========="
}

######################################################################################

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

function cp_tar_file_to_products()
{
	echo -e "\n======== $FUNCNAME ========="

    main_folder=$1
    echo "main_folder   = $main_folder"

	previos_dir=$(pwd)
    cd $main_folder

    mv *tar.gz $build_products_path
    if [ $? -ne 0 ]; then
        echo "ERROR: failed to copy tar files"
        is_stable=0
    fi
    
    # back to previos folder
    cd $previos_dir

	echo "======== End: $FUNCNAME ========="
}

#######################################################################################

function tar_folder_and_copy_to_products()
{
	echo -e "\n======== $FUNCNAME ========="

    target=$1
    main_folder=$2
    sub_folder=$3
    echo "target        = $target"
    echo "main_folder   = $main_folder"
    echo "sub_folder    = $sub_folder"

	previos_dir=$(pwd)
    test -d $main_folder$sub_folder
    if [ $? -ne 0 ]; then
        return
    fi

    # go to main folder
    cd $main_folder
    tar_file_name=$target.tar.gz
    tar -zcf $tar_file_name $sub_folder
    if [ $? -ne 0 ]; then
        echo "WARNNING: failed to creare tar file $target"

        # back to previos folder
        cd $previos_dir
        return
    fi

    cp_tar_file_to_products $main_folder
    
    # back to previos folder
    cd $previos_dir

	echo "======== End: $FUNCNAME ========="
}

#######################################################################################

function build_and_add_bins()
{
	echo -e "\n======== Build ALVS and add bin files ========="
	
    cd $git_dir

    # Create release package    
    ./verification/scripts/make_all.sh release deb
    if [ $? -ne 0 ]; then
        echo "Failed at make_all release in version $version!"
        is_stable=0
    fi
    	
	# Copy debian package to products path
    mv *.deb $build_products_path
    if [ $? -ne 0 ]; then
        echo "ERROR: failed to move debian package to failed to $build_products_path"
        is_stable=0
    fi


    # Create debug package    
	./verification/scripts/make_all.sh debug deb
	if [ $? -ne 0 ]; then
        echo "ERROR: Failed at make_all debug in version $version!"
        is_stable=0
    fi

	# Copy debug debian package to products path
    mv *.deb $build_products_path
    if [ $? -ne 0 ]; then
        echo "ERROR: failed to move debian debug package to failed to $build_products_path"
        is_stable=0
    fi
    
	echo -e "\n======== END Build ALVS and add bin files ========="
}

#######################################################################################

function update_products_dir()
{
    echo -e "\n======== Update release directory ========"
  
    # Copy log files
    tar_folder_and_copy_to_products "logs" $git_dir "logs/"

	# create git.txt file which include last commit.
	git log -1 > "git.txt"
    if [ $? -ne 0 ]; then
        echo "WARNNING: Creating git.txt failed"
	else
	    mv  $git_dir"git.txt" $build_products_path
        if [ $? -ne 0 ]; then
            echo "ERROR: failed to copy git.txt to product folder"
        fi
    fi

	# copy version.h
	cp $git_dir/src/common/version.h $build_products_path
    if [ $? -ne 0 ]; then
        echo "WARNNING: copy version.h failed"
    fi

    echo "======== END update release directory ========"
}

#######################################################################################

function update_links()
{
    echo -e "\n======= Update links ========"

    # update last build
    echo "Last release link = $last_release_link"
    echo "Last stable link  = $last_stable_link"

    
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

function print_end_script()
{
    echo ""
    echo "< END script $script_name."
    echo "< exit status $exit_status"
    echo "<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
    echo ""
}

#######################################################################################

function set_version_dir_variable()
{ 
    echo -e "\n======= Setting version_dir variable ========="

	cd $git_dir
	fixed_version=$(grep -e "\"\$Revision: .* $\"" src/common/version.h | cut -d":" -f 2 | cut -d" " -f2 | cut -d"." -f2| uniq)
	num_commits=$(git rev-list HEAD | wc -l)
	num_commits=$num_commits-$base_commit_num
	version="${fixed_version}.${num_commits}"
	version_dir=${branch_dir}"archive/"${version}/
	echo "version		= $version"
	echo "version_dir	= $version_dir"

	echo "====== END Setting version_dir variable ======="
}

#######################################################################################

function update_release_dir()
{ 
    echo -e "\n======= Copy build products to release folder ========="

    sshpass -p nps_sw_release11 scp -r $build_products_path/. nps_sw_release@l-nps-006:$version_dir
    if [ $? -ne 0 ]; then
        echo "ERROR: failed to copy build product to $version_dir"
        is_stable=0
    fi

	echo "====== END Copy build products to release folder ========="
}

#######################################################################################

function run_auto_build_local()
{
    echo -e "\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
    echo "> Start script $script_name"


	set_global_variables

    create_product_dir
    
	set_version_dir_variable

	create_release_dir

	build_and_add_bins

	update_products_dir
	
	update_release_dir

	update_links

    print_end_script
}

#######################################################################################

run_auto_build_local $@

