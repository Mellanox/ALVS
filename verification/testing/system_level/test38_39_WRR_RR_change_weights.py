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
import copy
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
request_count = 1000
server_count = 5
client_count = 4
service_count = 1


#===============================================================================
# User Area function needed by infrastructure
#===============================================================================
class Test38_39(ALVS_Tester):
	
	def __init__(self, sched_alg):
		self.sched_alg = sched_alg
		
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = ALVS_Players_Factory.generic_init(setup_num, service_count, server_count, client_count)
		
		w = 2
		for s in self.test_resources['server_list']:
			s.vip = self.test_resources['vip_list'][0]
			s.weight = w
			w += 5
	
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
			ezbox.add_server(server.vip, port, server.ip, port,server.weight)
		
		
		for client in client_list:
			process_list.append(Process(target=self.client_execution, args=(client,vip,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
		
		new_weight = 20
		print 'changing weight of server %s from %d to %d' %(server_list[0].ip, server_list[0].weight, new_weight)
		server_list[0].weight = new_weight
		ezbox.modify_server(vip, port, server_list[0].ip, port, weight=new_weight)
		new_weight = 32
		print 'changing weight of server %s from %d to %d' %(server_list[1].ip, server_list[1].weight, new_weight)
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
		vip_list = self.test_resources['vip_list']
		old_server_list = self.deep_copy_server_list()
		old_server_list[0].weight = 2
		old_server_list[1].weight = 7
		sd = 0.02
		
		expected_dict[0] = {'client_response_count':request_count,
							'client_count': client_count,
							'no_connection_closed': True,
							'no_404': True,
							'expected_servers':old_server_list,
							'check_distribution':(old_server_list,vip_list,sd,self.sched_alg)}
		expected_dict[1] = {'client_response_count':request_count,
							'client_count': client_count,
							'no_connection_closed': True,
							'no_404': True,
							'expected_servers':server_list,
							'check_distribution':(server_list,vip_list,sd,self.sched_alg)}
		
		return client_checker(log_dir, expected_dict,2)
	
