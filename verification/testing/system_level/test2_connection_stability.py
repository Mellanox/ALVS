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
import datetime


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
request_count = 50
server_count = 11
client_count = 1
service_count = 1

num_of_iterations = 3


#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	dict = ALVS_Players_Factory.generic_init(setup_num, service_count, server_count, client_count)
	
	for s in dict['server_list']:
		s.vip = dict['vip_list'][0]
		s.weight = 1
		
	return convert_generic_init_to_user_format(dict)

def client_execution(client, vip):
	client.exec_params += " -i %s -r %d" %(vip, request_count)
	client.execute()

def run_user_test(server_list, ezbox, client_list, vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	process_list = []
	port = '80'
	vip = vip_list[0]
	
	print "start step 0 ..."
	print "adding service ..."
	ezbox.add_service(vip, port)
	print "adding server: %s" %(server_list[0].ip)
	ezbox.add_server(server_list[0].vip, port, server_list[0].ip, port)
	
	for client in client_list:
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
	
	print "start step 1 ..."
	process_list = []
	
	print "start running step 1"
	for client in client_list:
		new_log_name = client.logfile_name+'_1'
		client.add_log(new_log_name) 
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	
	time.sleep(5)
	for server in server_list[1:]:
		print "adding server: %s" %(server.ip)
		ezbox.add_server(server.vip, port, server.ip, port)
	
	for p in process_list:
		p.join()
	
	print "start step 2 ..."
	process_list = []
	
	print "start running step 2"
	for client in client_list:
		new_log_name = client.logfile_name+'_2'
		client.add_log(new_log_name) 
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	
	time.sleep(5)
	new_weight = 0
	for server in server_list[1:]:
		print "change weight of server: %s to zero" %(server.ip)
		server.weight = new_weight
		ezbox.modify_server(vip, port, server.ip, port, weight=new_weight)
	
	for p in process_list:
		p.join()
	
	print "start step 3 ..."
	process_list = []
	
	print "start running step 3"
	for client in client_list:
		new_log_name = client.logfile_name+'_3'
		client.add_log(new_log_name) 
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	
	for server in server_list[1:]:
		print "remove server: %s" %(server.ip)
		ezbox.delete_server(server.vip, port, server.ip, port)
	
	for p in process_list:
		p.join()
	
	print "remove service - back to starting stage"
	ezbox.flush_ipvs()
	
	print 'End user test'

def run_user_checker(server_list, ezbox, client_list, log_dir):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	expected_dict = {}
	expected_server0 = []
	expected_server0.append(server_list[0])
	expected_dict[0] = {'client_response_count':request_count,
						'client_count': len(client_list),
						'no_connection_closed': True,
						'no_404': True,
						'expected_servers':expected_server0}
	expected_dict[1] = {'client_response_count':request_count,
						'client_count': len(client_list),
						'no_connection_closed': True,
						'no_404': True,
						'expected_servers':server_list}
	expected_dict[2] = {'client_response_count':request_count,
						'client_count': len(client_list),
						'no_connection_closed': True,
						'no_404': True,
						'expected_servers':server_list}
	expected_dict[3] = {'client_response_count':request_count,
						'client_count': len(client_list),
						'no_connection_closed': True,
						'no_404': True,
						'expected_servers':expected_server0}
	
	return client_checker(log_dir, expected_dict,4)

#===============================================================================
# main function
#===============================================================================
def main():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	if len(sys.argv) != 3:
		print "script expects exactly 2 input arguments"
		print "Usage: client_requests.py <setup_num> <True/False (use 4 k CPUs)>"
		exit(1)

	setup_num  = int(sys.argv[1])
	use_4_k_cpus = True if sys.argv[2].lower() == 'true' else False
	test_res = 0
	
	print datetime.datetime.now()
	
	server_list, ezbox, client_list, vip_list = user_init(setup_num)

	init_players(server_list, ezbox, client_list, vip_list, True, use_4_k_cpus)

	for server in server_list:
		server.set_large_index_html()
	
	for i in range(num_of_iterations):
		print "-----------------------------------------------"
		print "- S t a r t   l o o p   #   %i" %(i)
		print "-----------------------------------------------"
		
		run_user_test(server_list, ezbox, client_list, vip_list)
		
		log_dir = collect_logs(server_list, ezbox, client_list)
		print "Log dir: %s" %(log_dir)
		
		gen_rc = general_checker(server_list, ezbox, client_list)
		
		user_rc = run_user_checker(server_list, ezbox, client_list, log_dir)
		
		if (user_rc == 0) or (gen_rc == 0):
			test_res = 1
			print "======================="
			print "= E R R O R !!!!!!!!!  "
			print "= E R R O R !!!!!!!!!  "
			print "= test % i failed      " %(i)
			print "======================="
		else:
			print "test %d pass." %i
		
		# preparing next loop
		for c in client_list:
			c.remove_last_log()
	
	clean_players(server_list, ezbox, client_list, True, config['stop_ezbox'])
	
	print datetime.datetime.now()
	if test_res:
		print 'Test failed !!!'
	else:
		print 'Test passed !!!'
	
	exit(test_res)

main()
