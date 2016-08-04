#!/bin/bash

function usage()
{
    cat <<EOF
Usage: $script_name [IP]

This script porpuse is configure new VM
Script set static IP on ens6

IP: internal IP

Examples:
$script_name 192.168.0.50

EOF
   exit 1
}


#######################################################################################

function parse_cmd()
{
    # check number of arguments
    test $# -ne 1 && usage
    
    # move arguments to variables
    ip=$1
}

parse_cmd $@
echo "Set VM with IP: $ip"

# set NIC
ifconfig ens6 $ip netmask 255.255.0.0

# Set permanent configuration (for VM restart)
file_name="/etc/sysconfig/network-scripts/ifcfg-ens6"
echo -e "DEVICE=\"ens6\"\nONBOOT=yes\nBOOTPROTO=none\nTYPE=Ethernet\nIPADDR=$ip\nNETMASK=255.255.0.0" > $file_name



echo -e "script Ended for $ip \n"
