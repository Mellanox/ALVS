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
request_count = 500
server_count  = 5
client_count  = 5
service_count = 1



#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	dict = generic_init(setup_num, service_count, server_count, client_count)
	
	i = 0
	for s in dict['server_list']:
		s.vip = dict['vip_list'][i%service_count]
		s.weight = 1
		i+=1
		
	return convert_generic_init_to_user_format(dict)

def client_execution(client, vip):
	client.exec_params += " -i %s -r %d" %(vip, request_count)
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
	time.sleep(1)
	for index, client in enumerate(client_list):
		process_list.append(Process(target=client_execution, args=(client,vip_list[index%service_count],)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
		
	
	process_list = []
	#service scheduling algorithm is SH with port
	for i in range(service_count):
		print "modify service to sh-port"
		time.sleep(5)
		ezbox.modify_service(vip_list[i], port, sched_alg_opt='-b sh-port')
		print "finished modify service to sh-port"
		time.sleep(5)
	for index, client in enumerate(client_list):
		new_log_name = client.logfile_name+'_1'
		client.add_log(new_log_name) 
		process_list.append(Process(target=client_execution, args=(client,vip_list[index%service_count],)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
	
	process_list = []
	#service scheduling algorithm is SH without port
	for i in range(service_count):
		ezbox.modify_service(vip_list[i], port, sched_alg_opt='')
	for index, client in enumerate(client_list):
		new_log_name = client.logfile_name[:-2]+'_2'
		client.add_log(new_log_name) 
		process_list.append(Process(target=client_execution, args=(client,vip_list[index%service_count],)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
		
	print 'End user test'

def run_user_checker(server_list, ezbox, client_list, log_dir, vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	servers = [s.ip for s in server_list]
	expected_servers = dict((client.ip, servers) for client in client_list)
	expected_dict = {}
	expected_dict[0] = {'client_response_count':request_count,
						'client_count': len(client_list), 
						'no_404': True,
						'no_connection_closed':True,
					 	'server_count_per_client':1}
	expected_dict[1] = {'client_response_count':request_count,
						'client_count': len(client_list), 
						'no_connection_closed':True,
						'expected_servers_per_client':expected_servers,
						'check_distribution':(server_list,vip_list,0.05),
 						'no_404': True,
 					 	'server_count_per_client':server_count/service_count}
	expected_dict[2] = {'client_response_count':request_count,
						'client_count': len(client_list), 
						'no_connection_closed':True,
						'no_404': True,
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
	
	gen_rc = general_checker(server_list, ezbox, client_list)
	
	clean_players(server_list, ezbox, client_list, True, config['stop_ezbox'])
	
	user_rc = run_user_checker(server_list, ezbox, client_list, log_dir, vip_list)
	
	if user_rc and gen_rc:
		print 'Test passed !!!'
		exit(0)
	else:
		print 'Test failed !!!'
		exit(1)
	

main()
