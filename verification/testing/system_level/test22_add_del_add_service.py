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


# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir) 
from alvs_infra import *


#===============================================================================
# Test Globals
#===============================================================================
request_count = 10
server_count = 2
client_count = 1
service_count = 1
from alvs_players_factory import *


#===============================================================================
# User Area function needed by infrastructure
#===============================================================================
class Test22(ALVS_Tester):
	
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
		self.test_resources = ALVS_Players_Factory.generic_init(setup_num, service_count, server_count, client_count)
		
		for s in self.test_resources['server_list']:
			s.vip = self.test_resources['vip_list'][0]
			
	
	def client_execution(self, client, vip, expects404):
		client.exec_params += " -i %s -r %d -e %s" %(vip, request_count, expects404)
		client.execute()
	
	def run_user_test(self):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		process_list = []
		port = '80'
		ezbox = self.test_resources['ezbox']
		server_list = self.test_resources['server_list']
		client_list = self.test_resources['client_list']
		vip = self.test_resources['vip_list'][0]
		
		#add service with first server
		ezbox.add_service(vip, port)
		ezbox.add_server(server_list[0].vip, port, server_list[0].ip, port)
	
		for client in client_list:
			process_list.append(Process(target=self.client_execution, args=(client,vip,False,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
	
		process_list = []
		#remove service
		ezbox.delete_service(vip, port)
	
		for client in client_list:
			new_log_name = client.logfile_name+'_1'
			client.add_log(new_log_name) 
			process_list.append(Process(target=self.client_execution, args=(client,vip,True,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
	
		process_list = []
		#add service with second server
		ezbox.add_service(vip, port)
		ezbox.add_server(server_list[1].vip, port, server_list[1].ip, port)
		
	
		for client in client_list:
			new_log_name = client.logfile_name+'_2'
			client.add_log(new_log_name) 
			process_list.append(Process(target=self.client_execution, args=(client,vip, False,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
			
		print 'End user test'
	
	def run_user_checker(self, log_dir):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		expected_server_list=[]
		expected_server_list.append(self.test_resources['server_list'][0])
		expected_dict = {}
		expected_dict[0] = {'client_response_count':request_count,
							'client_count': client_count, 
							'no_404': True,
							'no_connection_closed': True,
							'expected_servers':expected_server_list,
						 	'server_count_per_client':1}
		expected_dict[1] = {'client_response_count':request_count,
							'client_count': client_count, 
							'no_404': False,
							'no_connection_closed': True,
						 	'server_count_per_client':1}
		expected_server_list=[]
		expected_server_list.append(self.test_resources['server_list'][1])
		expected_dict[2] = {'client_response_count':request_count,
							'client_count': client_count, 
							'no_404': True,
							'no_connection_closed': True,
							'expected_servers':expected_server_list,
						 	'server_count_per_client':1}
		
		return client_checker(log_dir, expected_dict, 3)
	
current_test = Test22()
current_test.main()
