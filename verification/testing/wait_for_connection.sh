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

trap "exit" INT TERM
#trap "kill 0" EXIT

echo "waiting for connectivity with $1 ..."    
while ! ping -c1 $1 &>/dev/null; do :; done
echo "nps is reachable..."
sleep 5
