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
g_next_vm_index  = 0

# got from user
g_setup_num      = None
g_use_4k_cpus    = None

# Configured acording to test number
g_server_count   = 0
g_client_count   = 0
g_service_count  = 0
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
	# modified global variables
	global g_next_vm_index
	
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	vip_list   = [get_setup_vip(setup_num,i) for i in range(g_service_count)]
	setup_list = get_setup_list(setup_num)

	server_list=[]
	for i in range(g_server_count):
		server_list.append(HttpServer(ip = setup_list[g_next_vm_index]['ip'],
						  hostname = setup_list[g_next_vm_index]['hostname'], 
						  username = "root", 
						  password = "3tango", 
						  vip = vip_list[0],
						  eth='ens6'))
		g_next_vm_index+=1
	
	client_list=[]
	for i in range(g_client_count):
		client_list.append(HttpClient(ip = setup_list[g_next_vm_index]['ip'],
						  hostname = setup_list[g_next_vm_index]['hostname']))
		g_next_vm_index+=1
	

	# get EZbox
	ezbox = ezbox_host(setup_num)

	return (server_list, ezbox, client_list, vip_list)


#===============================================================================
# Function: run_user_test
#
# Brief:
#===============================================================================
def configure_services(server_list, ezbox, client_list, vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
	#===========================================================================
	# init local variab;es 
	#===========================================================================

	
	#===========================================================================
	# add services with servers
	#===========================================================================
	port       = 80
	for vip in vip_list:
		ezbox.add_service(vip, port, g_sched_alg, g_sched_alg_opt)
		for server in server_list:
			ezbox.add_server(server.vip, port, server.ip, port)

	print 'End user test'



#===============================================================================
# Function: set_user_params
#
# Brief:
#===============================================================================
def set_user_params(setup_num, use_4k_cpus):
	# modified global variables
	global g_setup_num
	global g_use_4_k_cpus
	
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	# user configuration
	g_setup_num   = setup_num
	g_use_4k_cpus = True if use_4k_cpus.lower() == 'true' else False 
	
	# print configuration
	print "setup_num:      " + str(g_setup_num)
	print "use_4k_cpus     " + str(g_use_4k_cpus)
	print "service_count:  " + str(g_service_count)
	print "server_count:   " + str(g_server_count)
	print "client_count:   " + str(g_client_count)


#===============================================================================
# Function: set_user_params
#
# Brief:
#===============================================================================
def main():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	if len(sys.argv) != 3:
		print "script expects exactly 2 input arguments"
		print "Usage: client_requests.py <setup_num> <True/False (use 4 k CPUs)>"
		exit(1)
	
	set_user_params( int(sys.argv[1]), sys.argv[2])
	
	server_list, ezbox, client_list, vip_list = user_init(g_setup_num)
	
	init_players(server_list, ezbox, client_list, vip_list, True, g_use_4k_cpus)
	
	configure_services(server_list, ezbox, client_list, vip_list)


main()
