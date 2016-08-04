#!/usr/bin/env python

import os

from dut_definition.dut_def import DutDefinition

dut = DutDefinition("alvs_ap")

alvs_root = os.path.realpath(os.getcwd() + "/../../../")

module_name = "#MODULE_NAME#"

src_file = "%s/src/dp/%s.c" % (alvs_root , module_name)

output_dir = alvs_root + "/verification/venus/dp/output/" + module_name

unit_file = output_dir + "/model/UNIT_" + src_file.replace("/", "_").replace(".", "_") + ".unit" 

print "src_file = %s\n" % src_file
print "output_dir = %s\n" % output_dir
print "unit_file = %s\n" % unit_file

dut.add_model_file("DUT_01" , unit_file , src_file)

cmd = "gcc -fno-inline -D__inline= -Dalways_inline= -D__always_inline__=__noinline__ -nostdinc -nostartfiles -nodefaultlibs -nostdlib -fdump-rtl-expand -fpack-struct -gdwarf-2 -g3 -ggdb -w -Dasm= '-Dvolatile(...)=' -Dmain=my_main -DBIG_ENDIAN -O0 -c -isystem %s/src/common/ -I%s/EZdk/ldk/toolchain/lib/gcc/arceb-ezchip-linux-uclibc/4.8.4/include-fixed/ -I%s/EZdk/ldk/toolchain/lib/gcc/arceb-ezchip-linux-uclibc/4.8.4/include -I%s/EZdk/ldk/toolchain/arceb-ezchip-linux-uclibc/sysroot/usr/include/ -I%s/src/common -I%s/src/dp -I/mswg/release/nps/EZdk/EZdk-2.1a/dpe/dp/include -I/mswg/release/nps/EZdk/EZdk-2.1a/dpe/frame/include -I/mswg/release/nps/EZdk/EZdk-2.1a/dpe/sft/include -I/mswg/release/nps/EZdk/EZdk-2.1a/dpe/dpi/include -Wl,--warn-unresolved-symbols -c [[SRC:DUT_01]]" %(alvs_root, alvs_root, alvs_root, alvs_root, alvs_root, alvs_root)
dut.add_build_cmd(cmd)

glibc_unit_file = output_dir + "/glibc_model/UNIT_EXTERNAL_LIB.unit"
dut.add_external_lib("GLIBC", glibc_unit_file)

dut.add_regenerate_header("GENERATE_ALL")

dut.add_exclude_funcs([
        "ezasm_mov4bcl", #MOCK_METHOD13
        "_ezdp_prm_lookup_hash_entry", #MOCK_METHOD10
        "ezasm_cp_sd2cm", #MOCK_METHOD10
        "ezasm_ldphy_common_nd", #MOCK_METHOD10
        "ezasm_cp_cm2sd_schd", #MOCK_METHOD11
])

# add apis that are not included in the functions callgraph and needed by the test
dut.add_extra_apis([
])

