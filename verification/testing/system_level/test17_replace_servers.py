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
from alvs_tester_class import ALVS_Tester

# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir) 
from alvs_infra import *

#===============================================================================
# Test Globals
#===============================================================================
#general porpuse
g_request_count  = 1000
g_next_vm_index = 0

# got from user
g_setup_num      = None

# Configured acording to test number
g_server_count       = 15
g_client_count       = 5
g_service_count      = 1
g_servers_to_replace = 5
g_sched_alg          = "sh"
g_sched_alg_opt      = "-b sh-port"


#===============================================================================
# User Area function needed by infrastructure
#===============================================================================
class Test17(ALVS_Tester):
	
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = generic_init(setup_num, g_service_count, g_server_count, g_client_count)
		
		for s in self.test_resources['server_list']:
			s.vip = self.test_resources['vip_list'][0]
		
		
	def client_execution(self, client, vip):
		client.exec_params += " -i %s -r %d" %(vip, g_request_count)
		client.execute()
	
	
	def run_user_test(self):
		# modified global variables
		global g_next_vm_index
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		#===========================================================================
		# init local variab;es 
		#===========================================================================
		ezbox = self.test_resources['ezbox']
		server_list = self.test_resources['server_list']
		client_list = self.test_resources['client_list']
		vip       = self.test_resources['vip_list'][0]
		port      = 80
		
		#===========================================================================
		# Set services/servers & prepare server_list to add in the next step 
		#===========================================================================
		new_server_list = []
		
		ezbox.add_service(vip, port, g_sched_alg, g_sched_alg_opt)
		for idx,s in enumerate(server_list):
			if ( idx >= (g_server_count - g_servers_to_replace) ):
				new_server_list.append(s)
			else:
				ezbox.add_server(vip, port, s.ip, port)
	
		#===========================================================================
		# send requests & replace server while sending requests 
		#===========================================================================
		print "execute requests on client"
		process_list = []
		for client in client_list:
			process_list.append(Process(target=self.client_execution, args=(client,vip,)))
		for p in process_list:
			p.start()
		
		# wait for requests to start 
		time.sleep(0.5) 
		# remove servers from service (& from list) and add new server to service
		print 'Start user test step 0'
		for idx,new_server in enumerate(new_server_list):
			removed_server = server_list[idx]
			ezbox.delete_server(vip, port, removed_server.ip, port)			
			print "Server %s removed" %removed_server.ip 
			ezbox.add_server(vip, port, new_server.ip, port)
			print "Server %s added" %new_server.ip
	
		for p in process_list:
			p.join()
	
		#===========================================================================
		# send requests after modifing servers list
		# - removed servers should not answer
		#===========================================================================
		print 'Start user test step 1'
		process_list = []
		for client in client_list:
			new_log_name = client.logfile_name+'_1'
			client.add_log(new_log_name) 
			process_list.append(Process(target=self.client_execution, args=(client,vip,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
			
	
	def run_user_checker(self, log_dir):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		server_list_step_2 = []
		for idx in range(g_servers_to_replace, g_server_count):
			server_list_step_2.append(self.test_resources['server_list'][idx])
				
		expected_dict = {}
		expected_dict[0] = {'client_response_count':g_request_count,
							'client_count'     : g_client_count, 
							'no_404'           : True,
							'expected_servers' : self.test_resources['server_list']}
		expected_dict[1] = {'client_response_count':g_request_count,
							'client_count'     : g_client_count, 
							'no_404'           : True,
							'check_distribution':(server_list_step_2,self.test_resources['vip_list'],0.035),
							'expected_servers' : server_list_step_2}
		
		return client_checker(log_dir, expected_dict, 2)

current_test = Test17()
current_test.main()


