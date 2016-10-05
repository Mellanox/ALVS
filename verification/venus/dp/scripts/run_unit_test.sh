#!/bin/bash

###############################################################################

function build_unit {
	
	echo -e "\nStart building our test..." | tee -a $log

	echo -e "\nRemoving old binary..." | tee -a $log
	echo rm -f ${output_dir}/$1
	echo ""
	rm -f ${output_dir}/$1

	# unit test builder
	export GTEST=/swgwork/yohadd/unittestAPI/etc/gmock/origin/gmock-1.7.0/gtest/
	export GMOCK=/swgwork/yohadd/unittestAPI/etc/gmock/origin/gmock-1.7.0/

	export GTEST_LIB=/swgwork/yohadd/unittestAPI/etc/gmock/compile/gtest/lib/.libs/
	export GMOCK_LIB=/swgwork/yohadd/unittestAPI/etc/gmock/compile/lib/.libs/

	export CFLAGS=" -g -Wfatal-errors -w"
	export LDFLAGS="-Wl,--allow-shlib-undefined -Wl,--warn-unresolved-symbols -Wl,--unresolved-symbols=ignore-all "

	echo g++ -DNDEBUG -O0 -ggdb3 -o ${output_dir}/$1 ${output_dir}/mock/venus_syslib.c  ${output_dir}/mock/venus_mock.c ${output_dir}/mock/dut_mock.cpp $2 ${CFLAGS} ${LDFLAGS} -I${GTEST}/include -I${GMOCK}/include -pthread -L${output_dir}/mock/ -L${GMOCK_LIB} -L${GTEST_LIB} -L. -Wl,-rpath=${output_dir}/mock/ -Wl,-rpath=${GMOCK_LIB} -Wl,-rpath=. -Wl,-rpath=${GTEST_LIB} -lgmock -lgtest -ldut -lc | tee -a $log

	g++ -DNDEBUG -O0 -ggdb3 -o ${output_dir}/$1 ${output_dir}/mock/venus_syslib.c  ${output_dir}/mock/venus_mock.c ${output_dir}/mock/dut_mock.cpp $2 ${CFLAGS} ${LDFLAGS} -I${GTEST}/include -I${GMOCK}/include -pthread -L${output_dir}/mock/ -L${GMOCK_LIB} -L${GTEST_LIB} -L. -Wl,-rpath=${output_dir}/mock/ -Wl,-rpath=${GMOCK_LIB} -Wl,-rpath=. -Wl,-rpath=${GTEST_LIB} -lgmock -lgtest -ldut -lc | tee -a $log

	if [ -e "${output_dir}/$1" ]
	then
		return 0
	fi
	return 1
}

###############################################################################

function build_so {

	echo -e "\nRebuild share object to include code latest updates" | tee -a $log
	echo -e "Pay attention, if function's declaration was updated then you must run make_venus.sh again!!!\n" | tee -a $log

	echo gcc -fno-inline -D__inline= -Dalways_inline= -D__always_inline__=__noinline__ -nostdinc -nostartfiles -nodefaultlibs -nostdlib -fdump-rtl-expand -fpack-struct -gdwarf-2 -g3 -ggdb -w -Dasm= '-Dvolatile(...)=' -Dmain=my_main -DBIG_ENDIAN -O0 -c -I${alvs_root}/EZdk/ldk/toolchain/lib/gcc/arceb-ezchip-linux-uclibc/4.8.4/include-fixed/ -I${alvs_root}/EZdk/ldk/toolchain/lib/gcc/arceb-ezchip-linux-uclibc/4.8.4/include -I${alvs_root}/EZdk/ldk/toolchain/arceb-ezchip-linux-uclibc/sysroot/usr/include/ -I${alvs_root}/src/common -I${alvs_root}/src/dp -I/mswg/release/nps/EZdk/EZdk-2.0a-patch-1.0.0/dpe/dp/include -I/mswg/release/nps/EZdk/EZdk-2.0a-patch-1.0.0/dpe/frame/include -I/mswg/release/nps/EZdk/EZdk-2.0a-patch-1.0.0/dpe/sft/include -I/mswg/release/nps/EZdk/EZdk-2.0a-patch-1.0.0/dpe/dpi/include -Wl,--warn-unresolved-symbols -c $output_dir/mock/__wrap_${1}.c | tee -a $log
	
	gcc -fno-inline -D__inline= -Dalways_inline= -D__always_inline__=__noinline__ -nostdinc -nostartfiles -nodefaultlibs -nostdlib -fdump-rtl-expand -fpack-struct -gdwarf-2 -g3 -ggdb -w -Dasm= '-Dvolatile(...)=' -Dmain=my_main -DBIG_ENDIAN -O0 -c -I${alvs_root}/EZdk/ldk/toolchain/lib/gcc/arceb-ezchip-linux-uclibc/4.8.4/include-fixed/ -I${alvs_root}/EZdk/ldk/toolchain/lib/gcc/arceb-ezchip-linux-uclibc/4.8.4/include -I${alvs_root}/EZdk/ldk/toolchain/arceb-ezchip-linux-uclibc/sysroot/usr/include/ -I${alvs_root}/src/common -I${alvs_root}/src/dp -I/mswg/release/nps/EZdk/EZdk-2.0a-patch-1.0.0/dpe/dp/include -I/mswg/release/nps/EZdk/EZdk-2.0a-patch-1.0.0/dpe/frame/include -I/mswg/release/nps/EZdk/EZdk-2.0a-patch-1.0.0/dpe/sft/include -I/mswg/release/nps/EZdk/EZdk-2.0a-patch-1.0.0/dpe/dpi/include -Wl,--warn-unresolved-symbols -c $output_dir/mock/__wrap_${1}.c
	rm -rf $output_dir/mock/libdut.a
	ld --relocatable  -o $output_dir/mock/libdut.a $output_dir/mock/__wrap_${1}.o 
	objcopy -w -L '!vns_*'  $output_dir/mock/libdut.a

}

