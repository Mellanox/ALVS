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
server_count = 12
client_count = 6
service_count = 3


#===============================================================================
# User Area function needed by infrastructure
#===============================================================================
class Test35_36(ALVS_Tester):
	
	def __init__(self, sched_alg):
		self.sched_alg = sched_alg
	
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = generic_init(setup_num, service_count, server_count, client_count)
		service_num_servers = 4
		
		w1 = 1
		w2 = 7
		w3 = 4
		index = 0
		for s in self.test_resources['server_list']:
			if index < service_num_servers:
				print "init server %d weight = %d" %(index,w1)
				s.vip = self.test_resources['vip_list'][0]
				s.weight = w1
				w1 += 1
			elif index < 2 * service_num_servers:
				print "init server %d weight = %d" %(index,w2)
				s.vip = self.test_resources['vip_list'][1]
				s.weight = w2
				w2 += 4
			else:
				print "init server %d weight = %d" %(index,w3)
				s.vip = self.test_resources['vip_list'][2]
				s.weight = w3
				w2 += 2
			index += 1	
	
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
		vip_list = self.test_resources['vip_list']
	
		for i in range(service_count):
			print "service %s is set with %s scheduling algorithm" %(vip_list[i],self.sched_alg)
			ezbox.add_service(vip_list[i], port, sched_alg=self.sched_alg, sched_alg_opt='')
		
		for server in server_list:
			print "adding server %s to service %s" %(server.ip,server.vip)
			ezbox.add_server(server.vip, port, server.ip, port,server.weight)
		
		
		for index, client in enumerate(client_list):
			process_list.append(Process(target=self.client_execution, args=(client,vip_list[index%service_count],)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
		
		print 'End user test'	
	
	def run_user_checker(self, log_dir):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		server_list = self.test_resources['server_list']
		client_list = self.test_resources['client_list']
		vip_list = self.test_resources['vip_list']
	
		sd = 0.01
		expected_dict = {'client_response_count':request_count,
							'client_count': client_count,
							'no_connection_closed': True,
							'no_404': True,
							'check_distribution':(server_list,vip_list,sd,self.sched_alg),
							'expected_servers':server_list}
		
		expected_servers = {}
		for index, client in enumerate(client_list):
			client_expected_servers=[s.ip for s in server_list if s.vip == vip_list[index%service_count]]
			expected_servers[client.ip] = client_expected_servers
	
		expected_dict['expected_servers_per_client'] = expected_servers
		
		return client_checker(log_dir, expected_dict)

