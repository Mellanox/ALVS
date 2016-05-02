#!/bin/bash

function usage()
{
	cat <<EOF
Usage: $script_name [VIP]
This script runs host target with hello_packet_target input configuration
VIP:           virtual IP address
server Name:   server name (used by index.xhml


Examples:
$script_name 192.168.1.100 nps001

EOF
   exit 1
}


function parse_cmd()
{
	test $# -ne 2 && usage
	vip=$1
	servername=$2
		
	echo "virtual IP = $vip"	
}

function setup_ipvs()
{
    #install http server daemon & start it 
    echo "install http daemon server..."
    yum install httpd -y
    echo "starting http daemon server..."
    service httpd start
    chkconfig httpd on

    #configure loopback port
    echo "configuring loopback port..." 
    ifconfig lo:0 $vip netmask 255.255.255.255

    #setup arp configuration
    echo "0" >/proc/sys/net/ipv4/ip_forward

    echo "1" >/proc/sys/net/ipv4/conf/all/arp_ignore
    echo "1" >/proc/sys/net/ipv4/conf/eth0/arp_ignore

    echo "2" >/proc/sys/net/ipv4/conf/all/arp_announce
    echo "2" >/proc/sys/net/ipv4/conf/eth0/arp_announce

    echo "set index.xml"
    echo $servername >/var/www/html/index.html


}


#########################################
#                                       #
#              Main                     #
#                                       #
#########################################
parse_cmd $@
setup_ipvs
echo "All done!"


