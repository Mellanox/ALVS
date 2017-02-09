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
from pprint import pprint

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
request_count = 1000
server_count = 5
client_count = 1
service_count = 1



#===============================================================================
# User Area function needed by infrastructure
#===============================================================================
class Test26(ALVS_Tester):
	
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = ALVS_Players_Factory.generic_init(setup_num, service_count, server_count, client_count)
		
		for s in self.test_resources['server_list']:
			s.vip = self.test_resources['vip_list'][0]
			s.weight = 1
			
	
	def client_execution(self, client, vip):
		client.exec_params += " -i %s -r %d" %(vip, request_count)
		client.execute()
	
	def run_user_test(self):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		process_list = []
		port = '80'
		ezbox = self.test_resources['ezbox']
		server_list = self.test_resources['server_list']
		client_list = self.test_resources['client_list']
		vip = self.test_resources['vip_list'][0]
		
		ezbox.add_service(vip, port)
		for server in server_list:
			ezbox.add_server(server.vip, port, server.ip, port)
		
		for client in client_list:
			process_list.append(Process(target=self.client_execution, args=(client,vip,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
		
		print 'change weight of servers: %s & %s' %(server_list[0].ip , server_list[1].ip)
		
		new_weight = 2
		server_list[0].weight = new_weight
		ezbox.modify_server(vip, port, server_list[0].ip, port, weight=new_weight)
		server_list[1].weight = new_weight
		ezbox.modify_server(vip, port, server_list[1].ip, port, weight=new_weight)
		
		process_list = []
		for client in client_list:
			new_log_name = client.logfile_name+'_1'
			client.add_log(new_log_name) 
			process_list.append(Process(target=self.client_execution, args=(client,vip,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
		
		print 'End user test'
	
	def run_user_checker(self, log_dir):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		expected_dict = {}
		server_list = self.test_resources['server_list']
		old_server_list = self.deep_copy_server_list()
		old_server_list[0].weight = 1
		old_server_list[1].weight = 1
		sd = 0.02
		expected_dict[0] = {'client_response_count':request_count,
							'client_count': client_count,
							'no_connection_closed': True,
							'no_404': True,
							'expected_servers':old_server_list,
							'check_distribution':(old_server_list,self.test_resources['vip_list'],sd)}
		expected_dict[1] = {'client_response_count':request_count,
							'client_count': client_count,
							'no_connection_closed': True,
							'no_404': True,
							'expected_servers':server_list,
							'check_distribution':(server_list,self.test_resources['vip_list'],sd)}
		
		return client_checker(log_dir, expected_dict,2)

current_test = Test26()
current_test.main()	
