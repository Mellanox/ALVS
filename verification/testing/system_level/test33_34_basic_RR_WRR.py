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
request_count = 1000
server_count = 5
client_count = 5
service_count = 1


#===============================================================================
# User Area function needed by infrastructure
#===============================================================================
class Test33_34(ALVS_Tester):
	
	def __init__(self, sched_alg):
		self.sched_alg = sched_alg
	
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = generic_init(setup_num, service_count, server_count, client_count)
		
		w=1
		for s in self.test_resources['server_list']:
			s.vip = self.test_resources['vip_list'][0]
			s.weight = w
			w+=1
	
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
	
		print "service %s is set with %s scheduling algorithm" %(vip,self.sched_alg)
		ezbox.add_service(vip, port, sched_alg=self.sched_alg, sched_alg_opt='')
		
		for server in server_list:
			print "adding server %s to service %s" %(server.ip,server.vip)
			ezbox.add_server(server.vip, port, server.ip, port, server.weight)
		
		for index, client in enumerate(client_list):
			process_list.append(Process(target=self.client_execution, args=(client,vip,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
		#raw_input("Press any key to continue...")
		print 'End user test'	
	
	def run_user_checker(self, log_dir):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		server_list = self.test_resources['server_list']
		vip_list = self.test_resources['vip_list']
		
		sd = 0.01
		expected_dict = {'client_response_count':request_count,
							'client_count': client_count,
							'no_connection_closed': True,
							'no_404': True,
							'check_distribution':(server_list,vip_list,sd,self.sched_alg),
							'server_count_per_client':server_count/service_count,
							'expected_servers':server_list}
		
		return client_checker(log_dir, expected_dict)
	
