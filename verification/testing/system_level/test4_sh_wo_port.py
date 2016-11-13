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
import time
from system_tester_class import System_Tester

# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir) 
from e2e_infra import *


repeat = 100 
server_count = 5
client_count = 5
service_count = 1

#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

class Test4(System_Tester):
	
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = generic_init(setup_num, service_count, server_count, client_count)
	
		for s in self.test_resources['server_list']:
			s.vip = self.test_resources['vip_list'][0]
	
	def client_execution(self, client, vip):
		client.exec_params += " -i %s -r %d" %(vip, repeat)
		client.execute()
	
	def run_user_test(self):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		process_list = []
		vip = self.test_resources['vip_list'][0]
		port = '80'
	
		self.test_resources['ezbox'].add_service(vip, port, sched_alg_opt='')
		for server in self.test_resources['server_list']:
			self.test_resources['ezbox'].add_server(vip, port, server.ip, port)
			
		for client in self.test_resources['client_list']:
			process_list.append(Process(target=self.client_execution, args=(client,vip,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
			
	def run_user_checker(self, log_dir):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		expected_dict= {'client_response_count':repeat,
						'client_count': client_count,
						'expected_servers': self.test_resources['server_list'],
						'server_count_per_client':1,
						'no_connection_closed':True,
						'no_404': True}
		
		return client_checker(log_dir, expected_dict)
	
		pass
	
current_test = Test4()
current_test.main()