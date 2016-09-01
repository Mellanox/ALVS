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
request_count = 1000
server_count = 5
client_count = 4
service_count = 1


#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	dict = generic_init(setup_num, service_count, server_count, client_count)
	
	w = 2
	for s in dict['server_list']:
		s.vip = dict['vip_list'][0]
		s.weight = w
		w += 5
		
	return convert_generic_init_to_user_format(dict)

def client_execution(client, vip):
	client.exec_params += " -i %s -r %d" %(vip, request_count)
	client.execute()

def run_user_test(server_list, ezbox, client_list, vip_list, sched_algorithm):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	process_list = []
	port = '80'
	vip = vip_list[0]
	
	print "service %s is set with %s scheduling algorithm" %(vip,sched_algorithm)
	ezbox.add_service(vip, port, sched_alg=sched_algorithm, sched_alg_opt='')
	
	for server in server_list:
		print "adding server %s to service %s" %(server.ip,server.vip)
		ezbox.add_server(server.vip, port, server.ip, port,server.weight)
	
	print "wait 6 second for EZbox to update"
	time.sleep(6)
	
	for client in client_list:
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
	
	new_weight = 20
	print 'changing weight of server %s from %d to %d' %(server_list[0].ip, server_list[0].weight, new_weight)
	server_list[0].weight = new_weight
	ezbox.modify_server(vip, port, server_list[0].ip, port, weight=new_weight)
	new_weight = 32
	print 'changing weight of server %s from %d to %d' %(server_list[1].ip, server_list[1].weight, new_weight)
	server_list[1].weight = new_weight
	ezbox.modify_server(vip, port, server_list[1].ip, port, weight=new_weight)
	
	process_list = []
	print "wait 6 second for EZbox to update"
	time.sleep(6)
	
	for client in client_list:
		new_log_name = client.logfile_name+'_1'
		client.add_log(new_log_name) 
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
		
	print 'End user test'	

def run_user_checker(server_list, ezbox, client_list, log_dir,vip_list, sched_alg):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	expected_dict = {}
	old_server_list = copy.deepcopy(server_list)
	old_server_list[0].weight = 2
	old_server_list[1].weight = 7
	sd = 0.02
	
	expected_dict[0] = {'client_response_count':request_count,
						'client_count': len(client_list),
						'no_connection_closed': True,
						'no_404': True,
						'expected_servers':old_server_list,
						'check_distribution':(old_server_list,vip_list,sd,sched_alg)}
	expected_dict[1] = {'client_response_count':request_count,
						'client_count': len(client_list),
						'no_connection_closed': True,
						'no_404': True,
						'expected_servers':server_list,
						'check_distribution':(server_list,vip_list,sd,sched_alg)}
	
	return client_checker(log_dir, expected_dict,2)

#===============================================================================
# main function
#===============================================================================
def tests38_39_main(sched_alg):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	config = generic_main()
	
	server_list, ezbox, client_list, vip_list = user_init(config['setup_num'])
	
	init_players(server_list, ezbox, client_list, vip_list, config)
	
	run_user_test(server_list, ezbox, client_list, vip_list, sched_alg)
	
	log_dir = collect_logs(server_list, ezbox, client_list)

	gen_rc = general_checker(server_list, ezbox, client_list)
	
	clean_players(server_list, ezbox, client_list, True)
	
	user_rc = run_user_checker(server_list, ezbox, client_list, log_dir, vip_list, sched_alg)
	
	if user_rc and gen_rc:
		print 'Test passed !!!'
		exit(0)
	else:
		print 'Test failed !!!'
		exit(1)

