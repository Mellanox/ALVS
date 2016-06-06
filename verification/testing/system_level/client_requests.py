#!/usr/bin/env python


#===============================================================================
# imports
#===============================================================================

# system  
import os
import sys

# pythons modules 
import cmd
import logging
from collections import namedtuple

# local
import os,sys,inspect
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir) 

from e2e_infra import *

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
	vip = "10.157.7.244"
	ServerStruct = namedtuple("ServerStruct", "ip password virtual_ip index_str host_name exe_path exe_script exec_params")
	server_1     = ServerStruct(ip      = "10.157.7.195",
							password    = "3tango",
							virtual_ip  = vip,
							index_str   = "10.157.7.195",
							host_name   = "l-nps-001",
							exe_path    = None,
							exe_script  = None,
							exec_params = None)
	server_2     = ServerStruct(ip      = "10.157.7.196",
							password    = "3tango",
							virtual_ip  = vip,
							index_str   = "10.157.7.196",
							host_name   = "l-nps-002",
							exe_path    = None,
							exe_script  = None,
							exec_params = None)
	server_list  = {server_1, server_2}
	
	# create Client list
	ClientStruct = namedtuple("ClientStruct", "ip password exe_path exe_script exec_params")
	client_1     = ClientStruct(ip      = "10.157.7.196",
							password    = "3tango",
							exe_path    = "/.autodirect/swgwork/kobis/workspace/GIT2/ALVS/verification/MARS/LoadBalancer/",
							exe_script  = "ClientWrapper.py",
							exec_params = "-i 10.157.7.244 -r 10 --s1 5 --s2 5 --s3 0 --s4 0 --s5 0 --s6 0")
	client_list  = {client_1}

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
