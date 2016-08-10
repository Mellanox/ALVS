#!/bin/bash

trap "exit" INT TERM
trap "kill 0" EXIT

if [ "$1" == "--help" ] 
	then
		echo "Need to set the ezbox name to run"
		echo "Example:"
		echo "run_regression_unit_tests.sh ezbox55"
		echo "to run on debug mode add -debug"
		echo "to hard reset before starting add -reset"
		exit 0
fi

if test -z "$1"
	then
		echo "Need to set the ezbox name to run"
		echo "Example:"
		echo "run_regression_unit_tests.sh ezbox55"
		exit 1
fi


echo;echo "Run regression tests on ezbox $1 ..."    


#echo;echo "Run alvs_cp_check_agt_port.py..."
#verification/testing/cp/alvs_cp_check_agt_port.py -ezbox $1 $2 || exit

#echo;echo "Run arp_table_test.py..."
#./arp_table_test.py -ezbox $1 -scenarios 1,2,3,6,7,8 $2 || exit #todo-change scenarios to run after fixing bug

#echo;echo "Run entry_state_change.py..."
#./arp_entry_state_change.py -ezbox $1 $2 || exit



list_of_tests="
			   #dp_packet_to_host_test.py
			   #lag_test.py
			   #connection_established_delete_test.py
			   #host_to_network_test.py
			   schedule_algorithm_test.py
			   tcp_flags_test.py
			   server_fail_test.py
                           1service_1server_overloaded_flag.py
                           1sevice_1server_overloaded_flag_fallback.py
			   "
			
failed_tests=""

for test_name in $list_of_tests 
do
	echo;echo "Run $test_name..."
	verification/testing/dp/$test_name -ezbox $1 $2 $3
	rc=$?; if [[ $rc != 0 ]]; then failed_tests+="$test_name "; fi
done

# check for error tests
echo;echo
if [[ $failed_tests != "" ]] 
	then 
		echo "Regression failed, failed tests are:"
		for test_name in $failed_tests 
		do
			echo "$test_name"
		done
		echo
		exit 1
fi

exit 0
