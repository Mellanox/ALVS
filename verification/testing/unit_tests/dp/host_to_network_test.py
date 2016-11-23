#!/usr/bin/env python

import sys
import os
import inspect
my_currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
my_parentdir = os.path.dirname(my_currentdir)
my_grandparentdir =  os.path.dirname(my_parentdir)
sys.path.append(my_grandparentdir)
sys.path.append(my_parentdir)
import random
from common_infra import *
from e2e_infra import *
from unit_tester import Unit_Tester

server_count   = 0
client_count   = 1
service_count  = 0

class Host_To_Network_Test(Unit_Tester):
	
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
			
		self.test_resources = generic_init(setup_num, service_count, server_count, client_count)
		
		w = 1
		for s in self.test_resources['server_list']:
			s.vip = self.test_resources['vip_list'][0]
			s.weight = w
	
	def change_config(self, config):
		return
	
	def run_user_test(self):
		port = '80'
		sched_algorithm = 'source_hash'
		ezbox = self.test_resources['ezbox']
		server_list = self.test_resources['server_list']
		client_list = self.test_resources['client_list']
		vip_list = self.test_resources['vip_list']
		
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
		
current_test = Host_To_Network_Test()
current_test.main()
