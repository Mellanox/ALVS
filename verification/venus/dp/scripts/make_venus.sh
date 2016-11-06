#!/bin/bash

###############################################################################

function check_arg {
	
	for src in $@; do
		echo "check existence of dp tested src file $src ..."	
		if [ ! -e ${alvs_root}/src/dp/${src}.c ]
		then
			echo "BAD_ARG: dp tested src file ${alvs_root}/src/dp/${src}.c is not exist!!!"	
			return 1
		fi
	done
	return 0
}
###############################################################################

function set_args {
	
	script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
	alvs_root=$(readlink -e ${script_dir}/../../../../)
	src_file=${alvs_root}/src/dp/${1}.c
	output_dir=${alvs_root}/verification/venus/dp/output/$1
	log=${alvs_root}/verification/venus/dp/output/$1/${1}.log
	testing_dir=${alvs_root}/verification/venus/dp/
}
###############################################################################

function print_start_msg {

	echo -e "\n*************************************************************************************" | tee -a $log
	echo "************************** Start Running make_venus.sh ******************************" | tee -a $log
	echo -e "*************************************************************************************\n" | tee -a $log
	
	echo -e "******************************* Start Checking Arguments ****************************" | tee -a $log
	echo -e "Arg1 valid value: name of tested dp src file without extention, i.e., alvs_packet_processing for src/dp/alvs_packet_processing.c\n" | tee -a $log
	echo -e "Arg2- valid value: list of dp src files without extention which ARG1 src file functions may depends on, i.e., main for src/dp/main.c\n" | tee -a $log
	echo "Running this script to create model files & mock environment..." | tee -a $log
	echo "ALVS root: $alvs_root" | tee -a $log
	echo "dp src file: $src_file" | tee -a $log
	
	echo -e "Output dir: $output_dir" | tee -a $log
	echo -e "log file: $log\n" | tee -a $log
	echo -e "testing dir: $testing_dir" | tee -a $log	
}
###############################################################################

function run_cparser {

	echo -e "Creating glibc model ...\n" | tee -a $log
	echo "/swgwork/yohadd/unittestAPI/tools/Cparser/cparser.py -o $output_dir/glibc_model --external_lib /usr/include/stdio.h /usr/include/stdint.h /usr/include/stdlib.h /usr/include/string.h" | tee -a $log
	/swgwork/yohadd/unittestAPI/tools/Cparser/cparser.py -o $output_dir/glibc_model --external_lib /usr/include/stdio.h /usr/include/stdint.h /usr/include/stdlib.h /usr/include/string.h | tee -a $log
	if [ "$?" != 0 ]
	then
		echo "ERROR: failed in create glibc model!!!" | tee -a $log
		return 1
	fi

	cmd="/swgwork/yohadd/unittestAPI/tools/Cparser/cparser.py -o $output_dir/model -p \"-w -Dasm= '-Dvolatile(...)=' -Dmain=my_main -nostdinc -nostartfiles -nodefaultlibs -nostdlib -DBIG_ENDIAN -DNDEBUG -O0 -I${alvs_root}/EZdk/ldk/toolchain/lib/gcc/arceb-ezchip-linux-uclibc/4.8.4/include-fixed/ -I${alvs_root}/EZdk/ldk/toolchain/lib/gcc/arceb-ezchip-linux-uclibc/4.8.4/include -I${alvs_root}/EZdk/ldk/toolchain/arceb-ezchip-linux-uclibc/sysroot/usr/include/ -I${alvs_root}/src/common -I${alvs_root}/src/dp -I/mswg/release/nps/EZdk/EZdk-17.0300/dpe/dp/include -I/mswg/release/nps/EZdk/EZdk-17.0300/dpe/frame/include -I/mswg/release/nps/EZdk/EZdk-17.0300/dpe/sft/include -I/mswg/release/nps/EZdk/EZdk-17.0300/dpe/dpi/include\" "

	for src in $@; do
		cmd+="${alvs_root}/src/dp/${src}.c "
	done
	cmd+="| tee -a $log"
	echo -e "\n$cmd"
	eval $cmd
	if [ "$?" != 0 ]
	then
		echo "ERROR: failed in creating model files!!!" | tee -a $log
		return 1
	fi
	return 0
}

###############################################################################

function main {

        set -o pipefail

	set_args $@

	rm -rf $output_dir
	mkdir -p $output_dir

	print_start_msg
	check_arg $@
	if [ "$?" != 0 ]
	then
		return 1
	fi
	run_cparser $@
	if [ "$?" != 0 ]
	then
		return 1
	fi

	echo -e "\nCreating definition file of our testing src file: ${src_file} using script scripts/generate_def.py...\n" | tee -a $log
	${alvs_root}/verification/venus/dp/scripts/generate_def.py $@
	
	echo -e "\nCreating mock env of our testing src file: ${src_file}...\n" | tee -a $log
	echo "/swgwork/yohadd/unittestAPI/tools/generate_mock_env/generate_env.py -o $output_dir/mock -d $output_dir/${1}_def.py" | tee -a $log
	unbuffer /swgwork/yohadd/unittestAPI/tools/generate_mock_env/generate_env.py -o $output_dir/mock -d $output_dir/${1}_def.py | tee -a $log
	if [ "$?" != 0 ]
	then
		echo "ERROR: failed in create mock env!!!" | tee -a $log
		return 1
	fi
        
	echo -e "\nPatching environment (WORKAROUND)...\n" | tee -a $log
	echo "${alvs_root}/verification/venus/dp/scripts/fix_types.sh" | tee -a $log
	${alvs_root}/verification/venus/dp/scripts/fix_types.sh ${alvs_root}| tee -a $log
	if [ "$?" != 0 ]
	then
		echo "ERROR: failed to patch environment!!!" | tee -a $log
		return 1
	fi

	return 0
}

###############################################################################

main $@
exit $?


