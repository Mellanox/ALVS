#!/usr/bin/env python


#===============================================================================
# imports
#===============================================================================

# system  
import sys
import random
from __builtin__ import enumerate

# pythons modules 
# local
import sys
import os
import inspect
my_currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
my_parentdir = os.path.dirname(my_currentdir)
my_grandparentdir =  os.path.dirname(my_parentdir)
sys.path.append(my_grandparentdir)
sys.path.append(my_parentdir)
from common_infra import *
from e2e_infra import *
from unit_tester import Unit_Tester

server_count   = 5
client_count   = 0
service_count  = 4

class State_Sync_Test(Unit_Tester):

	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = generic_init(setup_num, service_count, server_count, client_count)
		
		w = 1
		for i,s in enumerate(self.test_resources['server_list']):
			if i<3:
				s.vip = self.test_resources['vip_list'][0]
				s.weight = w
			else:
				s.vip = self.test_resources['vip_list'][1]
				s.weight = w
	
	def change_config(self, config):
		return
	
	def server_class_hash_add_test(self, ezbox, server_list, vip_list):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		port = '80'
		ezbox = self.test_resources['ezbox']
		server_list = self.test_resources['server_list']
		client_list = self.test_resources['client_list']
		vip_list = self.test_resources['vip_list']
		
		#adding the servers to their service
		for server in server_list:
			ezbox.add_server(server.vip, port, server.ip, port)
		#Verify server0 in service0
		server0 = ezbox.get_server(vip_list[0], port, server_list[0].ip, port, 6)
		if (server0 == None):
			print 'Server0 not found in service0'
			return 1
	
		#Verify server1 in service0
		server1 = ezbox.get_server(vip_list[0], port, server_list[1].ip, port, 6)
		if (server1 == None):
			print 'Server1 not found in service0'
			return 1
	
		server2 = ezbox.get_server(vip_list[0], port, server_list[2].ip, port, 6)
		if (server2 == None):
			print 'Server2 not found in service0'
			return 1
		
		server3 = ezbox.get_server(vip_list[1], port, server_list[3].ip, port, 6)
		if (server3 == None):
			print 'Server3 not found in service1'
			return 1
		
		server4 = ezbox.get_server(vip_list[1], port, server_list[4].ip, port, 6)
		if (server4 == None):
			print 'Server4 not found in service1'
			return 1
		
		return 0
	
	
	def server_class_hash_delete_test(self, ezbox, server_list, vip_list):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		port = '80'
		
		ezbox.delete_server(server_list[1].vip, port, server_list[1].ip, port)
		ezbox.delete_server(server_list[3].vip, port, server_list[3].ip, port)
		
		#Verify server0 in service0
		server0 = ezbox.get_server(vip_list[0], port, server_list[0].ip, port, 6)
		if (server0 == None):
			print 'Server0 not found in service0'
			return 1
	
		#Verify server2 in service0
		server2 = ezbox.get_server(vip_list[0], port, server_list[2].ip, port, 6)
		if (server2 == None):
			print 'Server2 not found in service0'
			return 1
	
		#Verify server3 in service1
		server4 = ezbox.get_server(vip_list[1], port, server_list[4].ip, port, 6)
		if (server4 == None):
			print 'Server4 not found in service1'
			return 1
	
		return 0
	
	
	def clear_test(self, ezbox):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		print "Test clear ezbox test"
		ezbox.flush_ipvs()
		if ezbox.get_num_of_services()!= 0:
			print "after FLUSH all should be 0\n"
			return 1
		return 0
		
	
	#===============================================================================
	# main function
	#===============================================================================
	def run_user_test(self):
		port = '80'
		sched_algorithm = 'sh'
		ezbox = self.test_resources['ezbox']
		server_list = self.test_resources['server_list']
		client_list = self.test_resources['client_list']
		vip_list = self.test_resources['vip_list']
		
		ezbox.add_service(vip_list[0], port, sched_alg=sched_algorithm, sched_alg_opt='')
		ezbox.add_service(vip_list[1], port, sched_alg=sched_algorithm, sched_alg_opt='')
	
		failed_tests = 0
		rc = 0
		
		print "Test 1 - Add servers to services"
		rc = self.server_class_hash_add_test(ezbox, server_list, vip_list)
		if rc:
			print 'Test1 failed !!!\n'
			failed_tests += 1
		else:
			print 'Test1 passed !!!\n'
			
		print "Test 2 - Delete servers from services"
		rc = self.server_class_hash_delete_test(ezbox, server_list, vip_list)
		if rc:
			print 'Test2 failed !!!\n'
			failed_tests += 1
		else:
			print 'Test2 passed !!!\n'
		
		rc = self.clear_test(ezbox)
		if rc:
			print 'Clear test failed !!!\n'
			failed_tests += 1
		else:
			print 'Clear test passed !!!\n'
		# after test12 all services were cleared
		
		if failed_tests == 0:
			print 'ALL Tests were passed !!!'
		else:
			print 'Number of failed tests: %d' %failed_tests
			exit(1)
	
current_test = State_Sync_Test()
current_test.main()