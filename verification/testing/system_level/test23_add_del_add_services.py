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
request_count = 50
server_count = 10
client_count = 5
service_count = 5



#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	vip_list = [get_setup_vip(setup_num,i) for i in range(service_count)]

	index = 0
	setup_list = get_setup_list(setup_num)

	server_list=[]
	for i in range(server_count):
		server_list.append(HttpServer(ip = setup_list[index]['ip'],
						  hostname = setup_list[index]['hostname'], 
						  username = "root", 
						  password = "3tango", 
						  vip = vip_list[i%service_count],#servers0,5->service 0 ..... servers4,9->service 4
						  eth='ens6'))
		index+=1
	
	script_dirname = os.path.dirname(os.path.realpath(__file__))
	client_list=[]
	for i in range(client_count):
		client_list.append(HttpClient(ip = setup_list[index]['ip'],
						  hostname = setup_list[index]['hostname'], 
						  username = "root", 
						  password = "3tango",
						  exe_path    = script_dirname,
						  exe_script  = "basic_client_requests.py",
						  exec_params = ""))
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
	
	#add services
	for i in range(service_count):
		ezbox.add_service(vip_list[i], port)
	for server in server_list:
		ezbox.add_server(server.vip, port, server.ip, port)
	
	time.sleep(5)
	for index, client in enumerate(client_list):
		process_list.append(Process(target=client_execution, args=(client,vip_list[index],)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()

	#remove services
	process_list = []
	for i in range(service_count):
		ezbox.delete_service(vip_list[i], port)
	time.sleep(5)
	for index, client in enumerate(client_list):
		new_log_name = client.logfile_name+'_1'
		client.add_log(new_log_name) 
		process_list.append(Process(target=client_execution, args=(client,vip_list[index],)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()

	#add services with different servers
	process_list = []
	for i in range(service_count):
		ezbox.add_service(vip_list[i], port)
	#change service foreach server
	for i in range(server_count):
		server_list[i].update_vip(vip_list[(i+1)%service_count])#servers0,5->service 1 ..... servers4,9->service 0
	for server in server_list:
		ezbox.add_server(server.vip, port, server.ip, port)
	time.sleep(5)
	for index, client in enumerate(client_list):
		new_log_name = client.logfile_name+'_2'
		client.add_log(new_log_name) 
		process_list.append(Process(target=client_execution, args=(client,vip_list[index],)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
  		
	print 'End user test'

def run_user_checker(server_list, ezbox, client_list, log_dir, vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	expected_dict = {}
	expected_dict[0] = {'client_response_count':request_count,
						'client_count': len(client_list), 
						'no_404': True,
						'no_connection_closed': True,
					 	'server_count_per_client':server_count/service_count}
	expected_dict[1] = {'client_response_count':request_count,
						'client_count': len(client_list), 
						'no_404': False,
						'no_connection_closed': True,
					 	'server_count_per_client':1}
	expected_dict[2] = {'client_response_count':request_count,
						'client_count': len(client_list), 
						'no_404': True,
						'no_connection_closed': True,
					 	'server_count_per_client':server_count/service_count}
	
	return client_checker(log_dir, expected_dict, 3)

#===============================================================================
# main function
#===============================================================================
def main():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	if len(sys.argv) != 2:
		print "script expects exactly 1 input argument"
		print "Usage: client_requests.py <setup_num>"
		exit(1)
	
	setup_num  = int(sys.argv[1])
	
	server_list, ezbox, client_list, vip_list = user_init(setup_num)
	
	init_players(server_list, ezbox, client_list, vip_list, use_director=True, use_4k_cpus=False)
	
	run_user_test(server_list, ezbox, client_list, vip_list)
	
	log_dir = collect_logs(server_list, ezbox, client_list)

	gen_rc = general_checker(server_list, ezbox, client_list)
	
	clean_players(server_list, ezbox, client_list, use_director=True)
	
	user_rc = run_user_checker(server_list, ezbox, client_list, log_dir,vip_list)
	
	if user_rc and gen_rc:
		print 'Test passed !!!'
		exit(0)
	else:
		print 'Test failed !!!'
		exit(1)

main()
