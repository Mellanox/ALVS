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
	
	# Simple usage:
	# player1: HTTP server 
	# player2: HTTP server
	# player3: HTTP client
	# player4: EZbox 
	
	server_count = 2
	client_count = 1
	service_count = 1
	
	dict = generic_init(setup_num, service_count, server_count, client_count)
	
	for s in dict['server_list']:
		s.vip = dict['vip_list'][0]
		
	return convert_generic_init_to_user_format(dict)

def client_execution(client, vip):
	repeat = 10 
	client.exec_params += " -i %s -r %d " %(vip, repeat)
	client.execute()

def run_user_test(server_list, ezbox, client_list, vip_list):
	print 'Running user test'
	process_list = []
	vip = vip_list[0]

#	ezbox.add_service(vip)
	
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

	init_players(server_list, ezbox, client_list, vip_list)
	
	run_user_test(server_list, ezbox, client_list, vip_list)
	#run_test(server_list, client_list)
	
	collect_logs(server_list, ezbox, client_list)
	
	clean_players(server_list, ezbox, client_list, config['stop_ezbox'])
	



main()
