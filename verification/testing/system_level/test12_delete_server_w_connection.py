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
from tester_class import Tester



# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir) 
from e2e_infra import *


#===============================================================================
# Test Globals
#===============================================================================
request_count = 1
server_count = 1
client_count = 1
service_count = 1



#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

class Test12(Tester):
	
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = generic_init(setup_num, service_count, server_count, client_count)
		
		for s in self.test_resources['server_list']:
			s.vip = self.test_resources['vip_list'][0]
	
	def client_execution(self, client, vip):
		connTimeout = 30
		client.exec_params += " -i %s -r %d -t %d" %(vip, request_count,connTimeout)
		client.execute()
	
	def run_user_test(self):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		process_list = []
		port = '80'
		vip = self.test_resources['vip_list'][0]
		
		self.test_resources['ezbox'].add_service(vip, port)
		for server in self.test_resources['server_list']:
			server.set_extra_large_index_html()
			self.test_resources['ezbox'].add_server(server.vip, port, server.ip, port)
		
		for client in self.test_resources['client_list']:
			process_list.append(Process(target=self.client_execution, args=(client,vip,)))
		for p in process_list:
			p.start()
		
		time.sleep(7)
		
		print 'remove server[0]'
		self.test_resources['ezbox'].delete_server(self.test_resources['server_list'][0].vip, port, self.test_resources['server_list'][0].ip, port)
		
		for p in process_list:
			p.join()
		
		print 'End user test'
	
	def run_user_checker(self, log_dir):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		expected_dict = {}
		expected_dict = {'client_response_count':request_count,
							'client_count': client_count,
							'no_connection_closed': False,
							'no_404': True}
		
		return client_checker(log_dir, expected_dict, 1)
	
current_test = Test12()
current_test.main()
