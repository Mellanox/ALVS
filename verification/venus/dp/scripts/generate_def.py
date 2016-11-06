#!/usr/bin/env python

import sys
import os


alvs_root = os.path.realpath(os.path.dirname(__file__ ) + "/../../../../")
print "***alvs root dirname " + alvs_root
output_dir = alvs_root + "/verification/venus/dp/output/" + sys.argv[1]
def_file = output_dir + "/" + sys.argv[1] + "_def.py"
try:
    f = open(def_file, "w")
except:
    print 'cannot open', def_file
    exit(1)
f.write('#!/usr/bin/env python\n')
f.write('import os\n\n')
f.write('from dut_definition.dut_def import DutDefinition\n\n')
f.write("dut = DutDefinition(\"alvs_dp\")\n\n")

i = 1

for module_name in sys.argv[1:]:
	src_file = "%s/src/dp/%s.c" % (alvs_root , module_name)
	unit_file = output_dir + "/model/UNIT_" + src_file.replace("/", "_").replace(".", "_").replace("-","_") + ".unit" 
	#print "src_file = %s\n" % src_file
	#print "output_dir = %s\n" % output_dir
	#print "unit_file = %s\n" % unit_file
	f.write("dut.add_model_file(\"DUT_%02d\" , \"%s\" , \"%s\")\n\n" %(i, unit_file, src_file))
	i += 1

i = 1
cmd = "gcc -fno-inline -D__inline= -Dalways_inline= -D__always_inline__=__noinline__ -nostdinc -nostartfiles -nodefaultlibs -nostdlib -fdump-rtl-expand -fpack-struct -gdwarf-2 -g3 -ggdb -w -Dasm= '-Dvolatile(...)=' -Dmain=my_main -DBIG_ENDIAN -O0 -c -isystem %s/src/common/ -I%s/EZdk/ldk/toolchain/lib/gcc/arceb-ezchip-linux-uclibc/4.8.4/include-fixed/ -I%s/EZdk/ldk/toolchain/lib/gcc/arceb-ezchip-linux-uclibc/4.8.4/include -I%s/EZdk/ldk/toolchain/arceb-ezchip-linux-uclibc/sysroot/usr/include/ -I%s/src/common -I%s/src/dp -I/mswg/release/nps/EZdk/EZdk-17.0300/dpe/dp/include -I/mswg/release/nps/EZdk/EZdk-17.0300/dpe/frame/include -I/mswg/release/nps/EZdk/EZdk-17.0300/dpe/sft/include -I/mswg/release/nps/EZdk/EZdk-17.0300/dpe/dpi/include -Wl,--warn-unresolved-symbols -c " %(alvs_root, alvs_root, alvs_root, alvs_root, alvs_root, alvs_root)
for module_name in sys.argv[1:]:
	cmd += "[[SRC:DUT_%02d]] " %i
	i += 1	
f.write("dut.add_build_cmd(\"%s\")\n\n" %cmd)
	

glibc_unit_file = output_dir + "/glibc_model/UNIT_EXTERNAL_LIB.unit"
f.write("dut.add_external_lib(\"GLIBC\", \"%s\")\n\n" %glibc_unit_file)

f.write("dut.add_regenerate_header(\"GENERATE_ALL\")\n\n")

f.write("dut.add_exclude_funcs([\n")
f.write("        \"ezasm_mov4bcl\", #MOCK_METHOD13\n")
f.write("        \"_ezdp_prm_lookup_hash_entry\", #MOCK_METHOD10\n")
f.write("        \"ezasm_cp_sd2cm\", #MOCK_METHOD10\n")
f.write("        \"ezasm_ldphy_common_nd\", #MOCK_METHOD10\n")
f.write("        \"ezasm_cp_cm2sd_schd\", #MOCK_METHOD11\n")
f.write("])\n\n")

f.write("dut.add_extra_apis([\n")
f.write("])\n\n")

f.closed
exit(0)


