#!/usr/bin/env python


#===============================================================================
# imports
#===============================================================================

# system  
import sys
import os
import inspect
from __builtin__ import enumerate

# pythons modules 
# local
my_currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
my_parentdir = os.path.dirname(my_currentdir)
my_grandparentdir =  os.path.dirname(my_parentdir)
sys.path.append(my_grandparentdir)
sys.path.append(my_parentdir)
from common_infra import *
from alvs_infra import *
from unit_tester import Unit_Tester

server_count   = 0
client_count   = 0
service_count  = 0

class alvs_unsupported_features_test(Unit_Tester):

	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		self.test_resources = generic_init(setup_num, service_count, server_count, client_count)
		return dict
	
	def change_config(self, config):
		return
	
	#===============================================================================
	# main function
	#===============================================================================
	def run_user_test(self):
		try:
			linux_table={}
			linux_arp_entries_dict={}
			ezbox = self.test_resources['ezbox']
			print "cleaning arp tables"
			ezbox.clean_arp_table()
			
			cmd = 'ip -6 neigh replace lladdr a:a:a:a:a:a dev eth0 fe80::a539:c48e:4c21:2f18'
			
			result_= ezbox.execute_command_on_host(cmd)
			print result_[1]
			
			linux_table = ezbox.get_arp_table()	
			
			alvs_arp_entries_dict = ezbox.get_all_arp_entries()
			
			result = ezbox.compare_arp_tables(linux_table,alvs_arp_entries_dict)
			
			if result == False:
				print "Error - arp table from linux and control plane is not equal"
				exit(1)
		finally:
			#clean arp table any way
			print "cleaning arp tables"
			self.test_resources['ezbox'].clean_arp_table()
		
		
		
	
current_test = alvs_unsupported_features_test()
current_test.main()