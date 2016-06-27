#!/bin/bash

###############################################################################

function set_global_vars {
	echo "Check existence of ALVS_COVERITY_RES_DIR variable, This variable will control Coverity intermidiate and results directory location"
	if [ "$ALVS_COVERITY_RES_DIR" != "" ]
	then
		echo "Variable ALVS_COVERITY_RES_DIR already exists!!!"
	else
		echo "Variable ALVS_COVERITY_RES_DIR does not exist!!!"
		echo "Using default ALVS_COVERITY_RES_DIR"
		ALVS_COVERITY_RES_DIR=/tmp/ALVS_COVERITY_RES_DIR_$$_$(hostname)
	
	fi
	echo "ALVS_COVERITY_RES_DIR = $ALVS_COVERITY_RES_DIR"

	ALVS_COVERITY_RES_DIR_CP=$ALVS_COVERITY_RES_DIR/cp
	ALVS_COVERITY_RES_DIR_DP=$ALVS_COVERITY_RES_DIR/dp
	LOG_FILE=$ALVS_COVERITY_RES_DIR/coverity.log
	CP_CONFIG_DIR=$ALVS_COVERITY_RES_DIR_CP/config
	DP_CONFIG_DIR=$ALVS_COVERITY_RES_DIR_DP/config
}

###############################################################################

function creating_intermidiate_dir {
	if [ -d "$ALVS_COVERITY_RES_DIR" ]
	then
		echo "Removing old directory: $ALVS_COVERITY_RES_DIR..."
		rm -rf $ALVS_COVERITY_RES_DIR
	fi

	echo "Creating $ALVS_COVERITY_RES_DIR for Coverity results..."
	mkdir $ALVS_COVERITY_RES_DIR

	echo "Creating log file: $LOG_FILE"
	touch $LOG_FILE

}

###############################################################################

function unset_global_vars {
	unset ALVS_COVERITY_RES_DIR
	unset ALVS_COVERITY_RES_DIR_CP
	unset ALVS_COVERITY_RES_DIR_DP
	unset LOG_FILE
	unset CP_CONFIG_DIR
	unset DP_CONFIG_DIR
}

###############################################################################

function run_coverity_cp {

	ALVS_ACCEPTED_FP_CP=1

	echo "Creating $ALVS_COVERITY_RES_DIR_CP for CP Coverity intermidiate and results directory..." | tee -a $LOG_FILE
	mkdir $ALVS_COVERITY_RES_DIR_CP &>> $LOG_FILE

	echo "Creating $CP_CONFIG_DIR for CP Coverity config directory..." | tee -a $LOG_FILE
	mkdir $CP_CONFIG_DIR &>> $LOG_FILE

	echo "Creating compiler configuraion for CP..." | tee -a $LOG_FILE
	cov-configure --config $CP_CONFIG_DIR/cp_config.xml --gcc &>> $LOG_FILE

	DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
	makefile_dir="$DIR/../../"

	echo "Running CP clean project..." | tee -a $LOG_FILE
	make -C $makefile_dir cp-clean &>> $LOG_FILE

	echo "Build CP under Coverity..." | tee -a $LOG_FILE
	cov-build --config $CP_CONFIG_DIR/cp_config.xml --dir $ALVS_COVERITY_RES_DIR_CP make -C $makefile_dir cp &>> $LOG_FILE

	echo "Running CP Coverity Analysis..." | tee -a $LOG_FILE
	cov-analyze --config $CP_CONFIG_DIR/cp_config.xml --dir $ALVS_COVERITY_RES_DIR_CP --all --aggressiveness-level high -j 2 &>> $LOG_FILE

	echo -e "\n******************************* Generating CP Coverity static error report *********\n" | tee -a $LOG_FILE
	res=$(cov-format-errors --dir $ALVS_COVERITY_RES_DIR_CP --emacs-style --file "ALVS" --exclude-files 'sqlite3\.c' --functionsort | tee -a $LOG_FILE)	
	if [ "$res" == "" ]
	then
		echo -e "\n\n******************************* CP Coverity Result is PASS ***********************\n"
		return 0
	else
		error_num=$(cov-format-errors --dir $ALVS_COVERITY_RES_DIR_CP --emacs-style --file "ALVS" --exclude-files 'sqlite3\.c'  --functionsort | tee /dev/tty | grep -wc "Error:")
		if [ "$error_num" == "$ALVS_ACCEPTED_FP_CP" ]
		then
			echo -e "\nCurrently we'll accept these $ALVS_ACCEPTED_FP_CP ERRORS in CP as false positivies!!! we'll handle them in Coverity Portal later."
			echo -e "\n\n******************************* CP Coverity Result is PASS ***********************\n"
			return 0
		elif [ "$error_num" -lt "$ALVS_ACCEPTED_FP_CP" ]
		then
			echo -e "\nPay attention that number of errors ($error_num) is less than accepted number of false positives ($ALVS_ACCEPTED_FP_CP)."
		else
			echo -e "\nPay attention that number of errors ($error_num) is greater than accepted number of false positives ($ALVS_ACCEPTED_FP_CP)."
		fi
		echo -e "\n\n******************************* CP Coverity Result is FAIL ***********************\n"
		return 1
	fi
}

###############################################################################

