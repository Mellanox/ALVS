#!/bin/bash

function clean_setup_ipvs()
{
    echo "Enable ipv4 forwarding"
    echo "1" >/proc/sys/net/ipv4/ip_forward

    # stop connection sync daemons
    echo "stop ipvs connection sync daemons"
    ipvsadm --stop-daemon master
    ipvsadm --stop-daemon backup

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

