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
g_server_count   = None
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
	setup_list = get_setup_list(g_setup_num)
	
	for i in range(g_servers_to_add):
		# create new server object & add to list
		new_server = HttpServer(ip   = setup_list[g_next_vm_index]['ip'],
							hostname = setup_list[g_next_vm_index]['hostname'],
							username = "root",
							password = "3tango",
							vip      = vip,
							eth      ='ens6')
		server_list.append(new_server)
		g_next_vm_index+=1
	
		# init new server & add to service 
		new_server.init_server(new_server.ip)
		ezbox.add_server(new_server.vip, port, new_server.ip, port)

	
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



#===============================================================================
# Function: set_user_params
#
# Brief:
#===============================================================================
def run_user_checker(server_list, ezbox, client_list, log_dir):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	expected_dict = {}
	expected_dict[0] = {'client_response_count':g_request_count,
					'client_count'            : len(client_list), 
					'server_count_per_client' :  len(server_list) - g_servers_to_add,
					'no_404'                  : True}
	expected_dict[1] = {'client_response_count':g_request_count,
					'client_count'            : len(client_list),
					'server_count_per_client' :  len(server_list),
					'expected_servers'        : server_list,
					'no_404'                  : True}
	
	if client_checker(log_dir, expected_dict, 2):
		print 'Test passed !!!'
		exit(0)
	else:
		print 'Test failed !!!'
		exit(1)



#===============================================================================
# Function: set_user_params
#
# Brief:
#===============================================================================
def set_user_params(setup_num, test):
	# modified global variables
	global g_setup_num
	global g_server_count
	global g_client_count
	global g_service_count
	global g_servers_to_add
	global g_sched_alg
	global g_sched_alg_opt
	
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	# general
	g_setup_num = setup_num
	
	# config test parms
	if test == 8:
		g_server_count   = 2
		g_client_count   = 1
		g_service_count  = 1
		g_servers_to_add = 1
		g_sched_alg        = "sh"
		g_sched_alg_opt    = "-b sh-port"
	elif test == 9:
		g_server_count   = 5
		g_client_count   = 1
		g_service_count  = 1
		g_servers_to_add = 5
		g_sched_alg        = "sh"
		g_sched_alg_opt    = "-b sh-port"
	elif test == 10:
		g_server_count   = 5
		g_client_count   = 10
		g_service_count  = 1
		g_servers_to_add = 10
		g_sched_alg        = "sh"
		g_sched_alg_opt    = "-b sh-port"
	else:
		print "ERROR: unsupported test number: %d" %(test)
		exit()
	
	# print configuration
	print "setup_num:      " + str(g_setup_num)
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
def main():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	usage = "usage: %prog [-s, -t]"
	parser = OptionParser(usage=usage, version="%prog 1.0")
	
	parser.add_option("-s", "--setup_num", dest="setup_number",
					  help="Setup number", type="int")
	parser.add_option("-t", "--test_num", dest="test_num",
					  help="Number of test to run", default=8, type="int")

	(options, args) = parser.parse_args()

	if not options.setup_number:
		log('HTTP IP is not given')
		exit(1)


	set_user_params(options.setup_number, options.test_num) 
	
	server_list, ezbox, client_list, vip_list = user_init(g_setup_num)

	init_players(server_list, ezbox, client_list, vip_list)
	
	run_user_test_step(server_list, ezbox, client_list, vip_list)
	
	log_dir = collect_logs(server_list, ezbox, client_list)

	clean_players(server_list, ezbox, client_list)
	
	run_user_checker(server_list, ezbox, client_list, log_dir)


main()
