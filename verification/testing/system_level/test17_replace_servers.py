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
g_next_vm_index = 0

# got from user
g_setup_num      = None

# Configured acording to test number
g_server_count       = None
g_client_count       = None
g_service_count      = None
g_servers_to_replace = None
g_sched_alg          = None
g_sched_alg_opt      = None


#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

#===============================================================================
# Function: set_user_params
#
# Brief:
#===============================================================================
def user_init(setup_num):
	# modified global variables
	global g_next_vm_index
	
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	vip_list = [get_setup_vip(setup_num,i) for i in range(g_service_count)]

	setup_list = get_setup_list(setup_num)

	# get serevers list
	server_list=[]
	for i in range(g_server_count):
		server_list.append(HttpServer(ip = setup_list[g_next_vm_index]['ip'],
						  hostname = setup_list[g_next_vm_index]['hostname'], 
						  username = "root", 
						  password = "3tango", 
						  vip = vip_list[0],
						  eth='ens6'))
		g_next_vm_index+=1
	
 	# get clients list
	client_list=[]
 	script_dirname = os.path.dirname(os.path.realpath(__file__))
	for i in range(g_client_count):
		client_list.append(HttpClient(ip = setup_list[g_next_vm_index]['ip'],
						  hostname = setup_list[g_next_vm_index]['hostname'], 
						  username = "root", 
						  password = "3tango",
 						  exe_path    = script_dirname,
 						  exe_script  = "basic_client_requests.py",
 						  exec_params = ""))
		g_next_vm_index+=1
	

	# get EZbox
	ezbox = ezbox_host(setup_num)

	return (server_list, ezbox, client_list, vip_list)

#===============================================================================
# Function: set_user_params
#
# Brief:
#===============================================================================
def client_execution(client, vip):
	client.exec_params += " -i %s -r %d" %(vip, g_request_count)
	client.execute()

#===============================================================================
# Function: set_user_params
#
# Brief:
#===============================================================================
def run_user_test_step(server_list, ezbox, client_list, vip_list):
	# modified global variables
	global g_next_vm_index

	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
	#===========================================================================
	# init local variab;es 
	#===========================================================================
	vip       = vip_list[0]
	port      = 80

	
	#===========================================================================
	# Set service & Add servers to service 
	#===========================================================================
	ezbox.add_service(vip, port, g_sched_alg, g_sched_alg_opt)
	for s in server_list:
		ezbox.add_server(vip, port, s.ip, port)


	#===========================================================================
	# Create new servers list & init servers
	#===========================================================================
	setup_list = get_setup_list(g_setup_num)
	new_server_list=[]
	for i in range(g_servers_to_replace):
		# create new server object & add to list
		new_server = HttpServer(ip   = setup_list[g_next_vm_index]['ip'],
							hostname = setup_list[g_next_vm_index]['hostname'],
							username = "root",
							password = "3tango",
							vip      = vip,
							eth      ='ens6')
		new_server_list.append(new_server)
		g_next_vm_index+=1
	
		# init new server & add to service 
		new_server.init_server(new_server.ip)

	
	#===========================================================================
	# send requests & replace server while sending requests 
	#===========================================================================
	print "execute requests on client"
	process_list = []
	for client in client_list:
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()

	# remove servers from service (& form list)
	# and add new server to service
	for new_server in new_server_list:
		removed_server = server_list.pop()
		ezbox.delete_server(vip, port, removed_server.ip, port)			
		ezbox.add_server(vip, port, new_server.ip, port)
		
		# print removed & added servers
		print removed_server.ip + " removed" 
		print new_server.ip + " added"
		
	# add new servers to list
	for new_server in new_server_list:
		server_list.append(new_server)

	for p in process_list:
		p.join()

	#===========================================================================
	# send requests after modifing servers list
	# - removed servers should not answer
	#===========================================================================
	process_list = []
	for client in client_list:
		new_log_name = client.logfile_name+'_1'
		client.add_log(new_log_name) 
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
		
	print 'End user test step 1'

#===============================================================================
# Function: set_user_params
#
# Brief:
#===============================================================================
def run_user_checker(server_list, ezbox, client_list, log_dir):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	expected_dict = {}
	expected_dict[0] = {'client_response_count':g_request_count,
					'client_count'     : len(client_list), 
					'no_404'           : True}
	expected_dict[1] = {'client_response_count':g_request_count,
					'client_count'     : len(client_list), 
					'no_404'           : True,
					'expected_servers' : server_list}
	
	return client_checker(log_dir, expected_dict, 2)

#===============================================================================
# Function: set_user_params
#
# Brief:
#===============================================================================
def set_user_params(setup_num):
	# modified global variables
	global g_setup_num
	global g_server_count
	global g_client_count
	global g_service_count
	global g_servers_to_replace
	global g_sched_alg
	global g_sched_alg_opt
	
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	# user configuration
	g_setup_num = setup_num
	
	# static test configuration
	g_server_count       = 10
	g_client_count       = 5
	g_service_count      = 1
	g_servers_to_replace = 5
	g_sched_alg          = "sh"
	g_sched_alg_opt      = "-b sh-port"
	
	# print configuration
	print "setup_num:      " + str(g_setup_num)
	print "service_count:  " + str(g_service_count)
	print "server_count:   " + str(g_server_count)
	print "client_count:   " + str(g_client_count)
	print "request_count:  " + str(g_request_count)
	print "servers_to_add: " + str(g_servers_to_replace)


#===============================================================================
# Function: main function
#
# Brief:
#===============================================================================
def main():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	if len(sys.argv) != 3:
		print "script expects exactly 2 input arguments"
		print "Usage: client_requests.py <setup_num> <True/False (use 4 k CPUs)>"
		exit(1)

	set_user_params( int(sys.argv[1]) ) 	
	use_4_k_cpus = True if sys.argv[2].lower() == 'true' else False

	server_list, ezbox, client_list, vip_list = user_init(g_setup_num)

	init_players(server_list, ezbox, client_list, vip_list, True, use_4_k_cpus)

	run_user_test(server_list, ezbox, client_list, vip_list)

	log_dir = collect_logs(server_list, ezbox, client_list)

	gen_rc = general_checker(server_list, ezbox, client_list)

	clean_players(server_list, ezbox, client_list, True)

	client_rc = run_user_checker(server_list, ezbox, client_list, log_dir, vip_list)

	if client_rc and gen_rc:
		print 'Test passed !!!'
		exit(0)
	else:
		print 'Test failed !!!'
		exit(1)


main()