###############################################################################

function run_unit_test {

	echo -e "\nStart running unit test" | tee -a $log
	
	echo "gdb -x ${output_dir}/mock/dut_mock.gdb ${output_dir}/$1" | tee -a $log
	unbuffer gdb -x ${output_dir}/mock/dut_mock.gdb ${output_dir}/$1 | tee -a $log
	if [ "`tail -n 1 output/alvs_packet_processing/alvs_packet_processing.log`" == "Program exited with code 01." ] 
	then
		return 1
	fi
	return 0

}

###############################################################################

function run_test_case {

	echo -e "\nStart running unit test" | tee -a $log
	
	echo gdb -batch -x ${output_dir}/mock/dut_mock.gdb --args ${output_dir}/$1 --gtest_filter="PrePostTest.$2" | tee -a $log
	unbuffer gdb -batch -x ${output_dir}/mock/dut_mock.gdb --args ${output_dir}/$1 --gtest_filter="PrePostTest.$2" | tee -a $log
	if [ "`tail -n 1 output/alvs_packet_processing/alvs_packet_processing.log`" == "Program exited with code 01." ] 
	then
		return 1
	fi
	return 0

}

###############################################################################

function main {

	script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
	alvs_root=$(readlink -e ${script_dir}/../../../../)
	output_dir=${alvs_root}/verification/venus/dp/output/$1
	log=${alvs_root}/verification/venus/dp/output/$1/${1}.log

	echo -e "\n*************************************************************************************" | tee -a $log
	echo "************************** Start Running run_unit_test.sh ***************************" | tee -a $log
	echo -e "*************************************************************************************\n" | tee -a $log
	
	echo -e "********************************** Arguments ****************************************" | tee -a $log
	echo -e "Arg1 valid value: name of tested dp src file without extention, i.e., alvs_packet_processing for src/dp/alvs_packet_processing.c" | tee -a $log
	echo -e "Arg2 valid value: unit test path" | tee -a $log
	echo -e "Arg3 valid values: testcase to run\n" | tee -a $log

	echo "Running this script to build & run unit test: $2..." | tee -a $log
	echo "ALVS root: $alvs_root" | tee -a $log
	echo -e "Output dir: $output_dir" | tee -a $log
	echo -e "log file: $log\n" | tee -a $log

	#TODO build_so $1
	
	build_unit $1 $2
	if [ "$?" == 1 ]
	then
		echo -e "\nERROR: failed in build test: $2" | tee -a $log
		return 1
	fi

	res=1
	if [ "$3" == "" ]
	then
		run_unit_test $1
		res=$?
	else
		run_test_case $1 $3
		res=$?
	fi
	return $res
}

###############################################################################

main $1 $2 $3
exit $?


