#!/usr/bin/env python

###############################################################################################################################
#########################   This test check that unsupported commands will not affect the system ##############################
###############################################################################################################################

import sys
sys.path.append("verification/testing")
from test_infra import *

ezbox,args = init_test(test_arguments=sys.argv, agt_enable=True, wait_for_dp=False)

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


