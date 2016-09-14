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
request_count = 100
server_count = 3
client_count = 1
service_count = 1


#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	dict = generic_init(setup_num, service_count, server_count, client_count)
	
	w = 1
	for s in dict['server_list']:
		s.vip = dict['vip_list'][0]
		s.weight = w
		w *= 5
		
	return convert_generic_init_to_user_format(dict)

def client_execution(client, vip):
	client.exec_params += " -i %s -r %d" %(vip, request_count)
	client.execute()

def run_user_test(server_list, ezbox, client_list, vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	process_list = []
	port = '80'
	vip = vip_list[0]
	
	print "service %s is set with WRR scheduling algorithm" %(vip_list[0])
	ezbox.add_service(vip_list[0], port, sched_alg='wrr', sched_alg_opt='')
	
	for server in server_list:
		print "adding server %s to service %s" %(server.ip,server.vip)
		ezbox.add_server(server.vip, port, server.ip, port, server.weight)
	
	
	for client in client_list:
		process_list.append(Process(target=client_execution, args=(client,vip_list[0],)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
	
	server = server_list[server_count-1]
	print "delete server %s with weight %d" %(server.ip, server.weight)
	ezbox.delete_server(server.vip, port, server.ip, port)
	process_list = []
	
	
	for client in client_list:
		new_log_name = client.logfile_name+'_1'
		client.add_log(new_log_name) 
		process_list.append(Process(target=client_execution, args=(client,vip_list[0],)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
		
	print 'End user test'

def run_user_checker(server_list, ezbox, client_list, log_dir, vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	new_server_list = copy.deepcopy(server_list[:server_count-1])
	sd = 0.02
	expected_dict = {}
	expected_dict[0] = {'client_response_count':request_count,
						'client_count': len(client_list),
						'no_404': True,
						'no_connection_closed': True,
						'check_distribution':(server_list,vip_list,sd),
						'server_count_per_client':server_count/service_count,
						'expected_servers': server_list}
	expected_dict[1] = {'client_response_count':request_count,
						'client_count': len(client_list), 
						'no_connection_closed': True,
						'no_404': True,
						'check_distribution':(new_server_list,vip_list,sd),
						'server_count_per_client':2,
						'expected_servers': new_server_list}
	
	return client_checker(log_dir, expected_dict, 2)

#===============================================================================
# main function
#===============================================================================
def main():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	config = generic_main()
	
	server_list, ezbox, client_list, vip_list = user_init(config['setup_num'])
	
	init_players(server_list, ezbox, client_list, vip_list, config)
	
	run_user_test(server_list, ezbox, client_list, vip_list)
	
	log_dir = collect_logs(server_list, ezbox, client_list)

	gen_rc = general_checker(server_list, ezbox, client_list)
	
	clean_players(server_list, ezbox, client_list, True, config['stop_ezbox'])
	
	user_rc = run_user_checker(server_list, ezbox, client_list, log_dir,vip_list)
	
	if user_rc and gen_rc:
		print 'Test passed !!!'
		exit(0)
	else:
		print 'Test failed !!!'
		exit(1)

main()
