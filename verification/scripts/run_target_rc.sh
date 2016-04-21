#!/bin/bash

#######################################################################################
#
#                    Copyright by EZchip Technologies, 2016
#
#
#  Project:         demo_target_rc
#  Description:     This script runs real chip with demo target input configuration
#  Notes:           on each run all work directory contents are being deleted at the begining
#
#######################################################################################

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
Usage: $script_name [CP_BIN_PATH] [DP_BIN_PATH] [HOST_IP] [CHIP_IP]
This script runs host target with hello_packet_target input configuration
CP_BIN_PATH:             path to CP bin file
DP_BIN_PATH:             path to DP bin file
HOST_IP:                 host box IP address
CHIP_IP:                 NPS IP address

Examples:
$script_name ALVS/bin/alvs_cp.bin ALVS/bin/alvs_dp.bin ezbox-host-ip ezbox-chip-ip

EOF
   exit 1
}


function parse_cmd()
{
    # check number of arguments
    test $# -ne 4 && usage
    
    # move arguments to variables
    cp_bin_path="$(pwd)/$1"
    dp_bin_path="$(pwd)/$2"
    host_ip=$3
    chip_ip=$4

    # print arguments
    echo "CP bin path = $cp_bin_path"    
    echo "DP bin path = $dp_bin_path"    
    echo "box host ip = $host_ip"
    echo "nps ip      = $chip_ip"    
}


function set_global_variables()
{    
    target_dir=$(dirname $script_dir)
    base_dir=$(dirname $target_dir)
    target_work_dir=$target_dir/work    
    target_work_cp_dir=$target_work_dir/control_plane
    target_work_out_logs_dir=$target_work_dir/out/logs
}

function create_workspace()
{
    echo "creating local workspace..."    
    rm -rf $target_work_dir
    mkdir -p $target_work_cp_dir        
    mkdir -p $target_work_out_logs_dir
}

function reset_box()
{
    echo "connecting and resetting chip $chip_ip..."    
    sshpass -p ezchip ssh root@$host_ip nps_power -r por >& $target_work_out_logs_dir/nps_connect.log
}

function copy_cp_app()
{
    #copy CP application using ftp to host         
    echo "uploading CP application bin to $host_ip..."
    sshpass -p ezchip scp $cp_bin_path root@$host_ip:/tmp/cp_app
    retval=$?    
    if [ $retval -eq 0 ]; then
        echo "CP application copied to $host_ip succesfully."
    else
        echo "Fail copying CP application to $host_ip. exiting..."
        exit $retval
    fi    
}

function run_cp_app()
{
    echo "running CP application..."
    
    # run CP on new terminal 
    gnome-terminal --geometry 80x24+0+80 --title="Control Plan" -x bash -c "$script_dir/run_cp_rc.sh $host_ip ; bash" &
    
    #wait for CP to finish running
    echo "waiting for CP application to load..." 
    sleep 5
}

wait_for_chip()
{
    echo "waiting for $chip_ip network..."    
    while ! ping -c1 $chip_ip &>/dev/null; do :; done
    echo "nps is reachable..."
    sleep 5
}

function copy_dp_app()
{
    USER="root"
    echo "uploadingDP application bin to $chip_ip..."
    ftp -n $chip_ip <<END_FTP
    user $USER
    put $dp_bin_path /tmp/dp_app
    quit
END_FTP
}

open_terminals()
{
    # where can I see terminal. need to open terminal from our linux server? automatic opened?
    echo "opening terminal ssh connection to host..."    
    gnome-terminal --geometry 80x24+660+80 --title="$host_ip" -x bash -c "sshpass -p ezchip ssh root@$host_ip" &
    sleep 1
    echo "opening serial connection to chip..."
    gnome-terminal --geometry 80x24+1320+80 --title="$chip_ip" -x bash -c "telnet $chip_ip ; bash" &
}

function set_dp_app_perm()
{
    echo "change DP application permission..."    
    { echo "cd /tmp"; echo "chmod +x dp_app"; sleep 1;} | nohup telnet $chip_ip >& $target_work_out_logs_dir/telnet.log      
}

#########################################
#                                       #
#              Main                     #
#                                       #
#########################################
trap "exit" INT TERM
trap "kill 0" EXIT
echo "** Start script **"
parse_cmd $@
set_global_variables
create_workspace
reset_box
copy_cp_app
run_cp_app
wait_for_chip
copy_dp_app
open_terminals
set_dp_app_perm
echo "All done!"
echo "** End script **"
$SHELL
