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
	client_count = 1
	service_count = 1

	dict = generic_init(setup_num, service_count, server_count, client_count)
	
	for s in dict['server_list']:
		s.vip = dict['vip_list'][0]

	for c in dict['client_list']:
		c.exe_script = "test3_client_requests.py"

	return convert_generic_init_to_user_format(dict)


def client_execution(client, vip, setup_num):
	repeat = 10 
	client.exec_params += " -i %s -c %s -s %d -r %d" %(vip,client.ip,setup_num,repeat)
	client.execute(exe_prog="python2.7")

def run_user_test(server_list, ezbox, client_list, vip_list, setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	process_list = []
	vip = vip_list[0]
	port = '80'

	ezbox.add_service(vip, port)
	for server in server_list:
		ezbox.add_server(vip, port, server.ip, port)

	for client in client_list:
		process_list.append(Process(target=client_execution, args=(client,vip,setup_num)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
	
	print 'End user test'
	return process_list[0].exitcode

#===============================================================================
# main function
#===============================================================================
def main():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	config = generic_main()
	
	server_list, ezbox, client_list, vip_list = user_init(config['setup_num'])
	
	init_players(server_list, ezbox, client_list, vip_list, config)
	
	test_rc = run_user_test(server_list, ezbox, client_list, vip_list,config['setup_num'])
	
	log_dir = collect_logs(server_list, ezbox, client_list)

	gen_rc = general_checker(server_list, ezbox, client_list)
	
	clean_players(server_list, ezbox, client_list, True, config['stop_ezbox'])
	
	if test_rc == 0 and gen_rc:
		print 'Test passed !!!'
		exit(0)
	else:
		print 'Test failed !!!'
		exit(1)
	
main()
