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

#script_name=$(basename $0)
#script_dir=$(cd $(dirname $0) && pwd)

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
    test $# -ne 2 && usage
    
    # move arguments to variables
    dp_bin_path="$1"
    chip_ip=$2

    # print arguments
    echo "DP bin path = $dp_bin_path"    
    echo "nps ip      = $chip_ip"    
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
    put $dp_bin_path /tmp/alvs_dp
    quit
END_FTP
}


function set_dp_app_perm()
{
    echo "change DP application permission..."    
    { echo "cd /tmp"; echo "chmod +x alvs_dp"; sleep 1;} | telnet $chip_ip  
}

#########################################
#                                       #
#              Main                     #
#                                       #
#########################################
trap "exit" INT TERM
#trap "kill 0" EXIT

echo "** Start script **"
parse_cmd $@
wait_for_chip
copy_dp_app
set_dp_app_perm
echo "Run DP app"
{ echo "/tmp/alvs_dp &"; sleep 3;} | telnet $chip_ip
sleep 1
echo "All done!"
echo "** End script **"
