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
request_count = 1
server_count = 1
client_count = 1
service_count = 1



#===============================================================================
# User Area function needed by infrastructure
#===============================================================================
class Test19(System_Tester):

	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = generic_init(setup_num, service_count, server_count, client_count)
		
		for s in self.test_resources['server_list']:
			s.vip = self.test_resources['vip_list'][0]
	
	def client_execution(self, client, vip):
		connTimeout = 30
		client.exec_params += " -i %s -r %d -t %d " %(vip, request_count,connTimeout)
		client.execute()
	
	def run_user_test(self):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		process_list = []
		port = '80'
		ezbox = self.test_resources['ezbox']
		server_list = self.test_resources['server_list']
		client_list = self.test_resources['client_list']
		vip = self.test_resources['vip_list'][0]
		
		ezbox.add_service(vip, port)
		for server in server_list:
			server.set_extra_large_index_html()
			ezbox.add_server(server.vip, port, server.ip, port)
		
		for client in client_list:
			process_list.append(Process(target=self.client_execution, args=(client,vip,)))
		for p in process_list:
			p.start()
		
		print "wait to start..."
		time.sleep(10)
		
		print "terminate client..."
		for p in process_list:
			p.terminate()
		
		print "wait to join..."
		time.sleep(10)
		
		print "join client..."
		for p in process_list:
			p.join()
		
		print 'End user test'
	
	def run_user_checker(self):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		ezbox = self.test_resources['ezbox']
		opened_connection = False
		for i in range(ALVS_SERVICES_MAX_ENTRIES):
			stats_dict = ezbox.get_services_stats(i)
			if stats_dict['SERVICE_STATS_CONN_SCHED'] != 0:
				print 'The are open connections for service %d. Connection count = %d' %(i, stats_dict['SERVICE_STATS_CONN_SCHED'])
				opened_connection = True
		
		print "wait 16 minutes for chip clean open connections..."
		print "0 minutes..."
		for i in range(4):
			time.sleep(240)
			print "%d minutes..." %((i+1)*4)
		
		closed_connection = True
		print "check open connections again..."
		for i in range(ALVS_SERVICES_MAX_ENTRIES):
			stats_dict = ezbox.get_services_stats(i)
			if stats_dict['SERVICE_STATS_CONN_SCHED'] != 0:
				print 'The are open connections for service %d. Connection count = %d' %(i, stats_dict['SERVICE_STATS_CONN_SCHED'])
				closed_connection = False
				
		return (opened_connection and closed_connection)
	
	def main(self):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		try:
			
			self.config = fill_default_config(generic_main())
			
			self.user_init(self.config['setup_num'])
			
			init_players(self.test_resources, self.config)
			
			self.run_user_test()
			
			self.gen_rc = True #Skip general checker
			
			self.user_rc = self.run_user_checker()
			
			self.print_test_result()
		
		except KeyboardInterrupt:
			print "The test has been terminated, Good Bye"
			
		except:
			print "Unexpected error"
			
		finally:
			clean_players(self.test_resources, True, self.config['stop_ezbox'])
			exit(self.get_test_rc())
	
current_test = Test19()
current_test.main()
