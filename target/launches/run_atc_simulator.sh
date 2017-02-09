#!/bin/bash

###############################################################################
#
#                    Copyright by EZchip Technologies, 2015
#
#
#  Project:         demo_cp_target
#  Description:     This script runs EZsim with demo_cp_target
#  Notes:           on each run all work/sim/in + work/sim/out
#                   contents are being deleted at the begining
#
###############################################################################

script_name=$(basename $0)
script_dir=$(cd $(dirname $0) && pwd)

function error_exit()
{
	echo $@ 1>&2
	exit 1
}


function usage()
{
	cat <<EOF
Usage: $script_name [EZDK_DIR]
This script runs EZsim with demo_cp_target
EZDK_DIR:    EZdk base folder path

Examples:
$script_name /home/EZchip/EZdk

EOF
   exit 1
}


function parse_cmd()
{
	arg=$1
	main_dir=$(pwd)
	ezdk_base=$main_dir/EZdk
	bin_dir=$main_dir/bin
	if [ "$arg" == "-debug" ]; then
	  echo "Use bin on debug mode"
	  bin_dir=$main_dir/bin/debug
	fi
#	test $# -ne 1 && usage
#	ezdk_base=$(cd $1 && pwd)

}


function set_global_variables()
{
	# set enviroment variables
	#export PATH=/.autodirect/sw_tools/OpenSource/minicom/INSTALLS/minicom-2.7/linux_x86_64/bin:${PATH}
	#export LM_LICENSE_FILE=27020@mtlxls03-slv02:27020@mtlxls03-slv01:27020@mtlxls03

	
	target_dir=$main_dir/target
	#target_dir=$(dirname $script_dir)
	target_frames_dir=$target_dir/frames
	target_config_dir=$target_dir/config
	target_work_dir=$target_dir/work
	target_work_cp_dir=$target_work_dir/control_plane
	target_work_in_frames_dir=$target_work_dir/simulator/frame/in
	target_work_out_frames_dir=$target_work_dir/simulator/frame/out
	target_work_out_logs_dir=$target_work_dir/simulator/logs
	target_work_out_memory_dir=$target_work_dir/simulator/memory
	tools_dir=$ezdk_base/tools
	

	host_app_ip=127.0.0.1
	host_app_vpci_port=4509
	echo "host app ip: $host_app_ip"
	echo "target frames dir: $target_frames_dir"
	echo "host app vpci port: $host_app_vpci_port"
	echo "logs dir: $target_work_out_logs_dir"
	echo "===================================="
}

function copy_input_files()
{
	echo "copying input files..."
	cp $target_frames_dir/* $target_work_in_frames_dir
}

function create_workspace()
{
	echo "creating workspace..."	
	rm -rf $target_work_dir
	mkdir -p $target_work_in_frames_dir
	mkdir -p $target_work_out_frames_dir
	mkdir -p $target_work_out_logs_dir
	mkdir -p $target_work_out_memory_dir
	mkdir -p $target_work_cp_dir

}



function run_demo_cp()
{
	echo "running demo cp..."
	
	host_app_flags="--data_if ens3 --if0 name=nps_mac0,ip=192.168.100.1,netmask=255.255.255.0 --if1 name=nps_mac1,ip=192.168.101.1,netmask=255.255.255.0 --if2 name=nps_mac2,ip=192.168.102.1,netmask=255.255.255.0 --if3 name=nps_mac3,ip=192.168.103.1,netmask=255.255.255.0 --port_type 10GE --dp_bin_file $bin_dir/atc_dp_sim --routing_app"
	host_app_bin=$bin_dir/atc_daemon_sim

	if [ ! -f "$host_app_bin" ]; then
		echo "$host_app_bin not found."
		exit
	fi
	$host_app_bin $host_app_flags &
	echo $?
	#print syslog if failed( tail /var/log/syslog)
}


function run_ezsim()
{
	ezsim_bin=$tools_dir/EZsim/bin/EZsim_linux_x86_64
	eztap_bin=$tools_dir/EZtap/bin/EZtap_linux_x86_64
	kernel_sim_dir=$ezdk_base/ldk/images/sim
	
	#EZsim dp flags
	ezsim_flags="-frame_in $target_work_in_frames_dir "
	ezsim_flags+="-frame_out $target_work_out_frames_dir "
	ezsim_flags+="-frame_in_file_prefix frames_in_ "
	ezsim_flags+="-frame_out_file_prefix frames_out_ "
	
	#EZsim cp flags
	ezsim_flags+="-vpci_connect "
	ezsim_flags+="-vpci_host_name $host_app_ip "
	ezsim_flags+="-vpci_port $host_app_vpci_port "
	ezsim_flags+="-vpci_interrupt_port $(($host_app_vpci_port + 1)) "
	
	#EZsim output flags
	ezsim_flags+="-output $target_work_out_logs_dir "
	ezsim_flags+="-memory_out $target_work_out_memory_dir "
	ezsim_flags+="-log_mask 0x40010f -job_trace 0x00001F "
#-job_trace 0x00000C
	
	#EZsim network flags
	ezsim_flags+="-net_if_connect true "
	ezsim_flags+="-net_if_create_cmd \"sudo $eztap_bin -ip 192.168.0.0 -mask 255.255.255.0\" "
	
	#EZsim serial flags
	ezsim_flags+="-serial_if_connect true "
	ezsim_flags+="-serial_if_create true "
	ezsim_flags+="-serial_console_app_cmd \"/usr/bin/xterm -title 'EZsim console' -e 'minicom -o -c on -w -p <serial_if_name>'\" "
	
	#EZsim linux flags
	ezsim_flags+="-possible_cpus 0,16-31 -present_cpus 0,16 "
	ezsim_flags+="-flash_image $kernel_sim_dir/flash.img "

	#run EZsim
	echo "running EZsim..."
	eval $ezsim_bin $ezsim_flags >& $target_work_out_logs_dir/EZsim_stdout.log &
	ezsim_pid=$!
}


function wait_for_ezsim()
{
	echo "waiting for EZsim to start..."
	sleep 5
}



#########################################
#                                       #
#              Main                     #
#                                       #
#########################################
trap "exit" INT TERM
trap "kill 0" EXIT
parse_cmd $@

echo $PATH | grep -q  "/.autodirect/sw_tools/OpenSource/minicom/INSTALLS/minicom-2.7/linux_x86_64/bin"
if [ -n "$?" ] ;
	then export PATH=$PATH:/.autodirect/sw_tools/OpenSource/minicom/INSTALLS/minicom-2.7/linux_x86_64/bin;
fi 

set_global_variables
create_workspace
copy_input_files
set -m
run_demo_cp
run_ezsim
wait_for_ezsim
fg %1
kill %2


