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

from os import listdir
from os.path import isfile, join


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
	client_count = 20
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
	host_name, chip_name, interface =  get_ezbox_names(setup_num)
	ezbox = ezbox_host(management_ip=host_name, nps_ip=chip_name, username='root', password='ezchip', interface=interface)
	
	return (server_list, ezbox, client_list, vip_list)

def client_execution(client, vip):
	repeat = 10 
	client.exec_params += " -i %s -r %d" %(vip, repeat)
	client.execute()

def run_user_test(server_list, ezbox, client_list, vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
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

def run_user_checker(server_list, ezbox, client_list, log_dir):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	file_list = [log_dir+'/'+f for f in listdir(log_dir) if isfile(join(log_dir, f))]
	responses = []
	for filename in file_list:
		logfile=open(filename, 'r')
		client_responses = {}
		for line in logfile:
			if len(line) > 2 and line[0] != '#':
				split_line = line.split(':')
				key = split_line[1].strip()
				client_responses[key] = client_responses.get(key, 0) + 1
		responses.append(client_responses)	 
	
	if len(responses) != len(client_list):
		print 'ERROR: wrong number of logs. client count = %d , log count = %d' %(len(responses),len(client_list))
	
	for index,client_responses in enumerate(responses):
		print 'testing client %d ...' %index
		if len(client_responses) != 1:
			print 'ERROR: client received responses from different number of expected servers. expected = %d , received = %d' %(1, len(client_responses))
		total = 0
		for ip, count in client_responses.items():
			print 'response count from server %s = %d' %(ip,count)
			total += count
		if total != 10:
			print 'ERROR: client received wrong number of responses. expected = %d , received = %d' %(10, total)

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
 
	init_players(server_list, ezbox, client_list, vip_list)
  	
	run_user_test(server_list, ezbox, client_list, vip_list)
  	
	log_dir = collect_logs(server_list, ezbox, client_list)
	run_user_checker(server_list, ezbox, client_list, log_dir)
# 	check_connections(ezbox)
	
 	clean_players(server_list, ezbox, client_list)
	



main()
