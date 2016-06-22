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
# Function: main function
#
# Brief:
#===============================================================================
def main():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	if len(sys.argv) != 2:
		print "script expects exactly 1 input argument"
		print "Usage: client_requests.py <setup_num>"
		exit(1)

	setup_num = int(sys.argv[1]) 	
	vip_list = [get_setup_vip(setup_num,i) for i in range(1)]
	setup_list = get_setup_list(setup_num)
	server_list=[]
	for vm in setup_list:
		server_list.append(HttpServer(ip = vm['ip'],
						  hostname = vm['hostname'], 
						  username = "root", 
						  password = "3tango", 
						  vip = vip_list[0],
						  eth='ens6'))

	for s in server_list:
		print "clean server: " + s.str()
		s.connect()
		s.clean_server()
	
main()
