#!/bin/bash

###############################################################################

function check_arg {
	
	if [ "$app" != "ALVS" ]
	then
		echo "BAD_ARG #1: $app valid values: ALVS!!!"	
		return 1
	fi

	if [[ ("${cp_or_dp}" != "cp") &&  ("${cp_or_dp}" != "dp") ]]
	then
		echo "BAD_ARG #2: ${cp_or_dp} valid values are cp | dp!!!"	
		return 1
	fi
	
	if [ "$3" == "" ]
	then
		echo "BAD_ARG #3: valid value: name of tested src file without extention, i.e. alvs_db_manager for src/cp/alvs_db_manager!!!"	
		return 1
	fi

	for src in "${@:3}"; do
		echo "check existence of ${cp_or_dp} tested src file $src ..."	
		if [ ! -e ${alvs_root}/src/${cp_or_dp}/${src}.c ]
		then
			echo "BAD_ARG: ${cp_or_dp} tested src file ${alvs_root}/src/${cp_or_dp}/${src}.c is not exist!!!"	
			return 1
		fi
	done
	return 0
}
###############################################################################

function set_args {
	
	app=$1
	cp_or_dp=$2
	script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
	alvs_root=$(readlink -e ${script_dir}/../../../)
	src_file=${alvs_root}/src/${cp_or_dp}/${3}.c
	output_dir=${alvs_root}/verification/venus/output/${cp_or_dp}/$3
	log=${output_dir}/${3}.log
	testing_dir=${alvs_root}/verification/venus/
}
###############################################################################

function print_start_msg {

	echo -e "\n*************************************************************************************" |& tee -a $log
	echo "************************** Start Running make_venus.sh ******************************" |& tee -a $log
	echo -e "*************************************************************************************\n" |& tee -a $log
	
	echo -e "******************************* Start Checking Arguments ****************************" |& tee -a $log
	echo -e "Arg1 valid value: application name. currently only ALVS is supported\n" |& tee -a $log
	echo -e "Arg2 valid value: cp | dp. cp / dp tested src file \n" |& tee -a $log
	echo -e "Arg3 valid value: name of tested src file without extention, i.e., alvs_db_manager for src/cp/alvs_db_manager.c\n" |& tee -a $log
	echo -e "Arg4- valid value: list of src files without extention which ARG2 src file functions may depends on, i.e., main for src/cp/main.c\n" |& tee -a $log
	echo "Running this script to create model files & mock environment..." |& tee -a $log
	echo "ALVS root: $alvs_root" |& tee -a $log
	echo "cp src file: $src_file" |& tee -a $log
	
	echo -e "Output dir: $output_dir" |& tee -a $log
	echo -e "log file: $log\n" |& tee -a $log
	echo -e "testing dir: $testing_dir" |& tee -a $log	
}
###############################################################################

function cp_run_cparser {

	echo -e "Creating glibc model ...\n" |& tee -a $log
	
	echo "/swgwork/yohadd/unittestAPI/tools/Cparser/cparser.py -o $output_dir/glibc_model --external_lib /usr/include/stdio.h /usr/include/stdint.h /usr/include/stdlib.h /usr/include/string.h" |& tee -a $log
	python2.7 /swgwork/yohadd/unittestAPI/tools/Cparser/cparser.py -o $output_dir/glibc_model --external_lib /usr/include/stdio.h /usr/include/stdint.h /usr/include/stdlib.h /usr/include/string.h |& tee -a $log
	if [ "$?" != 0 ]
	then
		echo "ERROR: failed in create glibc model!!!" |& tee -a $log
		return 1
	fi

	echo "/swgwork/yohadd/unittestAPI/tools/Cparser/cparser.py -o $output_dir/pthread_model --external_lib /usr/include/pthread.h" |& tee -a $log
	python2.7 /swgwork/yohadd/unittestAPI/tools/Cparser/cparser.py -o $output_dir/pthread_model --external_lib /usr/include/pthread.h |& tee -a $log
	if [ "$?" != 0 ]
	then
		echo "ERROR: failed in create pthread model!!!" |& tee -a $log
		return 1
	fi

	echo "/swgwork/yohadd/unittestAPI/tools/Cparser/cparser.py -o $output_dir/libgnl3_model -p \"-I/usr/include/libnl3\" --external_lib /usr/include/libnl3/netlink/msg.h /usr/include/libnl3/netlink/genl/mngt.h /usr/include/libnl3/netlink/genl/genl.h /usr/include/libnl3/netlink/genl/ctrl.h /usr/include/libnl3/netlink/netlink-compat.h" |& tee -a $log
	python2.7 /swgwork/yohadd/unittestAPI/tools/Cparser/cparser.py -o $output_dir/libgnl3_model -p "-I/usr/include/libnl3" --external_lib /usr/include/libnl3/netlink/msg.h /usr/include/libnl3/netlink/genl/mngt.h /usr/include/libnl3/netlink/genl/genl.h /usr/include/libnl3/netlink/genl/ctrl.h /usr/include/libnl3/netlink/netlink-compat.h |& tee -a $log
	if [ "$?" != 0 ]
	then
		echo "ERROR: failed in create libnl3 model!!!" |& tee -a $log
		return 1
	fi

	app_flag=""
	if [ "$app" == "ALVS" ]
	then
		app_flag="-DCONFIG_ALVS=1"
	fi
	cmd="python2.7 /swgwork/yohadd/unittestAPI/tools/Cparser/cparser.py -o $output_dir/model -p \"-w -Dasm= '-Dvolatile(...)=' -Dmain=my_main -nostartfiles -nodefaultlibs -nostdlib -DALVS_LITTLE_ENDIAN $app_flag -DNDEBUG -O0 -I/usr/include/libnl3 -I${alvs_root}/src/common -I${alvs_root}/src/cp -I/mswg/release/nps/EZdk/EZdk-17.0300/dpe/dp/include -I/mswg/release/nps/EZdk/EZdk-17.0300/cpe/env/include -I/mswg/release/nps/EZdk/EZdk-17.0300/cpe/dev/include -I/mswg/release/nps/EZdk/EZdk-17.0300/cpe/cp/include -I/mswg/release/nps/EZdk/EZdk-17.0300/cpe/agt/agt-cp/include -I/mswg/release/nps/EZdk/EZdk-17.0300/cpe/agt/agt/include\" "
	for src in $@; do
		cmd+="${alvs_root}/src/cp/${src}.c "
	done
	cmd+="|& tee -a $log"
	echo -e "\n$cmd"
	eval $cmd
	if [ "$?" != 0 ]
	then
		echo "ERROR: failed in creating model files!!!" |& tee -a $log
		return 1
	fi
	return 0
}

