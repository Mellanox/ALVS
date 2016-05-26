#!/usr/bin/env python

###############################################################################################################################
########################### this test is checking if the agt is enable on release compiled bin ################################
################################ run this test always on release bin (dont use debug bin) #####################################
###############################################################################################################################

import sys
sys.path.append("../")
from test_infra import *
from ez_general.ezpy_exceptions import EZpyFailedToConnectSocket

args = read_test_arg(sys.argv)

log_file = "alvs_cp_check_agt_port.log"
if 'log_file' in args:
    log_file = args['log_file']
    
init_logging(log_file)
    
ezbox = ezbox_host(management_ip=args['host_ip'], username='root', password='ezchip',  nps_ip=args['nps_ip'], cp_app_bin=args['cp_bin'], dp_app_bin=args['dp_bin'])

if args['hard_reset']:
    ezbox.reset_ezbox(args['ezbox'])

ezbox.connect()
ezbox.terminate_cp_app()
ezbox.reset_chip()
ezbox.copy_cp_bin_to_host()
ezbox.run_cp_app(agt_enable = False) # run the cp app with agt disable mode

print "Check Connection to AGT"
try:
    ezbox.cpe.cp.struct.get_hash_params(STRUCT_ID_NW_ARP)
except EZpyFailedToConnectSocket as e:
    print "Connection to AGT Failed"
    print "Test Passed"
    exit(0)
    pass
except:
    print "AGT port is Enabled, Test Failed"
    exit(1)
else:
    print "AGT port is Enabled, Test Failed"
    exit(1)    
    
