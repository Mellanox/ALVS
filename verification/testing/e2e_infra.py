#!/usr/bin/env python
#===============================================================================
# imports
#===============================================================================
# system  
import cmd
from collections import namedtuple
import logging
from optparse import OptionParser
import os
import sys

from common_infra import *
from server_infra import *
from client_infra import *
from test_infra import *

#===============================================================================
# init functions
#===============================================================================

#------------------------------------------------------------------------------ 
def init_ALVS_daemon(host_ip):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	ezbox = ezbox_host(hostname=host_ip, username='root', password='ezchip')
	ezbox.connect()
	ezbox.terminate_cp_app()
	ezbox.run_cp_app()


#------------------------------------------------------------------------------ 	
def init_ALVS_DP(ip):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	ezbox = ezbox_host(hostname=host_ip, username='root', password='ezchip')
	ezbox.connect()
	ezbox.reset_chip()
	ezbox.copy_and_run_dp_bin()

#------------------------------------------------------------------------------ 
def init_players(server_list, ezbox, client_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	# init HTTP servers 
	for s in server_list:
		print "init server: " + s.str()
		s.init_server(s.ip)

	# init ALVS daemon
	init_ALVS_daemon(ezbox.host_ip)
	
	# init ALVS DP
	init_ALVS_DP(ezbox.dp_ip)
	
	# init client
	for c in client_list:
		print "init client: " + c.str()
		c.init_client()
	
	

#===============================================================================
# clean functions
#===============================================================================

#------------------------------------------------------------------------------ 
def clean_ALVS_daemon(host_ip):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	ezbox = ezbox_host(hostname=host_ip, username='root', password='ezchip')
	ezbox.connect()
	ezbox.terminate_cp_app()
	

#------------------------------------------------------------------------------ 
def clean_ALVS_DP(ip):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	ezbox = ezbox_host(hostname=host_ip, username='root', password='ezchip')
	ezbox.connect()
	ezbox.reset_chip()

#------------------------------------------------------------------------------ 
def clean_players(server_list, ezbox, client_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	# init HTTP servers 
	for s in server_list:
		print "init server: " + s.str()
		s.clean_server()

	# init ALVS daemon
	clean_ALVS_daemon(ezbox.host_ip)
	
	# init ALVS DP
	clean_ALVS_DP(ezbox.dp_ip)
	
	# init client
	for c in client_list:
		print "init client: " + c.str()
		c.clean_client()


#===============================================================================
# Run functions
#===============================================================================
def run_test(server_list, client_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	# Execute HTTP servers scripts 
	for s in server_list:
		print "running server: " + s.str()
		s.execute()

	# Excecure client
	for c in client_list:
		print "running client: " + c.str()
		c.execute()
	
	

def collect_logs(server_list, ezbox, client_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	print "TODO: implement " + sys._getframe().f_code.co_name
	

#===============================================================================
# User Area
#===============================================================================

def user_init_sample():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	# Simple usage:
	# player: 10.157.7.195 (l-nps-001) - HTTP server 
	# player: 10.157.7.196 (l-nps-002) - Client
	# player: 10.157.7.197 (l-nps-003) - LVS
	# player: 10.157.7.198 (l-nps-004) - HTTP server 
	
	# create HTTP server list
	vip = "10.157.7.244"
	server_1 = HttpServer(ip = "10.157.7.195",
						  hostname = "l-nps-001", 
						  username = "root", 
						  password = "3tango", 
						  exe_path = "", exe_script = "", exec_params = "", 
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
						  exe_path    = "/.autodirect/swgwork/kobis/workspace/GIT2/ALVS/verification/MARS/LoadBalancer/",
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
# this function should be implemented by user

def main_sample():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	server_list, ezbox, client_list = user_init_sample()

	init_players(server_list, ezbox, client_list)
	
	run_test(server_list, client_list)
	
	collect_logs(server_list, ezbox, client_list)
	
	clean_players(server_list, ezbox, client_list)
	


#main_sample()
