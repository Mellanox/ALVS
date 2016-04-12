#!/bin/bash

function clean_setup_ipvs()
{
    echo "Stop HTTP daemon"
    # ---------------------
    service httpd stop

    echo "Resore loopback port configuration"
    # ---------------------
    ifconfig lo:0 down

    echo "restore ARP configuration"
    # ------------------------------
    echo "1" >/proc/sys/net/ipv4/ip_forward

    echo "0" >/proc/sys/net/ipv4/conf/all/arp_ignore
    echo "0" >/proc/sys/net/ipv4/conf/eth0/arp_ignore

    echo "0" >/proc/sys/net/ipv4/conf/all/arp_announce
    echo "0" >/proc/sys/net/ipv4/conf/eth0/arp_announce
    
    echo "remove index.xml"
    rm /var/www/html/index.html

}


#########################################
#                                       #
#              Main                     #
#                                       #
#########################################
cho "Cleaning HTTP server setup for IPVS"
clean_setup_ipvs
echo "All done!"


