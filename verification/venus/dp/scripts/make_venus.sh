#!/bin/bash

###############################################################################

function check_arg {
	
	echo "check existence of dp tested src file ${1} ..."	
	if [ -e "${1}" ]
	then
		return 0
	fi
	echo "BAD_ARG: dp tested src file ${1} is not exist!!!"	
	return 1

}

###############################################################################

function main {

	script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
	alvs_root=$(readlink -e ${script_dir}/../../../../)
	src_file=${alvs_root}/src/dp/${1}.c
	output_dir=${alvs_root}/verification/venus/dp/output/$1
	log=${alvs_root}/verification/venus/dp/output/$1/${1}.log
	testing_dir=${alvs_root}/verification/venus/dp/

	rm -rf $output_dir
	mkdir -p $output_dir

	echo -e "\n*************************************************************************************" | tee -a $log
	echo "************************** Start Running make_venus.sh ******************************" | tee -a $log
	echo -e "*************************************************************************************\n" | tee -a $log
	
	echo -e "******************************* Start Checking Arguments ****************************" | tee -a $log
	echo -e "Arg1 valid value: name of tested dp src file without extention, i.e., alvs_packet_processing for src/dp/alvs_packet_processing.c\n" | tee -a $log
	echo "Running this script to create model files & mock environment..." | tee -a $log
	echo "ALVS root: $alvs_root" | tee -a $log
	echo "dp src file: $src_file" | tee -a $log
	check_arg $src_file
	if [ "$?" == 1 ]
	then
		return 1
	fi
	echo -e "Output dir: $output_dir" | tee -a $log
	echo -e "log file: $log\n" | tee -a $log
	echo -e "testing dir: $testing_dir" | tee -a $log
	echo -e "Creating glibc model ...\n" | tee -a $log
	echo "/swgwork/yohadd/unittestAPI/tools/Cparser/cparser.py -o $output_dir/glibc_model --external_lib /usr/include/stdio.h /usr/include/stdint.h /usr/include/stdlib.h /usr/include/string.h" | tee -a $log
	/swgwork/yohadd/unittestAPI/tools/Cparser/cparser.py -o $output_dir/glibc_model --external_lib /usr/include/stdio.h /usr/include/stdint.h /usr/include/stdlib.h /usr/include/string.h | tee -a $log
	if [ "$?" == 1 ]
	then
		echo "ERROR: failed in create glibc model!!!" | tee -a $log
		return 1
	fi

	echo -e "\nCreating model of our testing src file: ${src_file}...\n" | tee -a $log
	echo "/swgwork/yohadd/unittestAPI/tools/Cparser/cparser.py -o $output_dir/model -p \"-w -Dasm= \'-Dvolatile(...)=\' -Dmain=my_main -nostdinc -nostartfiles -nodefaultlibs -nostdlib -DBIG_ENDIAN -DNDEBUG -O0 -I${alvs_root}/EZdk/ldk/toolchain/lib/gcc/arceb-ezchip-linux-uclibc/4.8.4/include-fixed/ -I${alvs_root}/EZdk/ldk/toolchain/lib/gcc/arceb-ezchip-linux-uclibc/4.8.4/include -I${alvs_root}/EZdk/ldk/toolchain/arceb-ezchip-linux-uclibc/sysroot/usr/include/ -I${alvs_root}/src/common -I${alvs_root}/src/dp -I/mswg/release/nps/EZdk/EZdk-2.1a/dpe/dp/include -I/mswg/release/nps/EZdk/EZdk-2.1a/dpe/frame/include -I/mswg/release/nps/EZdk/EZdk-2.1a/dpe/sft/include -I/mswg/release/nps/EZdk/EZdk-2.1a/dpe/dpi/include\" $src_file" | tee -a $log
	/swgwork/yohadd/unittestAPI/tools/Cparser/cparser.py -o $output_dir/model -p "-w -Dasm= '-Dvolatile(...)=' -Dmain=my_main -nostdinc -nostartfiles -nodefaultlibs -nostdlib -DBIG_ENDIAN -DNDEBUG -O0 -I${alvs_root}/EZdk/ldk/toolchain/lib/gcc/arceb-ezchip-linux-uclibc/4.8.4/include-fixed/ -I${alvs_root}/EZdk/ldk/toolchain/lib/gcc/arceb-ezchip-linux-uclibc/4.8.4/include -I${alvs_root}/EZdk/ldk/toolchain/arceb-ezchip-linux-uclibc/sysroot/usr/include/ -I${alvs_root}/src/common -I${alvs_root}/src/dp -I/mswg/release/nps/EZdk/EZdk-2.1a/dpe/dp/include -I/mswg/release/nps/EZdk/EZdk-2.1a/dpe/frame/include -I/mswg/release/nps/EZdk/EZdk-2.1a/dpe/sft/include -I/mswg/release/nps/EZdk/EZdk-2.1a/dpe/dpi/include" $src_file | tee -a $log
	if [ "$?" == 1 ]
	then
		echo "ERROR: failed in create ${src_file} model!!!" | tee -a $log
		return 1
	fi

	echo -e "\nCreating definition file of our testing src file: ${src_file} using our config/template_def.py...\n" | tee -a $log
	if [ -e $output_dir/${1}_def.py ]
	then
		echo -e "Definition file: $output_dir/${1}_def.py already exist! generate_env.py will use it...\n" | tee -a $log
	else
		sed "s/\#MODULE_NAME\#/${1}/g" $testing_dir/config/template_def.py > $output_dir/${1}_def.py
	fi

	
	echo -e "\nCreating mock env of our testing src file: ${src_file}...\n" | tee -a $log
	echo "/swgwork/yohadd/unittestAPI/tools/generate_mock_env/generate_env.py -o $output_dir/mock -d $output_dir/${1}_def.py" | tee -a $log
	/swgwork/yohadd/unittestAPI/tools/generate_mock_env/generate_env.py -o $output_dir/mock -d $output_dir/${1}_def.py | tee -a $log
	if [ "$?" == 1 ]
	then
		echo "ERROR: failed in create mock env!!!" | tee -a $log
		return 1
	fi

	return 0
}

###############################################################################

main $1
exit $?


