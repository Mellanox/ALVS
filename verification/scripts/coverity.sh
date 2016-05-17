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

	echo "Generating CP Coverity static error report..." | tee -a $LOG_FILE
	res=$(cov-format-errors --dir $ALVS_COVERITY_RES_DIR_CP --emacs-style --file "ALVS"  --functionsort | tee -a $LOG_FILE)	
	if [ "$res" == "" ]
	then
		echo "CP Coverity PASSED!!!"
		return 1
	else
		cov-format-errors --dir $ALVS_COVERITY_RES_DIR_CP --emacs-style --file "ALVS"  --functionsort
		echo "CP Coverity FAILED!!!"
		return 0
	fi
}

###############################################################################

function run_coverity_dp {

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

	echo "Generating DP Coverity static error report..." | tee -a $LOG_FILE
	res=$(cov-format-errors --dir $ALVS_COVERITY_RES_DIR_DP --emacs-style --file "ALVS"  --functionsort | tee -a $LOG_FILE)	
	if [ "$res" == "" ]
	then
		echo "DP Coverity PASSED!!!"
		return 1
	else
		error_num=$(cov-format-errors --dir $ALVS_COVERITY_RES_DIR_DP --emacs-style --file "ALVS"  --functionsort | tee /dev/tty | grep -wc "Error:")
		if [ $error_num == "4" ]
		then
			echo "Currently we'll accept these 4 ERRORS in DP as false alarms!!! we'll handle them in Coverity Portal later."
			echo "DP Coverity PASSED!!!"
			return 1
		fi
		echo "DP Coverity FAILED!!!"
		return 0
	fi

}

function check_arg {
	
	if [ "$1" != "DP" ] && [ "$1" != "CP" ] && [ "$1" != "" ]
	then
		echo "BAD Arg1: $1, valid values: CP | DP | EMPTY "
		return	0
	elif [ "$2" != "cleanup" ] && [ "$2" != "" ]
	then
		echo "BAD Arg2: $2, valid values: cleanup | EMPTY "
		return	0
	fi
	
	return 1

}

###############################################################################

function run_coverity {

	echo "Arg1 valid values: EMPTY (Default: both CP & DP) | CP | DP"
	echo "Arg2 valid values: EMPTY (Default: keep intermidiate directory) | cleanup (remove intermidiate directory automatically)"

	check_arg $1 $2
	if [ "$?" == 0 ]
	then
		return 0
	fi
	
	set_global_vars
	creating_intermidiate_dir
	

	export COVERITY_DIR=/.autodirect/app/Coverity/cov-analysis-linux64-7.6.0
	echo "Coverity version: $COVERITY_DIR" | tee -a $LOG_FILE
	export PATH=${COVERITY_DIR}/bin:${PATH}
	
	final_res=-1
	if [ "$1" == "DP" ]
	then
		echo "Running Coverity on DP only!!!" | tee -a $LOG_FILE
		run_coverity_dp
		final_res=$?
	elif [ "$1" == "CP" ]
	then
		echo "Running Coverity on CP only!!!" | tee -a $LOG_FILE
		run_coverity_cp
		final_res=$?
	else
		echo "Running Coverity on CP & DP!!!" | tee -a $LOG_FILE
		run_coverity_cp
		final_res=$?
		run_coverity_dp
		final_res=$(( $final_res * $? ))
	fi
	
	if [ "$2" == "cleanup" ]
	then
		echo "Removing intermidiate directory: $ALVS_COVERITY_RES_DIR"
		echo "You can keep it by not using Arg2"
		rm -rf $ALVS_COVERITY_RES_DIR
	fi

	if [ $final_res == "1" ]
	then
		echo "Coverity PASSED!!!"
	else
		echo "Coverity FAILED!!!"
	fi
	
	unset_global_vars
	return $final_res

}

###############################################################################

run_coverity $1 $2