function run_coverity_dp {

	ALVS_ACCEPTED_FP_DP=1

	echo "Creating $ALVS_COVERITY_RES_DIR_DP for DP Coverity intermidiate and results directory..." | tee -a $LOG_FILE
	mkdir $ALVS_COVERITY_RES_DIR_DP &>> $LOG_FILE

	echo "Creating $DP_CONFIG_DIR for DP Coverity config directory..." | tee -a $LOG_FILE
	mkdir $DP_CONFIG_DIR &>> $LOG_FILE

	echo "Creating compiler configuraion for DP..." | tee -a $LOG_FILE
	cov-configure  --config $DP_CONFIG_DIR/dp_config.xml -co /auto/nps_release/EZdk/EZdk-2.0a-patch-1.0.0/ldk/toolchain/bin/arceb-linux-gcc -- -mq-class -mlong-calls -mbitops -munaligned-access -mcmem -mswape &>> $LOG_FILE

	DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
	makefile_dir="$DIR/../../"

	echo "Running DP clean project..." | tee -a $LOG_FILE
	make -C $makefile_dir dp-clean &>> $LOG_FILE

	echo "Build DP under Coverity..." | tee -a $LOG_FILE
	cov-build --config $DP_CONFIG_DIR/dp_config.xml --dir $ALVS_COVERITY_RES_DIR_DP make -C $makefile_dir dp &>> $LOG_FILE

	echo "Running DP Coverity Analysis..." | tee -a $LOG_FILE
	cov-analyze --config $DP_CONFIG_DIR/dp_config.xml --dir $ALVS_COVERITY_RES_DIR_DP --all --aggressiveness-level high -j 2  &>> $LOG_FILE

	echo -e "\n******************************* Generating DP Coverity static error report *********\n" | tee -a $LOG_FILE
	res=$(cov-format-errors --dir $ALVS_COVERITY_RES_DIR_DP --emacs-style --file "ALVS"  --functionsort | tee -a $LOG_FILE)	
	if [ "$res" == "" ]
	then
		echo -e "\n\n******************************* DP Coverity Result is PASS ***********************\n"
		return 0
	else
		error_num=$(cov-format-errors --dir $ALVS_COVERITY_RES_DIR_DP --emacs-style --file "ALVS"  --functionsort | tee /dev/tty | grep -wc "Error:")
		if [ $error_num == "$ALVS_ACCEPTED_FP_DP" ]
		then
			echo -e "\nCurrently we'll accept these $ALVS_ACCEPTED_FP_DP ERRORS in DP as false positivies!!! we'll handle them in Coverity Portal later."
			echo -e "\n\n******************************* DP Coverity Result is PASS ***********************\n"
			return 0
		elif [ "$error_num" -lt "$ALVS_ACCEPTED_FP_DP" ]
		then
			echo -e "\nPay attention that number of errors ($error_num) is less than accepted number of false positives ($ALVS_ACCEPTED_FP_DP)."
		else
			echo -e "\nPay attention that number of errors ($error_num) is greater than accepted number of false positives ($ALVS_ACCEPTED_FP_DP)."
		fi
		echo -e "\n\n******************************* DP Coverity Result is FAIL ***********************\n"
		return 1
	fi

}

function check_arg {
	
	if [ "$1" != "DP" ] && [ "$1" != "CP" ] && [ "$1" != "" ]
	then
		echo "BAD Arg1: $1, valid values: CP | DP | EMPTY "
		return	1
	elif [ "$2" != "nocleanup" ] && [ "$2" != "" ]
	then
		echo "BAD Arg2: $2, valid values: nocleanup | EMPTY "
		return	1
	fi
	
	return 0

}

###############################################################################

function run_coverity {
	
	echo -e "\n*************************************************************************************"
	echo "******************************* Start Running Coverity ******************************"
	echo -e "*************************************************************************************\n\n"
	
	echo -e "******************************* Start Checking Arguments ****************************\n"
	echo "Arg1 valid values: EMPTY (Default: both CP & DP) | CP | DP"
	echo "Arg2 valid values: EMPTY (Default: remove intermidiate directory) | nocleanup (keep intermidiate directory)"

	check_arg $1 $2
	if [ "$?" == 1 ]
	then
		return 1
	fi
	
	echo -e "\n******************************* Start Creating Intermidiate Directory ***************\n"
	set_global_vars
	creating_intermidiate_dir
	

	echo -e "\n******************************* Setting Coverity Version ***************************\n"
	export COVERITY_DIR=/.autodirect/app/Coverity/cov-analysis-linux64-7.6.0
	echo "Coverity version: $COVERITY_DIR" | tee -a $LOG_FILE
	export PATH=${COVERITY_DIR}/bin:${PATH}
	
	final_res=1
	if [ "$1" == "DP" ]
	then
		echo -e "\n******************************* Running Coverity on DP only ************************\n" | tee -a $LOG_FILE
		run_coverity_dp
		final_res=$?
	elif [ "$1" == "CP" ]
	then
		echo -e "\n******************************* Running Coverity on CP only ************************\n" | tee -a $LOG_FILE
		run_coverity_cp
		final_res=$?
	else
		echo -e "\n******************************* Running Coverity on CP *****************************\n" | tee -a $LOG_FILE
		run_coverity_cp
		final_res=$?
		echo -e "\n******************************* Running Coverity on DP *****************************\n" | tee -a $LOG_FILE
		run_coverity_dp
		final_res=$(( $final_res + $? ))
	fi
	
	if [ "$2" == "" ]
	then
		echo "Removing intermidiate directory: $ALVS_COVERITY_RES_DIR"
		echo "You can keep it by using Arg2=nocleanup"
		rm -rf $ALVS_COVERITY_RES_DIR
	fi

	if [ $final_res == "0" ]
	then
		echo -e "\n\n******************************* Coverity Result is PASS **************************\n"
	else
		echo -e "\n\n******************************* Coverity Result is FAIL **************************\n"
	fi
	
	unset_global_vars
	return $final_res

}

###############################################################################
 #   DEBUG=TRUE
 #  export DEBUG
run_coverity $1 $2

 #   unset DEBUG


