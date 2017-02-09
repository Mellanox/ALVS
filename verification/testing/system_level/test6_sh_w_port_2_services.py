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
from alvs_players_factory import *

#===============================================================================
# Test Globals
#===============================================================================
request_count = 500 
server_count = 8
client_count = 10
service_count = 2



#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

class Test6(ALVS_Tester):
	
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = ALVS_Players_Factory.generic_init(setup_num, service_count, server_count, client_count)
	
		i = 0
		for s in self.test_resources['server_list']:
			s.vip = self.test_resources['vip_list'][i%service_count]
			i+=1
	
	
	def client_execution(self, client, vip):
		client.exec_params += " -i %s -r %d" %(vip, request_count)
		client.execute()
	
	def run_user_test(self):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		process_list = []
		port = '80'
	
		for i in range(service_count):
			self.test_resources['ezbox'].add_service(self.test_resources['vip_list'][i], port, sched_alg_opt='-b sh-port')
		for server in self.test_resources['server_list']:
			self.test_resources['ezbox'].add_server(server.vip, port, server.ip, port)
		
		for index, client in enumerate(self.test_resources['client_list']):
			process_list.append(Process(target=self.client_execution, args=(client,self.test_resources['vip_list'][index%service_count],)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
			
	
	def run_user_checker(self, log_dir):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		expected_dict= {'client_response_count':request_count,
						'client_count': client_count, 
						'no_connection_closed':True,
						'check_distribution':(self.test_resources['server_list'],self.test_resources['vip_list'],0.02),
						'no_404': True,
						'server_count_per_client':server_count/service_count}
		expected_servers = {}
		for index, client in enumerate(self.test_resources['client_list']):
			client_expected_servers=[s.ip for s in self.test_resources['server_list'] if s.vip == self.test_resources['vip_list'][index%service_count]]
			expected_servers[client.ip] = client_expected_servers
	
		expected_dict['expected_servers_per_client'] = expected_servers
	
		return client_checker(log_dir, expected_dict)
	
current_test = Test6()
current_test.main()

