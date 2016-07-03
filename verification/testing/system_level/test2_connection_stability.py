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
from e2e_infra import *


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

	vip_list = [get_setup_vip(setup_num,i) for i in range(service_count)]

	index = 0
	setup_list = get_setup_list(setup_num)

	server_list=[]
	for i in range(server_count):
		server_list.append(HttpServer(ip = setup_list[index]['ip'],
						  hostname = setup_list[index]['hostname'], 
						  username = "root", 
						  password = "3tango", 
						  vip = vip_list[0],
						  eth='ens6',
						  weight=1))
		index+=1
	
	script_dirname = os.path.dirname(os.path.realpath(__file__))
	client_list=[]
	for i in range(client_count):
		client_list.append(HttpClient(ip = setup_list[index]['ip'],
						  hostname = setup_list[index]['hostname'], 
						  username = "root", 
						  password = "3tango",
						  exe_path= script_dirname,
						  exe_script  = "basic_client_requests.py",
						  exec_params = ""))
		index+=1
	

	# EZbox
	ezbox = ezbox_host(setup_num)
	
	return (server_list, ezbox, client_list, vip_list)

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
	
	time.sleep(5)
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
	
	time.sleep(5)
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
	if len(sys.argv) != 2:
		print "script expects exactly 1 input argument"
		print "Usage: client_requests.py <setup_num>"
		exit(1)
	print datetime.datetime.now()
	test_res = 0
	setup_num  = int(sys.argv[1])
	
	server_list, ezbox, client_list, vip_list = user_init(setup_num)
	
	init_players(server_list, ezbox, client_list, vip_list, use_director=True, use_4k_cpus=False)
	
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
	
	clean_players(server_list, ezbox, client_list, use_director=True)
	
	print datetime.datetime.now()
	if test_res:
		print 'Test failed !!!'
	else:
		print 'Test passed !!!'
	
	exit(test_res)

main()
