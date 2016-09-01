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
request_count = 1000
server_count = 5
client_count = 5
service_count = 1


#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	dict = generic_init(setup_num, service_count, server_count, client_count)
	
	w=1
	for s in dict['server_list']:
		s.vip = dict['vip_list'][0]
		s.weight = w
		w+=1
		
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
		ezbox.add_server(server.vip, port, server.ip, port, server.weight)
	
	print "wait 6 second for EZbox to update"
	time.sleep(6)
	
	for index, client in enumerate(client_list):
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
	#raw_input("Press any key to continue...")
	print 'End user test'	

def run_user_checker(server_list, ezbox, client_list, log_dir,vip_list, sched_alg):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	sd = 0.01
	expected_dict = {'client_response_count':request_count,
						'client_count': len(client_list),
						'no_connection_closed': True,
						'no_404': True,
						'check_distribution':(server_list,vip_list,sd,sched_alg),
						'server_count_per_client':server_count/service_count,
						'expected_servers':server_list}
	
	return client_checker(log_dir, expected_dict)

#===============================================================================
# main function
#===============================================================================
def test_33_34_main(sched_alg):
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
		print "Test failed !!!, user_rc %s gen_rc %s" %(str(user_rc), str(gen_rc))
		exit(1)