###############################################################################

function dp_run_cparser {

	echo -e "Creating glibc model ...\n" |& tee -a $log
	echo "/swgwork/yohadd/unittestAPI/tools/Cparser/cparser.py -o $output_dir/glibc_model --external_lib /usr/include/stdio.h /usr/include/stdint.h /usr/include/stdlib.h /usr/include/string.h" |& tee -a $log
	python2.7 /swgwork/yohadd/unittestAPI/tools/Cparser/cparser.py -o $output_dir/glibc_model --external_lib /usr/include/stdio.h /usr/include/stdint.h /usr/include/stdlib.h /usr/include/string.h |& tee -a $log
	if [ "$?" != 0 ]
	then
		echo "ERROR: failed in create glibc model!!!" |& tee -a $log
		return 1
	fi

	app_flag=""
	if [ "$app" == "ALVS" ]
	then
		app_flag="-DCONFIG_ALVS=1"
	fi

	cmd="python2.7 /swgwork/yohadd/unittestAPI/tools/Cparser/cparser.py -o $output_dir/model -p \"-w -Dasm= '-Dvolatile(...)=' -Dmain=my_main -nostdinc -nostartfiles -nodefaultlibs -nostdlib -DBIG_ENDIAN $app_flag -DNDEBUG -O0 -I${alvs_root}/EZdk/ldk/toolchain/lib/gcc/arceb-ezchip-linux-uclibc/4.8.4/include-fixed/ -I${alvs_root}/EZdk/ldk/toolchain/lib/gcc/arceb-ezchip-linux-uclibc/4.8.4/include -I${alvs_root}/EZdk/ldk/toolchain/arceb-ezchip-linux-uclibc/sysroot/usr/include/ -I${alvs_root}/src/common -I${alvs_root}/src/dp -I/mswg/release/nps/EZdk/EZdk-17.0300/dpe/dp/include -I/mswg/release/nps/EZdk/EZdk-17.0300/dpe/frame/include -I/mswg/release/nps/EZdk/EZdk-17.0300/dpe/sft/include -I/mswg/release/nps/EZdk/EZdk-17.0300/dpe/dpi/include\" "

	for src in $@; do
		cmd+="${alvs_root}/src/dp/${src}.c "
	done
	cmd+="|& tee -a $log"
	echo -e "\n$cmd"
	eval $cmd
	if [ "$?" != 0 ]
	then
		echo "ERROR: failed in creating model files!!!" |& tee -a $log
		return 1
	fi
	return 0
}

###############################################################################

function main {

	set -o  pipefail

	set_args $@

	rm -rf $output_dir
	mkdir -p $output_dir

	print_start_msg

	check_arg $@
	if [ "$?" != 0 ]
	then
		return 1
	fi

	if [ "${cp_or_dp}" == "cp" ]
	then
		cp_run_cparser ${@:3}
	else
		dp_run_cparser ${@:3}
	fi

	if [ "$?" != 0 ]
	then
		return 1
	fi

	echo -e "\nCreating definition file of our testing src file: ${src_file} using script scripts/generate_(cp|dp)_def.py...\n" |& tee -a $log
	if [ "${cp_or_dp}" == "cp" ]
	then
		$testing_dir/scripts/generate_cp_def.py $@
	else
		$testing_dir/scripts/generate_dp_def.py $@
	fi
	if [ "$?" != 0 ]
	then
		echo "ERROR: failed in create definition file!!!" |& tee -a $log
		return 1
	fi

	echo -e "\nCreating mock env of our testing src file: ${src_file}...\n" |& tee -a $log
	echo "/swgwork/yohadd/unittestAPI/tools/generate_mock_env/generate_env.py -o $output_dir/mock -d $output_dir/${3}_def.py" |& tee -a $log
	unbuffer python2.7 /swgwork/yohadd/unittestAPI/tools/generate_mock_env/generate_env.py -o $output_dir/mock -d $output_dir/${3}_def.py |& tee -a $log
	if [ "$?" != 0 ]
	then
		echo "ERROR: failed in create mock env!!!" |& tee -a $log
		return 1
	fi

	if [ "${cp_or_dp}" == "dp" ]
	then
		echo -e "\nPatching environment (WORKAROUND)...\n" |& tee -a $log
		echo "$testing_dir/scripts/dp_fix_types.sh" |& tee -a $log
		$testing_dir/scripts/dp_fix_types.sh |& tee -a $log
		if [ "$?" != 0 ]
		then
			echo "ERROR: failed to patch environment!!!" |& tee -a $log
			return 1
		fi
	fi
	return 0
}

###############################################################################

main $@
exit $?


