#!/usr/bin/env python


#===============================================================================
# imports
#===============================================================================

# system  
import cmd
from collections import namedtuple
import logging
import os
import os, sys, inspect
import sys

from e2e_infra import *


# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir) 


#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

def user_init():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	# Simple usage:
	# player: 10.157.7.195 (l-nps-001) - HTTP server 
	# player: 10.157.7.196 (l-nps-002) - Client
	# player: 10.157.7.197 (l-nps-003) - LVS
	# player: 10.157.7.198 (l-nps-004) - HTTP server 
	
	# create HTTP server list
	
	script_dirname = os.path.dirname(os.path.realpath(__file__))
	vip = "10.157.7.244"
	exec_script_dir = script_dirname + "/../../MARS/LoadBalancer/"
	server_1 = HttpServer(ip = "10.157.7.195",
						  hostname = "l-nps-001", 
						  username = "root", 
						  password = "3tango", 
						  exe_path = None, exe_script = None, exec_params = None, 
						  vip = vip)
	server_2 = HttpServer(ip = "10.157.7.196",
						  hostname = "l-nps-002", 
						  username = "root", 
						  password = "3tango", 
						  exe_path = None, exe_script = None, exec_params = None, 
						  vip = vip)
	
	server_list  = [server_1, server_2]
	
	# create Client list
	client_1 = HttpClient(ip = "10.157.7.196",
						  hostname = "l-nps-003", 
						  username = "root", 
						  password = "3tango", 
						  exe_path    = exec_script_dir,
						  exe_script  = "ClientWrapper.py",
						  exec_params = "-i 10.157.7.244 -r 10 --s1 5 --s2 5 --s3 0 --s4 0 --s5 0 --s6 0")
	client_list  = [client_1]

	# EZbox list
	EZboxStruct = namedtuple("EZbox", "host_ip dp_ip")  # TODO: add more fields
	ezbox       = EZboxStruct(host_ip = "192.168.0.1", dp_ip = "192.168.0.1")  # TODO: handle

	return (server_list, ezbox, client_list)


#===============================================================================
# main function
#===============================================================================
def main():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	server_list, ezbox, client_list = user_init()

	init_players(server_list, ezbox, client_list)
	
	run_test(server_list, client_list)
	
	collect_logs(server_list, ezbox, client_list)
	
	clean_players(server_list, ezbox, client_list)
	



main()
