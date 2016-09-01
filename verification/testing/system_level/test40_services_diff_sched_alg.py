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
server_count  = 9
client_count  = 3
service_count = 3


#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	dict = generic_init(setup_num, service_count, server_count, client_count)
	
	sh_service = dict['vip_list'][0]
	rr_service = dict['vip_list'][1]
	wrr_service = dict['vip_list'][2]
	
	w1 = 1
	w2 = 1
	w3 = 97
	index = 0
	for s in dict['server_list']:
		if index < (server_count-6):
			print "init server %d weight = %d" %(index,w1)
			s.vip = sh_service
			s.weight = w1
			w1 = 5 * (index + 1)
		elif index < (server_count-3):
			print "init server %d weight = %d" %(index,w2)
			s.vip = rr_service
			s.weight = w2
		else:
			print "init server %d weight = %d" %(index,w3)
			s.vip = wrr_service
			s.weight = w3
			w3 += 1
		index += 1	
		
	return convert_generic_init_to_user_format(dict)

def client_execution(client, vip):
	client.exec_params += " -i %s -r %d" %(vip, request_count)
	client.execute()

def run_user_test(server_list, ezbox, client_list, vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	process_list = []
	port = '80'
	
	print "service %s is set with SH scheduling algorithm" %(vip_list[0])
	ezbox.add_service(vip_list[0], port, sched_alg='sh', sched_alg_opt='-b sh-port')
	print "service %s is set with RR scheduling algorithm" %(vip_list[1])
	ezbox.add_service(vip_list[1], port, sched_alg='rr', sched_alg_opt='')
	print "service %s is set with WRR scheduling algorithm" %(vip_list[2])
	ezbox.add_service(vip_list[2], port, sched_alg='wrr', sched_alg_opt='')
	
	for server in server_list:
		print "adding server %s to service %s" %(server.ip,server.vip)
		ezbox.add_server(server.vip, port, server.ip, port, server.weight)
	
	print "wait 6 second for EZbox to update"
	time.sleep(6)
	#raw_input("Press any key to continue...")
	for index, client in enumerate(client_list):
		process_list.append(Process(target=client_execution, args=(client,vip_list[index],)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
	#raw_input("Press any key to continue...")
	print 'End user test'

def run_user_checker(server_list, ezbox, client_list, log_dir,vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	expected_dict = {'client_response_count':request_count,
						'client_count': len(client_list),
						'no_connection_closed': True,
						'no_404': True,
						'check_distribution':(server_list,vip_list,0.05),
						'server_count_per_client':server_count/service_count,
						'expected_servers':server_list}
	
	expected_servers = {}
	for index, client in enumerate(client_list):
		client_expected_servers=[s.ip for s in server_list if s.vip == vip_list[index]]
		expected_servers[client.ip] = client_expected_servers

	expected_dict['expected_servers_per_client'] = expected_servers
	
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
	
	user_rc = run_user_checker(server_list, ezbox, client_list, log_dir, vip_list)
	
	if user_rc and gen_rc:
		print 'Test passed !!!'
		exit(0)
	else:
		print 'Test failed !!!'
		exit(1)

main()
