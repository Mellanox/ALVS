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
#general porpuse
g_request_count  = 500

# Configured acording to test number
g_server_count   = None   # includes g_servers_to_add
g_client_count   = None
g_service_count  = None
g_servers_to_add = None
g_sched_alg      = None
g_sched_alg_opt  = None


#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

#===============================================================================
# Function: set_user_params
#
# Brief:
#===============================================================================
class Test8_10(Tester):
	
	def __init__(self, test_no):
		self.set_user_params(test_no)
	
	def user_init(self, setup_num):		
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = generic_init(setup_num, g_service_count, g_server_count, g_client_count)
		
		for s in self.test_resources['server_list']:
			s.vip = self.test_resources['vip_list'][0]
			

	def client_execution(self, client, vip):
		client.exec_params += " -i %s -r %d" %(vip, g_request_count)
		client.execute()
	

	def run_user_test(self):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
			
		ezbox = self.test_resources['ezbox']
		server_list = self.test_resources['server_list']
		client_list = self.test_resources['client_list']
		vip = self.test_resources['vip_list'][0]
		port = 80	

		ezbox.add_service(vip, port, g_sched_alg, g_sched_alg_opt)
		for idx, s in enumerate(server_list):
			if idx >= (g_server_count - g_servers_to_add):
				break
			
			ezbox.add_server(vip, port, s.ip, port)
	
	
		#===========================================================================
		# send requests & check results 
		#===========================================================================
		print "execute requests on client"
		process_list = []
		for client in client_list:
			process_list.append(Process(target=self.client_execution, args=(client,vip,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
				
	
		#===========================================================================
		# Add new servers
		#===========================================================================
		for idx, s in enumerate(server_list):
			if idx < (g_server_count - g_servers_to_add):
				continue
			
			# init new server & add to service 
			ezbox.add_server(vip, port, s.ip, port)
		
	
		#===========================================================================
		# send requests & check results
		#===========================================================================
		process_list = []
		for client in client_list:
			new_log_name = client.logfile_name+'_1'
			client.add_log(new_log_name) 
			process_list.append(Process(target=self.client_execution, args=(client,vip,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
	
		print 'End user test step 1'
	
	
	def run_user_checker(self, log_dir):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		server_list = self.test_resources['server_list']
		vip_list = self.test_resources['vip_list']
		expected_dict = {}
		expected_dict[0] = {'client_response_count':g_request_count,
						'client_count'            : g_client_count, 
						'server_count_per_client' : g_server_count - g_servers_to_add,
						'no_connection_closed'    : True,
						'no_404'                  : True}
		expected_dict[1] = {'client_response_count':g_request_count,
						'client_count'            : g_client_count,
						'server_count_per_client' : g_server_count,
						'expected_servers'        : server_list,
						'check_distribution'      : (server_list,vip_list,0.02),
						'no_connection_closed'    : True,
						'no_404'                  : True}
		
		return client_checker(log_dir, expected_dict, 2)
	
	#===============================================================================
	# Function: set_user_params
	#
	# Brief:
	#===============================================================================
	def set_user_params(self, test):
		# modified global variables
		global g_server_count
		global g_client_count
		global g_service_count
		global g_servers_to_add
		global g_sched_alg
		global g_sched_alg_opt
		
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
		# general
		# config test parms
		if test == 8:
			g_server_count   = 3  # includes g_servers_to_add
			g_client_count   = 1
			g_service_count  = 1
			g_servers_to_add = 1
			g_sched_alg        = "sh"
			g_sched_alg_opt    = "-b sh-port"
		elif test == 9:
			g_server_count   = 10  # includes g_servers_to_add
			g_client_count   = 1
			g_service_count  = 1
			g_servers_to_add = 5
			g_sched_alg        = "sh"
			g_sched_alg_opt    = "-b sh-port"
		elif test == 10:
			g_server_count   = 15  # includes g_servers_to_add
			g_client_count   = 10
			g_service_count  = 1
			g_servers_to_add = 10
			g_sched_alg        = "sh"
			g_sched_alg_opt    = "-b sh-port"
		else:
			print "ERROR: unsupported test number: %d" %(test)
			exit()
		
		# print configuration
		print "service_count:  " + str(g_service_count)
		print "server_count:   " + str(g_server_count)
		print "client_count:   " + str(g_client_count)
		print "request_count:  " + str(g_request_count)
		print "servers_to_add: " + str(g_servers_to_add)
		

