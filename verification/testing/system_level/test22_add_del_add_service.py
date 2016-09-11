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
request_count = 10
server_count = 2
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

def client_execution(client, vip, expects404):
	client.exec_params += " -i %s -r %d -e %s" %(vip, request_count, expects404)
	client.execute()

def run_user_test(server_list, ezbox, client_list, vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	process_list = []
	port = '80'
	vip = vip_list[0]
	
	#add service with first server
	ezbox.add_service(vip, port)
	ezbox.add_server(server_list[0].vip, port, server_list[0].ip, port)

	print "wait 6 second for EZbox to update"
	time.sleep(6)

	for client in client_list:
		process_list.append(Process(target=client_execution, args=(client,vip,False,)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()

	process_list = []
	#remove service
	ezbox.delete_service(vip, port)
  
	print "wait 6 second for EZbox to update"
	time.sleep(6)
 
	for client in client_list:
		new_log_name = client.logfile_name+'_1'
		client.add_log(new_log_name) 
		process_list.append(Process(target=client_execution, args=(client,vip,True,)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
  
	process_list = []
	#add service with second server
	ezbox.add_service(vip, port)
	ezbox.add_server(server_list[1].vip, port, server_list[1].ip, port,)
  
	print "wait 6 second for EZbox to update"
	time.sleep(6)
 
	for client in client_list:
		new_log_name = client.logfile_name+'_2'
		client.add_log(new_log_name) 
		process_list.append(Process(target=client_execution, args=(client,vip, False,)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
		
	print 'End user test'

def run_user_checker(server_list, ezbox, client_list, log_dir, vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	expected_server_list=[]
	expected_server_list.append(server_list[0])
	expected_dict = {}
	expected_dict[0] = {'client_response_count':request_count,
						'client_count': len(client_list), 
						'no_404': True,
						'no_connection_closed': True,
						'expected_servers':expected_server_list,
					 	'server_count_per_client':1}
	expected_dict[1] = {'client_response_count':request_count,
						'client_count': len(client_list), 
						'no_404': False,
						'no_connection_closed': True,
					 	'server_count_per_client':1}
	expected_server_list=[]
	expected_server_list.append(server_list[1])
	expected_dict[2] = {'client_response_count':request_count,
						'client_count': len(client_list), 
						'no_404': True,
						'no_connection_closed': True,
						'expected_servers':expected_server_list,
					 	'server_count_per_client':1}
	
	return client_checker(log_dir, expected_dict, 3)

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

	gen_rc = general_checker(server_list, ezbox, client_list, expected={'no_open_connections': False})
	
	clean_players(server_list, ezbox, client_list, True, config['stop_ezbox'])
	
	user_rc = run_user_checker(server_list, ezbox, client_list, log_dir,vip_list)
	
	if user_rc and gen_rc:
		print 'Test passed !!!'
		exit(0)
	else:
		print 'Test failed !!!'
		exit(1)

main()
