#!/usr/bin/env python

import sys
import os

alvs_root = os.path.realpath(os.path.dirname(__file__ ) + "/../../../")
print "***alvs root dirname " + alvs_root
output_dir = alvs_root + "/verification/venus/output/cp/" + sys.argv[3]
def_file = output_dir + "/" + sys.argv[3] + "_def.py"
print "output_dir:   "   + output_dir
try:
    f = open(def_file, "w")
except:
    print 'cannot open', def_file
    exit(1)
f.write('#!/usr/bin/env python\n')
f.write('import os\n\n')

f.write('from dut_definition.dut_def import DutDefinition\n\n')

f.write("dut = DutDefinition(\"alvs_cp\")\n\n")

i = 1
for module_name in sys.argv[3:]:
	src_file = "%s/src/cp/%s.c" % (alvs_root , module_name)
	unit_file = output_dir + "/model/UNIT_" + src_file.replace("/", "_").replace(".", "_").replace("-","_") + ".unit" 
	#print "src_file = %s\n" % src_file
	#print "output_dir = %s\n" % output_dir
	#print "unit_file = %s\n" % unit_file
	f.write("dut.add_model_file(\"DUT_%02d\" , \"%s\" , \"%s\")\n\n" %(i, unit_file, src_file))
	i += 1

i = 1
app_flag = ""
if sys.argv[1] == "ALVS":
	app_flag = "-DCONFIG_ALVS=1"

cmd = "gcc -fno-inline -D__inline= -Dalways_inline= -D__always_inline__=__noinline__ -nostartfiles -nodefaultlibs -nostdlib -fdump-rtl-expand -fpack-struct -gdwarf-2 -g3 -ggdb -w -Dasm= '-Dvolatile(...)=' -Dmain=my_main -DNPS_LITTLE_ENDIAN %s -DNDEBUG -O0 -I/usr/include/libnl3 -I%s/src/common -I%s/src/cp -I/mswg/release/nps/EZdk/EZdk-17.0300/dpe/dp/include -I/mswg/release/nps/EZdk/EZdk-17.0300/cpe/env/include -I/mswg/release/nps/EZdk/EZdk-17.0300/cpe/dev/include -I/mswg/release/nps/EZdk/EZdk-17.0300/cpe/cp/include -I/mswg/release/nps/EZdk/EZdk-17.0300/cpe/agt/agt-cp/include -I/mswg/release/nps/EZdk/EZdk-17.0300/cpe/agt/agt/include -Wl,--warn-unresolved-symbols -c "  %(app_flag, alvs_root, alvs_root)
for module_name in sys.argv[3:]:
	cmd += "[[SRC:DUT_%02d]] " %i
	i += 1
#print "add_build_cmd = %s\n" % cmd
f.write("dut.add_build_cmd(\"%s\")\n\n" %cmd)
	

glibc_unit_file = output_dir + "/glibc_model/UNIT_EXTERNAL_LIB.unit"
f.write("dut.add_external_lib(\"GLIBC\", \"%s\")\n\n" %glibc_unit_file)

pthread_unit_file = output_dir + "/pthread_model/UNIT_EXTERNAL_LIB.unit"
f.write("dut.add_external_lib(\"PTHREAD\", \"%s\")\n\n" %pthread_unit_file)

libgnl3_unit_file = output_dir + "/libgnl3_model/UNIT_EXTERNAL_LIB.unit"
f.write("dut.add_external_lib(\"LIBGNL3\", \"%s\")\n\n" %libgnl3_unit_file)

f.write("dut.add_regenerate_header(\"GENERATE_ALL\")\n\n")

f.write("dut.add_exclude_funcs([\n")
f.write("        \"EZapiPrm_WriteMem\",\n")
f.write("])\n\n")

f.write("dut.add_extra_apis([\n")
f.write("])\n\n")

f.closed
exit(0)


