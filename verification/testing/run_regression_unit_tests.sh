#!/bin/bash

trap "exit" INT TERM
trap "kill 0" EXIT

echo "Run regression tests on ezbox $1 ..."    

cd cp/
echo;echo "Run alvs_cp_check_agt_port.py..."
./alvs_cp_check_agt_port.py -ezbox $1 $2 || exit

echo;echo "Run arp_table_test.py..."
./arp_table_test.py -ezbox $1 -scenarios 1,2,3,6,7,8 $2 || exit #todo-change scenarios to run after fixing bug

echo;echo "Run entry_state_change.py..."
./arp_entry_state_change.py -ezbox $1 $2 || exit

cd ..

echo;echo "Run dp_packet_to_host_test.py..."
./dp/dp_packet_to_host_test.py -ezbox $1 $2 || exit
