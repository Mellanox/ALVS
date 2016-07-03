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
g_request_count_def  = 500
g_request_count_s1   = 2*g_request_count_def
g_request_count_s2   = 10
g_request_count      = g_request_count_def
g_next_vm_index      = 0

# got from user
g_setup_num      = None
g_use_4k_cpus    = None

# Configured acording to test number
g_server_count   = 2
g_client_count   = 2
g_service_count  = 1
g_sched_alg      = "sh"
g_sched_alg_opt  = "-b sh-port"
g_base_weight    = 2



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
						  eth='ens6',
						  weight=g_base_weight))
		g_next_vm_index+=1
	
	script_dirname = os.path.dirname(os.path.realpath(__file__))
	client_list=[]
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
	new_weight = 0

	
	print "wait 20 second for EZbox to raise"
	time.sleep(20)
	
	#===========================================================================
	# add service with servers
	#===========================================================================
	ezbox.add_service(vip, port, g_sched_alg, g_sched_alg_opt)
	for server in server_list:
		ezbox.add_server(server.vip, port, server.ip, port)
	
	print "wait 6 second for EZbox to update"
	time.sleep(6)
	
	
	#===========================================================================
	# send requests on current configuration 
	#===========================================================================
	process_list = []
	for client in client_list:
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
	
	
	#===========================================================================
	# Change weight of the first server to zero & send requests 
	#===========================================================================
	print 'change first server weight to zero while sending requests. servers: %s' %(server_list[0].ip)
	
	# change request count. try to see change of weigth 0
	g_request_count = g_request_count_s1

	process_list = []
	for client in client_list:
		new_log_name = client.logfile_name+'_1'
		client.add_log(new_log_name) 
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	
	time.sleep(1)
	server_list[0].weight = new_weight
	ezbox.modify_server(vip, port, server_list[0].ip, port, weight=new_weight)

	for p in process_list:
		p.join()

	#===========================================================================
	# Change weight of all servers to zero & send requests 
	#===========================================================================
	print 'change all servers weight to zero.'
	
	# change request count because expecting ERROR 404
	g_request_count = g_request_count_s2
	
	for s in server_list:
		s.weight = new_weight
		ezbox.modify_server(vip, port, s.ip, port, weight=s.weight)
		print "server %s weigth change to zero" %(s.ip)
		
 
	print "wait 6 second for EZbox to update"
	time.sleep(6)
 
	process_list = []
	for client in client_list:
		new_log_name = client.logfile_name[:-2]+'_2'
		client.add_log(new_log_name) 
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
	
	#===========================================================================
	# change weight back to default 
	#===========================================================================
	print 'change all servers weight to default'
	
	# change request count back to default
	g_request_count = g_request_count_def

	for idx,s in enumerate(server_list):
		s.weight = g_base_weight
		ezbox.modify_server(vip, port, s.ip, port, weight=s.weight)
		

	process_list = []
	time.sleep(5)
	for client in client_list:
		new_log_name = client.logfile_name[:-2]+'_3'
		client.add_log(new_log_name) 
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
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
	

	# TODO: test weight
	expected_dict[0] = {'client_response_count':g_request_count,
						'g_client_count': len(client_list),
						'no_connection_closed': True,
						'no_404': True,
						'server_count_per_client':g_server_count,
						'check_distribution':(server_list,vip_list,0.02)}
	expected_dict[1] = {'client_response_count':g_request_count_s1,
						'g_client_count': len(client_list),
						'no_connection_closed': True,
						'no_404': True}
	expected_dict[2] = {'client_response_count':g_request_count_s2,
						'g_client_count': len(client_list),
						'no_connection_closed': True,
						'no_404': False}
	expected_dict[3] = {'client_response_count':g_request_count,
						'g_client_count': len(client_list),
						'no_connection_closed': True,
						'no_404': True,
						'server_count_per_client':g_server_count} # TODO: change 1 with variable
	
	if client_checker(log_dir, expected_dict,4):
		return True
	return False


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
	print "request_count:  " + str(g_request_count)


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
	
	run_user_test(server_list, ezbox, client_list, vip_list)
	
	log_dir = collect_logs(server_list, ezbox, client_list)

	gen_rc = general_checker(server_list, ezbox, client_list)
	
	clean_players(server_list, ezbox, client_list, use_director=True)
	
	user_rc = run_user_checker(server_list, ezbox, client_list, log_dir, vip_list)
	
	if user_rc and gen_rc:
		print 'Test passed !!!'
		exit(0)
	else:
		print 'Test failed !!!'
		exit(1)


main()
