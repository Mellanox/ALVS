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
from __builtin__ import enumerate
from system_tester_class import System_Tester


# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir) 
from e2e_infra import *


#===============================================================================
# Test Globals
#===============================================================================
request_count = 5000 
server_count = 1
client_count = 1
service_count = 1



#===============================================================================
# User Area function needed by infrastructure
#===============================================================================
class Test15(System_Tester):
	
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = generic_init(setup_num, service_count, server_count, client_count)
		
		for i,s in enumerate(self.test_resources['server_list']):
			s.vip = self.test_resources['vip_list'][i%service_count]
			
	
	def client_execution(self, client, vip):
		client.exec_params += " -i %s -r %d -e True" %(vip, request_count)
		client.execute()
	
	def run_user_test(self):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		process_list = []
		port = '80'		
		ezbox = self.test_resources['ezbox']
		server_list = self.test_resources['server_list']
		client_list = self.test_resources['client_list']
		vip_list = self.test_resources['vip_list']
	
		#service scheduling algorithm is SH without port
		for i in range(service_count):
			ezbox.add_service(vip_list[i], port, sched_alg_opt='')
		for server in server_list:
			ezbox.add_server(server.vip, port, server.ip, port)
			
		
		for index, client in enumerate(client_list):
			process_list.append(Process(target=self.client_execution, args=(client,vip_list[index%service_count],)))
		for p in process_list:
			p.start()
			
		time.sleep(1) 
		# Disable server - director will remove server from IPVS
		print 'remove test.html'
		server_list[0].delete_test_html()
		time.sleep(60) 
		print 're-add test.html'
		server_list[0].set_test_html()
		
		for p in process_list:
			p.join()
	
	def run_user_checker(self, log_dir):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
		expected_dict = {'client_response_count':request_count,
						'client_count': client_count,
						'expected_servers': self.test_resources['server_list'], 
						'no_404': False,
						'server_count_per_client':2}
		
		return client_checker(log_dir, expected_dict)
	
current_test = Test15()
current_test.main()
