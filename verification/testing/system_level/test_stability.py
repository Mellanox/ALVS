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
# general porpuse
g_request_count  = 500
g_next_vm_index  = 0
g_loops          = 10

# got from user
g_setup_num      = None

# Configured acording to test number
g_server_count       = 10
g_client_count       = 5
g_service_count      = 1
g_servers_to_replace = 5
g_sched_alg          = "sh"
g_sched_alg_opt      = "-b sh-port"


#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

#===============================================================================
# Function: user_init
#
# Brief:
#===============================================================================
def user_init(setup_num):
	# modified global variables
	global g_next_vm_index
	
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	vip_list   = [get_setup_vip(setup_num,i) for i in range(g_service_count)]
	setup_list = get_setup_list(setup_num)

	# get serevers list
	server_list=[]
	for i in range(g_server_count + g_servers_to_replace):
		server_list.append(HttpServer(ip = setup_list[g_next_vm_index]['ip'],
						  hostname = setup_list[g_next_vm_index]['hostname'], 
						  username = "root", 
						  password = "3tango", 
						  vip = vip_list[0],
						  eth='ens6'))
		g_next_vm_index+=1
	
 	# get clients list
	client_list=[]
	for i in range(g_client_count):
		client_list.append(HttpClient(ip = setup_list[g_next_vm_index]['ip'],
						  hostname = setup_list[g_next_vm_index]['hostname']))
		g_next_vm_index+=1
	

	# get EZbox
	ezbox = ezbox_host(setup_num)

	return (server_list, ezbox, client_list, vip_list)

#===============================================================================
# Function: client_execution
#
# Brief:
#===============================================================================
def client_execution(client, vip):
	client.exec_params += " -i %s -r %d" %(vip, g_request_count)
	client.execute()

#===============================================================================
# Function: run_user_test_step
#
# Brief:
#===============================================================================
def run_user_test_step(server_list, ezbox, client_list, vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
	#===========================================================================
	# init local variab;es 
	#===========================================================================
	vip       = vip_list[0]
	port      = 80

	
	#===========================================================================
	# Set service
	# Add servers to service
	# create 2 server list: current (step 1) new (added in step 2)  
	#===========================================================================
	new_server_list=[]
	current_server_list=[]
	
	ezbox.add_service(vip, port, g_sched_alg, g_sched_alg_opt)
	for idx,s in enumerate(server_list):
		if idx < g_server_count:
			current_server_list.append(s)
			ezbox.add_server(vip, port, s.ip, port)
			print s.ip + " added"
		else:
			new_server_list.append(s)


	print "wait 6 second for EZbox to update"
	time.sleep(6)

	
	#===========================================================================
	# send requests & replace server while sending requests 
	#===========================================================================
	print "execute requests on client"
	process_list = []
	for client in client_list:
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()

	# remove servers from service (& form list)
	# and add new server to service
	for new_server in new_server_list:
		removed_server = current_server_list.pop()
		ezbox.delete_server(vip, port, removed_server.ip, port)
		#ezbox.modify_server(vip, port, removed_server.ip, port, weight=0)			
		ezbox.add_server(vip, port, new_server.ip, port)
		
		# print removed & added servers
		#print removed_server.ip + " change weigth to zero" 
		print removed_server.ip + " removed"
		print new_server.ip + " added"
	
	# wait till ezbox updated
	print "wait 5 second for EZbox to update"
	time.sleep(5)
	
	# add new servers to list
	for new_server in new_server_list:
		current_server_list.append(new_server)


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
		
		
	#===========================================================================
	# Remove service - back to starting stage 
	#===========================================================================
	ezbox.flush_ipvs()

		
	print "FUNCTION " + sys._getframe().f_code.co_name + " Ended"
	return current_server_list
	
	
#===============================================================================
# Function: run_user_checker
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
	
	if client_checker(log_dir, expected_dict, 2):
		# Test passed
		return 0
	else:
		# Test failed
		return 1



#===============================================================================
# Function: set_user_params
#
# Brief:
#===============================================================================
def set_user_params(setup_num):
	# modified global variables
	global g_setup_num
	
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	# user configuration
	g_setup_num = setup_num
	
	# print configuration
	print "setup_num:      " + str(g_setup_num)
	print "service_count:  " + str(g_service_count)
	print "server_count:   " + str(g_server_count)
	print "client_count:   " + str(g_client_count)
	print "request_count:  " + str(g_request_count)
	print "servers_to_add: " + str(g_servers_to_replace)
	print "loops:          " + str(g_loops)


#===============================================================================
# Function: main function
#
# Brief:
#===============================================================================
def main():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	global_test_rc = 0

	if len(sys.argv) != 2:
		print "script expects exactly 1 input argument"
		print "Usage: client_requests.py <setup_num>"
		exit(1)

	set_user_params( int(sys.argv[1]) ) 	
	
	server_list, ezbox, client_list, vip_list = user_init(g_setup_num)

	init_players(server_list, ezbox, client_list, vip_list, use_director=True, use_4k_cpus=False)
	
	for i in range(g_loops):
		print "-----------------------------------------------"
		print "- S t a r t   l o o p   #   %i" %(i)
		print "-----------------------------------------------"
		current_server_list = run_user_test_step(server_list, ezbox, client_list, vip_list)
		
		log_dir = collect_logs(server_list, ezbox, client_list, g_setup_num)
		print "Log dir: %s" %(log_dir)
		
		rc = run_user_checker(current_server_list, ezbox, client_list, log_dir)
		if rc:
			global_test_rc = rc
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
	
	exit(global_test_rc)


main()
