#!/usr/bin/env python

###############################################################################################################################
#########################   This test check that unsupported commands will not affect the system ##############################
###############################################################################################################################

import sys
sys.path.append("../")
from test_infra import *

args = read_test_arg(sys.argv)    

log_file = "alvs_unsupported_features_test.log"
if 'log_file' in args:
    log_file = args['log_file']
init_logging(log_file)

scenarios_to_run = args['scenarios']

ezbox = ezbox_host(management_ip=args['host_ip'], username='root', password='ezchip',  nps_ip=args['nps_ip'], cp_app_bin=args['cp_bin'], dp_app_bin=args['dp_bin'])

if args['hard_reset']:
    ezbox.reset_ezbox(args['ezbox'])

ezbox.connect()
ezbox.terminate_cp_app()
ezbox.reset_chip()
ezbox.copy_cp_bin_to_host()
ezbox.run_cp_app()

# add ipv6 arp entry
cmd = 'ip -6 neigh add lladdr a:a:a:a:a:a dev eth0 fe80::a539:c48e:4c21:2f18'

result,output,pid = ezbox.execute_command_on_host(cmd)
print result
print output
print pid

result = ezbox.compare_arp_tables(update_arp_entries=True)
if result == False:
    print "Error - arp table from linux and control plane is not equal"
    exit(1)
    

