#!/usr/bin/env python

import sys
sys.path.append("verification/testing")
from common_infra import *
from pprint import pprint
try:
	setup_num = int(sys.argv[1])
except:
	raise

ezbox = ezbox_host(setup_num, "atc")

for interface in range(4):
	stats = ezbox.get_interface_stats(interface)
	for counter in stats:
		if stats[counter] != 0:
			print "Interface %d, Counter %s: %d"%(interface,counter,stats[counter])