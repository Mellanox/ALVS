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
# general porpuse
g_request_count_def  = 500
g_request_count_s1   = 2*g_request_count_def
g_request_count_s2   = 10
g_request_count      = g_request_count_def

# Configured acording to test number
g_server_count   = 2
g_client_count   = 2
g_service_count  = 1
g_sched_alg      = "sh"
g_sched_alg_opt  = "-b sh-port"
g_base_weight    = 2



#===============================================================================
# User Area function needed by infrastructure
#===============================================================================
class Test29(Tester):

	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = generic_init(setup_num, g_service_count, g_server_count, g_client_count)
	
		for s in self.test_resources['server_list']:
			s.vip = self.test_resources['vip_list'][0]
			s.weight=g_base_weight
	
	
	def client_execution(self, client, vip , expects404):
		client.exec_params += " -i %s -r %d -e %s" %(vip, g_request_count, expects404)
		client.execute()
	
	
	def run_user_test(self):
		# modified global variables
		global g_request_count
		
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
					
		ezbox = self.test_resources['ezbox']
		server_list = self.test_resources['server_list']
		client_list = self.test_resources['client_list']
		vip        = self.test_resources['vip_list'][0]
		port       = 80
		new_weight = 0
	
		
		ezbox.add_service(vip, port, g_sched_alg, g_sched_alg_opt)
		for server in server_list:
			ezbox.add_server(server.vip, port, server.ip, port)
		
			
		
		#===========================================================================
		# send requests on current configuration 
		#===========================================================================
		process_list = []
		for client in client_list:
			process_list.append(Process(target=self.client_execution, args=(client,vip,False,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
		
		
		#===========================================================================
		# Change weight of the first server to zero & send requests 
		#===========================================================================
		print 'change first server weight to zero while sending requests. servers: %s' %(server_list[0].ip)
		
		# change request count. try to see change of weigth 0
		g_request_count = g_request_count_s1
	
		process_list = []
		for client in client_list:
			new_log_name = client.logfile_name+'_1'
			client.add_log(new_log_name) 
			process_list.append(Process(target=self.client_execution, args=(client,vip,False,)))
		for p in process_list:
			p.start()
		
		time.sleep(1)
		server_list[0].weight = new_weight
		ezbox.modify_server(vip, port, server_list[0].ip, port, weight=new_weight)
	
		for p in process_list:
			p.join()
	
		#===========================================================================
		# Change weight of all servers to zero & send requests 
		#===========================================================================
		print 'change all servers weight to zero.'
		
		# change request count because expecting ERROR 404
		g_request_count = g_request_count_s2
		
		for s in server_list:
			s.weight = new_weight
			ezbox.modify_server(vip, port, s.ip, port, weight=s.weight)
			print "server %s weigth change to zero" %(s.ip)
			
	
		process_list = []
		for client in client_list:
			new_log_name = client.logfile_name[:-2]+'_2'
			client.add_log(new_log_name) 
			process_list.append(Process(target=self.client_execution, args=(client,vip,True,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
		
		#===========================================================================
		# change weight back to default 
		#===========================================================================
		print 'change all servers weight to default'
		
		# change request count back to default
		g_request_count = g_request_count_def
	
		for idx,s in enumerate(server_list):
			s.weight = g_base_weight
			ezbox.modify_server(vip, port, s.ip, port, weight=s.weight)
			
	
		process_list = []
		for client in client_list:
			new_log_name = client.logfile_name[:-2]+'_3'
			client.add_log(new_log_name) 
			process_list.append(Process(target=self.client_execution, args=(client,vip,False,)))
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
	
		# TODO: test weight
		expected_dict[0] = {'client_response_count':g_request_count,
							'client_count': g_client_count,
							'no_connection_closed': True,
							'no_404': True,
							'server_count_per_client':g_server_count,
							'check_distribution':(server_list,vip_list,0.02)}
		expected_dict[1] = {'client_response_count':g_request_count_s1,
							'client_count': g_client_count,
							'no_connection_closed': True,
							'no_404': True}
		expected_dict[2] = {'client_response_count':g_request_count_s2,
							'client_count': g_client_count,
							'no_connection_closed': True,
							'no_404': False}
		expected_dict[3] = {'client_response_count':g_request_count,
							'client_count': g_client_count,
							'no_connection_closed': True,
							'no_404': True,
							'server_count_per_client':g_server_count} # TODO: change 1 with variable
		
		if client_checker(log_dir, expected_dict,4):
			return True
		return False

current_test = Test29()
current_test.main()
