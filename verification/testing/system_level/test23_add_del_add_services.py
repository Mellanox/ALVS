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
request_count = 50
server_count = 10
client_count = 5
service_count = 5



#===============================================================================
# User Area function needed by infrastructure
#===============================================================================
class Test23(ALVS_Tester):
	
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = generic_init(setup_num, service_count, server_count, client_count)
		
		i = 0
		for s in self.test_resources['server_list']:
			s.vip = self.test_resources['vip_list'][i%service_count]
			i +=1
			
	
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
		vip_list = self.test_resources['vip_list']
		
		#add services
		for i in range(service_count):
			ezbox.add_service(vip_list[i], port)
		for server in server_list:
			ezbox.add_server(server.vip, port, server.ip, port)
		
		for index, client in enumerate(client_list):
			process_list.append(Process(target=self.client_execution, args=(client,vip_list[index],False,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
	
		#remove services
		process_list = []
		for i in range(service_count):
			ezbox.delete_service(vip_list[i], port)
	
		for index, client in enumerate(client_list):
			new_log_name = client.logfile_name+'_1'
			client.add_log(new_log_name) 
			process_list.append(Process(target=self.client_execution, args=(client,vip_list[index],True,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
	
		#add services with different servers
		process_list = []
		for i in range(service_count):
			ezbox.add_service(vip_list[i], port)
		#change service foreach server
		for i in range(server_count):
			server_list[i].update_vip(vip_list[(i+1)%service_count])#servers0,5->service 1 ..... servers4,9->service 0
		for server in server_list:
			ezbox.add_server(server.vip, port, server.ip, port)
		
		for index, client in enumerate(client_list):
			new_log_name = client.logfile_name+'_2'
			client.add_log(new_log_name) 
			process_list.append(Process(target=self.client_execution, args=(client,vip_list[index],False,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
	  		
		print 'End user test'
	
	def run_user_checker(self, log_dir):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		expected_dict = {}
		expected_dict[0] = {'client_response_count':request_count,
							'client_count': client_count, 
							'no_404': True,
							'no_connection_closed': True,
						 	'server_count_per_client':server_count/service_count}
		expected_dict[1] = {'client_response_count':request_count,
							'client_count': client_count, 
							'no_404': False,
							'no_connection_closed': True,
						 	'server_count_per_client':1}
		expected_dict[2] = {'client_response_count':request_count,
							'client_count': client_count, 
							'no_404': True,
							'no_connection_closed': True,
						 	'server_count_per_client':server_count/service_count}
		
		return client_checker(log_dir, expected_dict, 3)
	
current_test = Test23()
current_test.main()
