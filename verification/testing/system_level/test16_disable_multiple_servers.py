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
request_count = 200 
server_count = 4
client_count = 10
service_count = 2



#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	dict = generic_init(setup_num, service_count, server_count, client_count)
	
	i = 0
	for s in dict['server_list']:
		s.vip = dict['vip_list'][i%service_count]
		i += 1
		
	return convert_generic_init_to_user_format(dict)

def client_execution(client, vip):
	client.exec_params += " -i %s -r %d" %(vip, request_count)
	client.execute()

def run_user_test(server_list, ezbox, client_list, vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	process_list = []
	port = '80'

	#service scheduling algorithm is SH with port
	for i in range(service_count):
		ezbox.add_service(vip_list[i], port)
	for server in server_list:
		ezbox.add_server(server.vip, port, server.ip, port)
		server.set_large_index_html()
		
	print "wait 6 second for EZbox to update"
	time.sleep(6)
	
	for index, client in enumerate(client_list):
		process_list.append(Process(target=client_execution, args=(client,vip_list[index%service_count],)))
	for p in process_list:
		p.start()

	for i in range(20):		
		time.sleep(2) 
		# Disable server - director will remove server from IPVS
		print 'remove test.html'
		server_list[0].delete_test_html()
		server_list[1].delete_test_html()
		time.sleep(10) 
		print 're-add test.html'
		server_list[0].set_test_html()
		server_list[1].set_test_html()
	
 	for p in process_list:
 		p.join()
	
		
	print 'End user test'

def run_user_checker(server_list, ezbox, client_list, log_dir, vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	expected_servers = {}
	for index, client in enumerate(client_list):
		client_expected_servers=[s.ip for s in server_list if s.vip == vip_list[index%service_count]]
		client_expected_servers.append('Connection closed ERROR')
		expected_servers[client.ip] = client_expected_servers
	expected_dict = {'client_response_count':request_count,
						'client_count': len(client_list),
						'no_connection_closed': False,
						'expected_servers_per_client':expected_servers}
	
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

	clean_players(server_list, ezbox, client_list, True)

	client_rc = run_user_checker(server_list, ezbox, client_list, log_dir, vip_list)

	if client_rc and gen_rc:
		print 'Test passed !!!'
		exit(0)
	else:
		print 'Test failed !!!'
		exit(1)

main()
