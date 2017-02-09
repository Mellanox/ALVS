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

mask_entries = ezbox.get_all_masks_entries()
print "\nMask Table:"
pprint(mask_entries)

classifier_entries = ezbox.get_all_classifier_entries()
print "\nClassifier Table"
pprint(classifier_entries)

rules_list_entries = ezbox.get_all_rules_list_entries()
print "\nRules List Table"
pprint(rules_list_entries)

actions_entries = ezbox.get_all_actions_entries()
print "\nActions Table"
pprint(actions_entries[0])

extra_actions_entries = ezbox.get_all_actions_extra_info_entries()
print "\nActions Extra Info Table"
pprint(extra_actions_entries[0])

filter_actions_entries = ezbox.get_all_filter_actions_entries()
print "\nFilter Actions Table"
pprint(filter_actions_entries[0])

print "\n\nAction Statistics:"
for i in range(256):
	in_packet_counter, in_bytes_counter = ezbox.get_actions_statistics(i)
	if in_packet_counter != 0:
		print "In packet statistics of action %d is %d"%(i,in_packet_counter)
	if in_bytes_counter != 0:
		print "In bytes statistics of action %d is %d"%(i,in_bytes_counter)
