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

# Configured acording to test number
g_server_count   = None   # includes g_servers_to_add
g_client_count   = None
g_service_count  = None
g_servers_to_add = None
g_sched_alg      = None
g_sched_alg_opt  = None


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
	
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	dict = generic_init(setup_num, g_service_count, g_server_count, g_client_count)
	
	for s in dict['server_list']:
		s.vip = dict['vip_list'][0]
		
	return convert_generic_init_to_user_format(dict)
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
	for idx, s in enumerate(server_list):
		if idx >= (g_server_count - g_servers_to_add):
			break
		
 		ezbox.add_server(vip, port, s.ip, port)

	print "wait 6 second for EZbox to update"
	time.sleep(6)

	#===========================================================================
	# send requests & check results 
	#===========================================================================
	print "execute requests on client"
	process_list = []
	for client in client_list:
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
			

	#===========================================================================
	# Add new servers
	#===========================================================================
	for idx, s in enumerate(server_list):
		if idx < (g_server_count - g_servers_to_add):
			continue
		
		# init new server & add to service 
 		ezbox.add_server(vip, port, s.ip, port)
	
	print "wait 6 second for EZbox to update"
	time.sleep(6)

	#===========================================================================
	# send requests & check results
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


def run_user_checker(server_list, ezbox, client_list, log_dir, vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	expected_dict = {}
	expected_dict[0] = {'client_response_count':g_request_count,
					'client_count'            : len(client_list), 
					'server_count_per_client' : g_server_count - g_servers_to_add,
					'no_connection_closed'    : True,
					'no_404'                  : True}
	expected_dict[1] = {'client_response_count':g_request_count,
					'client_count'            : len(client_list),
					'server_count_per_client' : len(server_list),
					'expected_servers'        : server_list,
					'check_distribution'      : (server_list,vip_list,0.02),
					'no_connection_closed'    : True,
					'no_404'                  : True}
	
	return client_checker(log_dir, expected_dict, 2)

#===============================================================================
# Function: set_user_params
#
# Brief:
#===============================================================================
def set_user_params(setup_num, test):
	# modified global variables
	global g_server_count
	global g_client_count
	global g_service_count
	global g_servers_to_add
	global g_sched_alg
	global g_sched_alg_opt
	
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	# general
	# config test parms
	if test == 8:
		g_server_count   = 3  # includes g_servers_to_add
		g_client_count   = 1
		g_service_count  = 1
		g_servers_to_add = 1
		g_sched_alg        = "sh"
		g_sched_alg_opt    = "-b sh-port"
	elif test == 9:
		g_server_count   = 10  # includes g_servers_to_add
		g_client_count   = 1
		g_service_count  = 1
		g_servers_to_add = 5
		g_sched_alg        = "sh"
		g_sched_alg_opt    = "-b sh-port"
	elif test == 10:
		g_server_count   = 15  # includes g_servers_to_add
		g_client_count   = 10
		g_service_count  = 1
		g_servers_to_add = 10
		g_sched_alg        = "sh"
		g_sched_alg_opt    = "-b sh-port"
	else:
		print "ERROR: unsupported test number: %d" %(test)
		exit()
	
	# print configuration
	print "service_count:  " + str(g_service_count)
	print "server_count:   " + str(g_server_count)
	print "client_count:   " + str(g_client_count)
	print "request_count:  " + str(g_request_count)
	print "servers_to_add: " + str(g_servers_to_add)
	


#===============================================================================
# Function: main function
#
# Brief:
#	Add server during requests
# Configuration:
#	SH - with port
#	one client (many ports)
#	2 servers
# Operation:
#	configure setup
#	send requests
#	add server while sending requests
# Expected:
#	request distributed evenly between 2 servers.
#	When getting new server response, requests distribute evenly between 3 servers"
#===============================================================================
def tests8_10_main(test_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	config = generic_main()
	
	set_user_params(config['setup_num'], test_num) 

	server_list, ezbox, client_list, vip_list = user_init(config['setup_num'])
	
	init_players(server_list, ezbox, client_list, vip_list, config)
	
	run_user_test_step(server_list, ezbox, client_list, vip_list)
	
	log_dir = collect_logs(server_list, ezbox, client_list)

	gen_rc = general_checker(server_list, ezbox, client_list)
	
	clean_players(server_list, ezbox, client_list, True)
	
	user_rc = run_user_checker(server_list, ezbox, client_list, log_dir, vip_list)
	
	if user_rc and gen_rc:
		print 'Test passed !!!'
		exit(0)
	else:
		print 'Test failed !!!'
		exit(1)

