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
# User Area function needed by infrastructure
#===============================================================================

def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	server_count = 5
	client_count = 10
	service_count = 1

	vip_list = [get_setup_vip(setup_num,i) for i in range(service_count)]

	index = 0
	setup_list = get_setup_list(setup_num)

	server_list=[]
	for i in range(server_count):
		server_list.append(HttpServer(ip = setup_list[index]['ip'],
						  hostname = setup_list[index]['hostname'], 
						  username = "root", 
						  password = "3tango", 
						  vip = vip_list[0],
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
	host_name, chip_name =  get_ezbox_names(setup_num)
	ezbox = ezbox_host(management_ip=host_name, nps_ip=chip_name, username='root', password='ezchip')

	return (server_list, ezbox, client_list, vip_list)

def client_execution(client, vip):
	repeat = 10 
	client.exec_params += " -i %s -r %d" %(vip, repeat)
	client.execute()

def run_user_test(server_list, ezbox, client_list, vip_list):
	print 'Running user test'
	process_list = []
	vip = vip_list[0]
	port = '80'

	add_service(ezbox, vip, port, sched_alg_opt='')
	for server in server_list:
		add_server(ezbox, vip, port, server.ip, port)
	
	for client in client_list:
		process_list.append(Process(target=client_execution, args=(client,vip,)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
		
	print 'End user test'
	pass
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

	init_players(server_list, ezbox, client_list)
	
	run_user_test(server_list, ezbox, client_list, vip_list)
	#run_test(server_list, client_list)
	
	collect_logs(server_list, ezbox, client_list)
	
	clean_players(server_list, ezbox, client_list)
	



main()