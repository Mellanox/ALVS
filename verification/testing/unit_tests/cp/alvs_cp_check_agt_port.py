#!/usr/bin/env python

###############################################################################################################################
########################### this test is checking if the agt is enable on release compiled bin ################################
################################ run this test always on release bin (dont use debug bin) #####################################
###############################################################################################################################

import sys
sys.path.append("verification/testing/")
from ez_general.ezpy_exceptions import EZpyFailedToConnectSocket

ezbox,args = init_test(test_arguments=sys.argv, agt_enable=False, wait_for_dp=False)

print "Check Connection to AGT"
try:
	ezbox.cpe.cp.struct.get_hash_params(STRUCT_ID_NW_ARP)
except EZpyFailedToConnectSocket as e:
	print "Connection to AGT Failed"
	print "Test Passed"
	pass
except:
	print "AGT port is Enabled, Test Failed"
	exit(1)
else:
	print "AGT port is Enabled, Test Failed"
	exit(1)

