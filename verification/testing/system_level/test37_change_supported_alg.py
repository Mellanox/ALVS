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
server_count  = 4
client_count  = 4
service_count = 1


#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	vip_list = [get_setup_vip(setup_num,i) for i in range(service_count)]

	index = 0
	setup_list = get_setup_list(setup_num)

	server_list=[]
	w=3
	for i in range(server_count):
		server_list.append(HttpServer(ip = setup_list[index]['ip'],
						  hostname = setup_list[index]['hostname'], 
						  username = "root", 
						  password = "3tango", 
						  vip = vip_list[0],
						  eth='ens6',
						  weight=w))
		print "init server %d weight = %d" %(i,w)
		index+=1
		w+=2
	
	client_list=[]
	for i in range(client_count):
		client_list.append(HttpClient(ip = setup_list[index]['ip'],
						  hostname = setup_list[index]['hostname']))
		index+=1
	

	# EZbox
	ezbox = ezbox_host(setup_num)
	
	return (server_list, ezbox, client_list, vip_list)

def client_execution(client, vip):
	client.exec_params += " -i %s -r %d" %(vip, request_count)
	client.execute()

def run_user_test(server_list, ezbox, client_list, vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	process_list = []
	port = '80'
	vip = vip_list[0]
	
	print "service %s is set with SH scheduling algorithm" %(vip)
	ezbox.add_service(vip, port, sched_alg='sh', sched_alg_opt='-b sh-port')
	for server in server_list:
		print "adding server %s to service %s" %(server.ip,server.vip)
		ezbox.add_server(server.vip, port, server.ip, port, server.weight)
	
	print "wait 6 second for EZbox to update"
	time.sleep(6)
	
	for client in client_list:
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
	
	process_list = []
	print "modify scheduling algorithm of service %s to RR" %(vip)
	ezbox.modify_service(vip, port, sched_alg='rr', sched_alg_opt='')
	
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
	
	process_list = []
	print "modify scheduling algorithm of service %s to WRR" %(vip)
	ezbox.modify_service(vip, port, sched_alg='wrr', sched_alg_opt='')
	
	print "wait 6 second for EZbox to update"
	time.sleep(6)
	
	for client in client_list:
		new_log_name = client.logfile_name[:-2]+'_2'
		client.add_log(new_log_name) 
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
	
	process_list = []
	print "modify scheduling algorithm of service %s to SH" %(vip)
	ezbox.modify_service(vip, port,  sched_alg='sh', sched_alg_opt='-b sh-port')
	
	print "wait 6 second for EZbox to update"
	time.sleep(6)
	
	for client in client_list:
		new_log_name = client.logfile_name[:-2]+'_3'
		client.add_log(new_log_name) 
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
	
	process_list = []
	print "modify scheduling algorithm of service %s to WRR" %(vip)
	ezbox.modify_service(vip, port,  sched_alg='wrr', sched_alg_opt='')
	
	print "wait 6 second for EZbox to update"
	time.sleep(6)
	
	for client in client_list:
		new_log_name = client.logfile_name[:-2]+'_4'
		client.add_log(new_log_name) 
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
	
	process_list = []
	print "modify scheduling algorithm of service %s to RR" %(vip)
	ezbox.modify_service(vip, port,  sched_alg='rr', sched_alg_opt='')
	
	print "wait 6 second for EZbox to update"
	time.sleep(6)
	
	for client in client_list:
		new_log_name = client.logfile_name[:-2]+'_5'
		client.add_log(new_log_name) 
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
	
	process_list = []
	print "modify scheduling algorithm of service %s to SH" %(vip)
	ezbox.modify_service(vip, port,  sched_alg='sh', sched_alg_opt='-b sh-port')
	
	print "wait 6 second for EZbox to update"
	time.sleep(6)
	
	for client in client_list:
		new_log_name = client.logfile_name[:-2]+'_6'
		client.add_log(new_log_name) 
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
	
	print 'End user test'

def run_user_checker(server_list, ezbox, client_list, log_dir, vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	sh_sd = 0.05
	rr_wrr_sd = 0.02
	expected_dict = {}
	expected_dict[0] = {'client_response_count':request_count,
						'client_count': len(client_list), 
						'no_404': True,
						'no_connection_closed':True,
						'check_distribution':(server_list,vip_list,sh_sd),
						'expected_servers':server_list}
	expected_dict[1] = {'client_response_count':request_count,
						'client_count': len(client_list), 
						'no_404': True,
						'no_connection_closed':True,
						'check_distribution':(server_list,vip_list,rr_wrr_sd,"rr"),
						'expected_servers':server_list}
	expected_dict[2] = {'client_response_count':request_count,
						'client_count': len(client_list), 
						'no_404': True,
						'no_connection_closed':True,
						'check_distribution':(server_list,vip_list,rr_wrr_sd),
						'expected_servers':server_list}
	expected_dict[3] = {'client_response_count':request_count,
						'client_count': len(client_list), 
						'no_404': True,
						'no_connection_closed':True,
						'check_distribution':(server_list,vip_list,sh_sd),
						'expected_servers':server_list}
	expected_dict[4] = {'client_response_count':request_count,
						'client_count': len(client_list), 
						'no_404': True,
						'no_connection_closed':True,
						'check_distribution':(server_list,vip_list,rr_wrr_sd),
						'expected_servers':server_list}
	expected_dict[5] = {'client_response_count':request_count,
						'client_count': len(client_list), 
						'no_404': True,
						'no_connection_closed':True,
						'check_distribution':(server_list,vip_list,rr_wrr_sd,"rr"),
						'expected_servers':server_list}
	expected_dict[6] = {'client_response_count':request_count,
						'client_count': len(client_list), 
						'no_404': True,
						'no_connection_closed':True,
						'check_distribution':(server_list,vip_list,sh_sd),
						'expected_servers':server_list}
	return client_checker(log_dir, expected_dict, 7)

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
