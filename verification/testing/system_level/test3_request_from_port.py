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
# User Area function needed by infrastructure
#===============================================================================
class Test3(Tester):
	
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
		server_count = 5
		client_count = 1
		service_count = 1
		global  g_setup_num 
		g_setup_num = setup_num
		self.test_resources = generic_init(setup_num, service_count, server_count, client_count)
		
		for s in self.test_resources['server_list']:
			s.vip = self.test_resources['vip_list'][0]
	
		for c in self.test_resources['client_list']:
			c.exe_script = "test3_client_requests.py"
	
	
	def client_execution(self, client, vip, setup_num):
		repeat = 10 
		client.exec_params += " -i %s -c %s -s %d -r %d" %(vip,client.ip,setup_num,repeat)
		client.execute(exe_prog="python2.7")
	
	def run_user_test(self):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		process_list = []
		ezbox = self.test_resources['ezbox']
		server_list = self.test_resources['server_list']
		client_list = self.test_resources['client_list']
		vip = self.test_resources['vip_list'][0]
		port = '80'
	
		ezbox.add_service(vip, port)
		for server in server_list:
			ezbox.add_server(vip, port, server.ip, port)
	
		for client in client_list:
			process_list.append(Process(target=self.client_execution, args=(client,vip,g_setup_num)))
		for p in process_list:
			p.start()
		for p in process_list:
			p.join()
		
		print 'End user test'
		if process_list[0].exitcode != 0:
			raise Exception

	
	def run_user_checker(self, log_dir):
		return True

current_test = Test3()
current_test.main()	
