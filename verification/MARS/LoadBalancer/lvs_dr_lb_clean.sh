#!/bin/bash

function clean_setup_ipvs()
{
    echo "Enaable ipv4 forwarding"
    echo "1" >/proc/sys/net/ipv4/ip_forward

    # disable ipvsadm
    echo "check ipvs adminstrator off"
    chkconfig ipvsadm off

    echo "clear ipvs table"
    ipvsadm -C

    echo "remove virtual interface configuration"
    ifconfig eth0:0 down
}


#########################################
#                                       #
#              Main                     #
#                                       #
#########################################
clean_setup_ipvs
echo "All done!"

