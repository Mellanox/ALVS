#!/bin/bash

function usage()
{
	cat <<EOF
Usage: $script_name [VIP] [BRDCAST] [NETMASK] [SCHD_ALGO] [S1_RIP] [S2_RIP] [ROLE]
This script runs host target with hello_packet_target input configuration
VIP:    			    virtual IP address
BRDCAST:                broadcast address
NETMASK:  	            netmask of VIP
SCHD_ALGO:              scheduling algorithm (rr,wrr etc.)
S1_RIP:                 real server 1 IP address 
S2_RIP:                 real server 2 IP address
ROLE:                   master\backup role

Examples:
$script_name 192.168.1.100 192.168.1.254 255.255.255.0 rr 192.168.1.10 192.168.1.11 master

EOF
   exit 1
}


function parse_cmd()
{
	test $# -ne 7 && usage
	vip=$1
	brdcst_ip=$2
	netmask=$3
	schd_algo=$4
	s1_rip=$5
	s2_rip=$6
        role=$7
		
	echo "vip       = $vip"
	echo "brdcst_ip = $brdcst_ip"		
	echo "netmask   = $netmask"
	echo "schd_algo = $schd_algo"
	echo "s1_rip    = $s1_rip"
	echo "s2_rip    = $s2_rip"
	echo "role	= $role"
}


function setup_ipvs()
{
    #set ipv4 forwarding to off --> /etc/sysctl.conf --> sysctl -p
    echo "disable ipv4 forwarding..."
    echo "0" >/proc/sys/net/ipv4/ip_forward

    #install ipvsadm
    echo "install ipvs adminstrator..."
    yum install ipvsadm -y

    #enable ipvsadm
    echo "check ipvs adminstrator..."
    chkconfig ipvsadm on

    #set up new virtual interface and bring it up if we're master
    if [ "$role" == "master" ]; then
        echo "config new virtual interface..."
        ifconfig eth0:0 $vip broadcast $brdcst_ip netmask $netmask;
    fi

    #clear ipvs tables
    echo "clear ipvs table..."
    ipvsadm -C

    #add new service 
    echo "add new service to ipvs table ..."
    ipvsadm -A -t $vip:80 -s $schd_algo

    #add real server 1 to service
    echo "add real server 1 to service ..."
    ipvsadm -a -t $vip:80 -r $s1_rip -g -w 1

    #add real server 2 to service
    echo "add real server 2 to service ..."
    ipvsadm -a -t $vip:80 -r $s2_rip -g -w 1

    #start connection state synchronization
    echo "start $role connection state sync daemon"
    ipvsadm --start-daemon=$role --mcast-interface=eth0

    #ping and check alive
    #need to add configuration for ldirectord and heartbeat in the future
}


#########################################
#                                       #
#              Main                     #
#                                       #
#########################################
parse_cmd $@
setup_ipvs
echo "All done!"


