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
request_count = 1
server_count = 1
client_count = 1
service_count = 1



#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	dict = generic_init(setup_num, service_count, server_count, client_count)
	
	for s in dict['server_list']:
		s.vip = dict['vip_list'][0]
		
	return convert_generic_init_to_user_format(dict)

def client_execution(client, vip):
	connTimeout = 30
	client.exec_params += " -i %s -r %d -t %d " %(vip, request_count,connTimeout)
	client.execute()

def run_user_test(server_list, ezbox, client_list, vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	process_list = []
	port = '80'
	vip = vip_list[0]
	
	ezbox.add_service(vip, port)
	for server in server_list:
		server.set_extra_large_index_html()
		ezbox.add_server(server.vip, port, server.ip, port)
	
	time.sleep(5)
	for client in client_list:
		process_list.append(Process(target=client_execution, args=(client,vip,)))
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

def run_user_checker(ezbox):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
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

	server_list, ezbox, client_list, vip_list = user_init(setup_num)

	init_players(server_list, ezbox, client_list, vip_list, True, use_4_k_cpus)
	
	run_user_test(server_list, ezbox, client_list, vip_list)
	
	user_rc = run_user_checker(ezbox)
	
	clean_players(server_list, ezbox, client_list, True, config['stop_ezbox'])
	
	if user_rc:
		print 'Test passed !!!'
		exit(0)
	else:
		print 'Test failed !!!'
		exit(1)

main()
