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

# Configured acording to test number
g_server_count   = 5
g_client_count   = 2
g_service_count  = 1
g_sched_alg      = "sh"
g_sched_alg_opt  = "-b sh-port"



#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

#===============================================================================
# Function: user_init
#
# Brief:
#===============================================================================
def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	dict = generic_init(setup_num, service_count, server_count, client_count)
	
	for s in dict['server_list']:
		s.vip = dict['vip_list'][0]
		
	return convert_generic_init_to_user_format(dict)

#===============================================================================
# Function: client_execution
#
# Brief:
#===============================================================================
def client_execution(client, vip):
	client.exec_params += " -i %s -r %d" %(vip, g_request_count)
	client.execute()


#===============================================================================
# Function: run_user_test
#
# Brief:
#===============================================================================
def run_user_test(server_list, ezbox, client_list, vip_list):
	# modified global variables
	global g_request_count
	
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
	#===========================================================================
	# init local variab;es 
	#===========================================================================
	vip        = vip_list[0]
	port       = 80

	
	#===========================================================================
	# add service with servers
	#===========================================================================
	ezbox.add_service(vip, port, g_sched_alg, g_sched_alg_opt)
	for server in server_list:
		ezbox.add_server(server.vip, port, server.ip, port)
	
	server_list[0].take_down_loopback()

	print "wait 6 second for EZbox to update"
	time.sleep(6)
	
	process_list = []
	for client in client_list:
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
	
	
	#===========================================================================
	# enable first server  
	#===========================================================================
	process_list = []
	for client in client_list:
		new_log_name = client.logfile_name+'_1'
		client.add_log(new_log_name) 
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	
	server_list[0].configure_loopback()


	for p in process_list:
		p.join()
	
	
	print 'End user test'


#===============================================================================
# Function: run_user_checker
#
# Brief:
#===============================================================================
def run_user_checker(server_list, ezbox, client_list, log_dir,vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	expected_dict = {}
	

	expected_dict[0] = {'client_response_count':g_request_count,
						'g_client_count': len(client_list),
						'no_connection_closed': True,
						'no_404': True,
						'server_count_per_client':(g_server_count - 1)}
	expected_dict[1] = {'client_response_count':g_request_count,
						'g_client_count': len(client_list),
						'no_connection_closed': True,
						'no_404': True,
						'server_count_per_client':g_server_count}
	
	if client_checker(log_dir, expected_dict,2):
		return True
	else:
		return False


#===============================================================================
# Function: print_params
#
# Brief:
#===============================================================================
def print_params():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	print "service_count:  " + str(g_service_count)
	print "server_count:   " + str(g_server_count)
	print "client_count:   " + str(g_client_count)
	print "request_count:  " + str(g_request_count)


#===============================================================================
# Function: main
#
# Brief:
#===============================================================================
def main():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	print_params()

	config = generic_main()
	
	server_list, ezbox, client_list, vip_list = user_init(config['setup_num'])
	
	init_players(server_list, ezbox, client_list, vip_list, config)
	
	run_user_test(server_list, ezbox, client_list, vip_list)
	
	log_dir = collect_logs(server_list, ezbox, client_list)

	gen_rc = general_checker(server_list, ezbox, client_list)
	
	clean_players(server_list, ezbox, client_list, True, config['stop_ezbox'])
	
	user_rc = run_user_checker(server_list, ezbox, client_list, log_dir, vip_list)
	
	if user_rc and gen_rc:
		print 'Test passed !!!'
		exit(0)
	else:
		print 'Test failed !!!'
		exit(1)


main()
