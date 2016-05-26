#!/bin/bash

trap "exit" INT TERM
trap "kill 0" EXIT

echo "Run regression tests on ezbox $1 ..."    
echo;echo "Run alvs_cp_check_agt_port.py..."
./alvs_cp_check_agt_port.py -ezbox $1 $2 || exit
echo;echo "Run arp_table_test.py..."
./arp_table_test.py -ezbox $1 -scenarios 1,2,3,6,7,8 $2 || exit
echo;echo "Run entry_state_change.py..."
./entry_state_change.py -ezbox $1 $2 || exit



# not ready tests
#echo;echo "Run alvs_unsupported_features_test.py..."
#./alvs_unsupported_features_test.py -ezbox $1 $2 || exit
#
#echo;echo "Run cp_error_testing.py..."
#./cp_error_testing.py -ezbox $1 $2 || exit
#
#echo;echo "Run kill_cp_app_test.py..."
#./kill_cp_app_test.py -ezbox $1 $2 || exit

