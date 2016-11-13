#!/usr/bin/env python


#===============================================================================
# imports
#===============================================================================

# system  
import cmd
from collections import namedtuple
import logging
import os
import sys
import inspect
from multiprocessing import Process
from system_tester_class import System_Tester


# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir) 
from e2e_infra import *


#===============================================================================
# Test Globals
#===============================================================================
# general porpuse
g_request_count  = 500
g_next_vm_index  = 0

# Configured acording to test number
g_server_count   = 5
g_client_count   = 2
g_service_count  = 1
g_sched_alg      = "sh"
g_sched_alg_opt  = "-b sh-port"



#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

#===============================================================================
# Function: user_init
#
# Brief:
#===============================================================================
class Test21(System_Tester):
	
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = generic_init(setup_num, g_service_count, g_server_count, g_client_count)
		
		for s in self.test_resources['server_list']:
			s.vip = self.test_resources['vip_list'][0]
	
	def client_execution(self, client, vip):
		client.exec_params += " -i %s -r %d" %(vip, g_request_count)
		client.execute()

	def run_user_test(self):
	
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"

		ezbox = self.test_resources['ezbox']
		server_list = self.test_resources['server_list']
		client_list = self.test_resources['client_list']
		vip        = self.test_resources['vip_list'][0]
		port       = 80
			
		#===========================================================================
		# add service with servers
		#===========================================================================
		ezbox.add_service(vip, port, g_sched_alg, g_sched_alg_opt)
		for server in server_list:
			ezbox.add_server(server.vip, port, server.ip, port)
		
		server_list[0].take_down_loopback()
		
		process_list = []
		for client in client_list:
			process_list.append(Process(target=self.client_execution, args=(client,vip,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
		
		
		#===========================================================================
		# enable first server  
		#===========================================================================
		process_list = []
		for client in client_list:
			new_log_name = client.logfile_name+'_1'
			client.add_log(new_log_name) 
			process_list.append(Process(target=self.client_execution, args=(client,vip,)))
		for p in process_list:
			p.start()
		
		server_list[0].configure_loopback()
	
	
		for p in process_list:
			p.join()
		
		
		print 'End user test'
	
	
	#===============================================================================
	# Function: run_user_checker
	#
	# Brief:
	#===============================================================================
	def run_user_checker(self, log_dir):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		expected_dict = {}
		
	
		expected_dict[0] = {'client_response_count':g_request_count,
							'g_client_count': g_client_count,
							'no_connection_closed': True,
							'no_404': True,
							'server_count_per_client':(g_server_count - 1)}
		expected_dict[1] = {'client_response_count':g_request_count,
							'g_client_count': g_client_count,
							'no_connection_closed': True,
							'no_404': True,
							'server_count_per_client':g_server_count}
		
		if client_checker(log_dir, expected_dict,2):
			return True
		else:
			return False
	
current_test = Test21()
current_test.main()
