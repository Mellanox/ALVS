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
request_count = 1000
server_count  = 4
client_count  = 4
service_count = 1


#===============================================================================
# User Area function needed by infrastructure
#===============================================================================
class Test37(System_Tester):
	
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = generic_init(setup_num, service_count, server_count, client_count)
		
		w = 3
		for s in self.test_resources['server_list']:
			s.vip = self.test_resources['vip_list'][0]
			s.weight = w
			w += 2
	
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
		
		print "service %s is set with SH scheduling algorithm" %(vip)
		ezbox.add_service(vip, port, sched_alg='sh', sched_alg_opt='-b sh-port')
		for server in server_list:
			print "adding server %s to service %s" %(server.ip,server.vip)
			ezbox.add_server(server.vip, port, server.ip, port, server.weight)
		
		
		for client in client_list:
			process_list.append(Process(target=self.client_execution, args=(client,vip,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
		
		process_list = []
		print "modify scheduling algorithm of service %s to RR" %(vip)
		ezbox.modify_service(vip, port, sched_alg='rr', sched_alg_opt='')
		
		
		for client in client_list:
			new_log_name = client.logfile_name+'_1'
			client.add_log(new_log_name) 
			process_list.append(Process(target=self.client_execution, args=(client,vip,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
		
		process_list = []
		print "modify scheduling algorithm of service %s to WRR" %(vip)
		ezbox.modify_service(vip, port, sched_alg='wrr', sched_alg_opt='')
		
		
		for client in client_list:
			new_log_name = client.logfile_name[:-2]+'_2'
			client.add_log(new_log_name) 
			process_list.append(Process(target=self.client_execution, args=(client,vip,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
		
		process_list = []
		print "modify scheduling algorithm of service %s to SH" %(vip)
		ezbox.modify_service(vip, port,  sched_alg='sh', sched_alg_opt='-b sh-port')
		
		
		for client in client_list:
			new_log_name = client.logfile_name[:-2]+'_3'
			client.add_log(new_log_name) 
			process_list.append(Process(target=self.client_execution, args=(client,vip,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
		
		process_list = []
		print "modify scheduling algorithm of service %s to WRR" %(vip)
		ezbox.modify_service(vip, port,  sched_alg='wrr', sched_alg_opt='')
		
		
		for client in client_list:
			new_log_name = client.logfile_name[:-2]+'_4'
			client.add_log(new_log_name) 
			process_list.append(Process(target=self.client_execution, args=(client,vip,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
		
		process_list = []
		print "modify scheduling algorithm of service %s to RR" %(vip)
		ezbox.modify_service(vip, port,  sched_alg='rr', sched_alg_opt='')
		
		
		for client in client_list:
			new_log_name = client.logfile_name[:-2]+'_5'
			client.add_log(new_log_name) 
			process_list.append(Process(target=self.client_execution, args=(client,vip,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
		
		process_list = []
		print "modify scheduling algorithm of service %s to SH" %(vip)
		ezbox.modify_service(vip, port,  sched_alg='sh', sched_alg_opt='-b sh-port')
		
		
		for client in client_list:
			new_log_name = client.logfile_name[:-2]+'_6'
			client.add_log(new_log_name) 
			process_list.append(Process(target=self.client_execution, args=(client,vip,)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
		
		print 'End user test'
	
	def run_user_checker(self, log_dir):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		sh_sd = 0.05
		rr_wrr_sd = 0.02
		server_list = self.test_resources['server_list']
		client_list = self.test_resources['client_list']
		vip_list = self.test_resources['vip_list']
		expected_dict = {}
		expected_dict[0] = {'client_response_count':request_count,
							'client_count': client_count, 
							'no_404': True,
							'no_connection_closed':True,
							'check_distribution':(server_list,vip_list,sh_sd),
							'expected_servers':server_list}
		expected_dict[1] = {'client_response_count':request_count,
							'client_count': client_count, 
							'no_404': True,
							'no_connection_closed':True,
							'check_distribution':(server_list,vip_list,rr_wrr_sd,"rr"),
							'expected_servers':server_list}
		expected_dict[2] = {'client_response_count':request_count,
							'client_count': client_count, 
							'no_404': True,
							'no_connection_closed':True,
							'check_distribution':(server_list,vip_list,rr_wrr_sd),
							'expected_servers':server_list}
		expected_dict[3] = {'client_response_count':request_count,
							'client_count': client_count, 
							'no_404': True,
							'no_connection_closed':True,
							'check_distribution':(server_list,vip_list,sh_sd),
							'expected_servers':server_list}
		expected_dict[4] = {'client_response_count':request_count,
							'client_count': client_count, 
							'no_404': True,
							'no_connection_closed':True,
							'check_distribution':(server_list,vip_list,rr_wrr_sd),
							'expected_servers':server_list}
		expected_dict[5] = {'client_response_count':request_count,
							'client_count': client_count, 
							'no_404': True,
							'no_connection_closed':True,
							'check_distribution':(server_list,vip_list,rr_wrr_sd,"rr"),
							'expected_servers':server_list}
		expected_dict[6] = {'client_response_count':request_count,
							'client_count': client_count, 
							'no_404': True,
							'no_connection_closed':True,
							'check_distribution':(server_list,vip_list,sh_sd),
							'expected_servers':server_list}
		return client_checker(log_dir, expected_dict, 7)
	
current_test = Test37()
current_test.main()