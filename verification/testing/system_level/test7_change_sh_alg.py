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
request_count = 500
server_count  = 5
client_count  = 5
service_count = 1



#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

class Test7(ALVS_Tester):
	
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
		self.test_resources = generic_init(setup_num, service_count, server_count, client_count)
		
		i = 0
		for s in self.test_resources['server_list']:
			s.vip = self.test_resources['vip_list'][i%service_count]
			s.weight = 1
			i+=1
		
	
	def client_execution(self, client, vip):
		client.exec_params += " -i %s -r %d" %(vip, request_count)
		client.execute()
	
	def run_user_test(self):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		process_list = []
		port = '80'
	
		#service scheduling algorithm is SH without port
		for i in range(service_count):
			self.test_resources['ezbox'].add_service(self.test_resources['vip_list'][i], port, sched_alg_opt='')
		for server in self.test_resources['server_list']:
			self.test_resources['ezbox'].add_server(server.vip, port, server.ip, port)
		time.sleep(1)
		for index, client in enumerate(self.test_resources['client_list']):
			process_list.append(Process(target=self.client_execution, args=(client,self.test_resources['vip_list'][index%service_count],)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
			
		
		process_list = []
		#service scheduling algorithm is SH with port
		for i in range(service_count):
			print "modify service to sh-port"
			time.sleep(5)
			self.test_resources['ezbox'].modify_service(self.test_resources['vip_list'][i], port, sched_alg_opt='-b sh-port')
			print "finished modify service to sh-port"
			time.sleep(5)
		for index, client in enumerate(self.test_resources['client_list']):
			new_log_name = client.logfile_name+'_1'
			client.add_log(new_log_name) 
			process_list.append(Process(target=self.client_execution, args=(client,self.test_resources['vip_list'][index%service_count],)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
		
		process_list = []
		#service scheduling algorithm is SH without port
		for i in range(service_count):
			self.test_resources['ezbox'].modify_service(self.test_resources['vip_list'][i], port, sched_alg_opt='')
		for index, client in enumerate(self.test_resources['client_list']):
			new_log_name = client.logfile_name[:-2]+'_2'
			client.add_log(new_log_name) 
			process_list.append(Process(target=self.client_execution, args=(client,self.test_resources['vip_list'][index%service_count],)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
			
		print 'End user test'
	
	def run_user_checker(self, log_dir):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		servers = [s.ip for s in self.test_resources['server_list']]
		expected_servers = dict((client.ip, servers) for client in self.test_resources['client_list'])
		expected_dict = {}
		expected_dict[0] = {'client_response_count':request_count,
							'client_count': client_count, 
							'no_404': True,
							'no_connection_closed':True,
						 	'server_count_per_client':1}
		expected_dict[1] = {'client_response_count':request_count,
							'client_count': client_count, 
							'no_connection_closed':True,
							'expected_servers_per_client':expected_servers,
							'check_distribution':(self.test_resources['server_list'],self.test_resources['vip_list'],0.05),
	 						'no_404': True,
	 					 	'server_count_per_client':server_count/service_count}
		expected_dict[2] = {'client_response_count':request_count,
							'client_count': client_count, 
							'no_connection_closed':True,
							'no_404': True,
						 	'server_count_per_client':1}
		
		return client_checker(log_dir, expected_dict, 3)
	
current_test = Test7()
current_test.main()
		
	
