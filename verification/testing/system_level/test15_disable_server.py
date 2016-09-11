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
from __builtin__ import enumerate



# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir) 
from e2e_infra import *


#===============================================================================
# Test Globals
#===============================================================================
request_count = 5000 
server_count = 1
client_count = 1
service_count = 1



#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	dict = generic_init(setup_num, service_count, server_count, client_count)
	
	for i,s in enumerate(dict['server_list']):
		s.vip = dict['vip_list'][i%service_count]
		
	return convert_generic_init_to_user_format(dict)

def client_execution(client, vip):
	client.exec_params += " -i %s -r %d -e True" %(vip, request_count)
	client.execute()

def run_user_test(server_list, ezbox, client_list, vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	process_list = []
	port = '80'

	#service scheduling algorithm is SH without port
	for i in range(service_count):
		ezbox.add_service(vip_list[i], port, sched_alg_opt='')
	for server in server_list:
		ezbox.add_server(server.vip, port, server.ip, port)
		
	print "wait 6 second for EZbox to update"
	time.sleep(6)
	
	for index, client in enumerate(client_list):
		process_list.append(Process(target=client_execution, args=(client,vip_list[index%service_count],)))
	for p in process_list:
		p.start()
		
	time.sleep(1) 
	# Disable server - director will remove server from IPVS
	print 'remove test.html'
	server_list[0].delete_test_html()
	time.sleep(60) 
	print 're-add test.html'
	server_list[0].set_test_html()
	
 	for p in process_list:
 		p.join()

def run_user_checker(server_list, ezbox, client_list, log_dir):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	expected_dict = {'client_response_count':request_count,
					'client_count': len(client_list),
					'expected_servers': server_list, 
					'no_404': False,
					'server_count_per_client':2}
	
	return client_checker(log_dir, expected_dict)

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

	client_rc = run_user_checker(server_list, ezbox, client_list, log_dir)

	if client_rc and gen_rc:
		print 'Test passed !!!'
		exit(0)
	else:
		print 'Test failed !!!'
		exit(1)

main()
