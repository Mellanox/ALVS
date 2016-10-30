#!/usr/bin/env python

import sys
sys.path.append("verification/testing")
import random
from common_infra import *
from e2e_infra import *

server_count   = 0
client_count   = 1
service_count  = 0

def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
	dict = generic_init(setup_num, service_count, server_count, client_count)
	
	w = 1
	for s in dict['server_list']:
		s.vip = dict['vip_list'][0]
		s.weight = w
	
	return dict

def main():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	config = fill_default_config(generic_main())
	
	dict = user_init(config['setup_num'])
	
	init_players(dict, config)
	
	server_list, ezbox, client_list, vip_list = convert_generic_init_to_user_format(dict)
	
	run_user_test(server_list, ezbox, client_list, vip_list)
	
	clean_players(dict, True, config['stop_ezbox'])
	
	print "\nTest Passed\n"
	

def run_user_test(server_list, ezbox, client_list, vip_list):
	port = '80'
	sched_algorithm = 'source_hash'
	ezbox.flush_ipvs()    
	
	print "Flush all arp entries"
	ezbox.clean_arp_table()

	print "Checking ping from host to one of the VMs"
	result,output = ezbox.execute_command_on_host('ping ' + client_list[0].ip + ' & sleep 10s && pkill -HUP -f ping')
	print output
	
	time.sleep(11)	

	print "Reading all the arp entries"
	arp_entries = ezbox.get_all_arp_entries() 

	print "The Arp Table:"
	print arp_entries

	print "\nChecking if the VM arp entry exist on arp table"
	if client_list[0].ip not in arp_entries:
	    print "ERROR, arp entry is not exist, ping from host to VM was failed\n"
	    exit(1)
	    
	print "Arp entry exist\n"
	
main()
