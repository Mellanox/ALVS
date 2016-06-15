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
# ipvsadm commands
#===============================================================================
def add_service(ezbox, vip, port, sched_alg='sh', sched_alg_opt='-b sh-port'):
	ezbox.execute_command_on_host("ipvsadm -A -t %s:%s -s %s %s"%(vip,port, sched_alg, sched_alg_opt))

def modify_service(ezbox, vip, port, sched_alg='sh', sched_alg_opt=' -b sh-port'):
	ezbox.execute_command_on_host("ipvsadm -E -t %s:%s -s %s %s"%(vip,port, sched_alg, sched_alg_opt))

def delete_service(ezbox, vip, port):
	ezbox.execute_command_on_host("ipvsadm -D -t %s:%s "%(vip,port))
	
def add_server(ezbox, vip, service_port, server_ip, server_port, weight=1, routing_alg_opt=' '):
	ezbox.execute_command_on_host("ipvsadm -a -t %s:%s -r %s:%s -w %d %s"%(vip, service_port, server_ip, server_port, weight, routing_alg_opt))

def modify_server(ezbox, vip, service_port, server_ip, server_port, weight=1, routing_alg_opt=' '):
	ezbox.execute_command_on_host("ipvsadm -e -t %s:%s -r %s:%s -w %d %s"%(vip, service_port, server_ip, server_port, weight, routing_alg_opt))

def delete_server(ezbox, vip, service_port, server_ip, server_port, weight=1, routing_alg_opt=' '):
	ezbox.execute_command_on_host("ipvsadm -d -t %s:%s -r %s:%s" %(vip, service_port, server_ip, server_port))

def flush_ipvs(ezbox):
	ezbox.execute_command_on_host("ipvsadm -C")
	
#===============================================================================
# init functions
#===============================================================================

#------------------------------------------------------------------------------ 
def init_players(server_list, ezbox, client_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	# init HTTP servers 
	for s in server_list:
		print "init server: " + s.str()
		s.init_server(s.ip)

	# init ALVS daemon
	ezbox.connect()
	ezbox.terminate_cp_app()
	ezbox.reset_chip()
	flush_ipvs(ezbox)
	ezbox.copy_cp_bin_to_host()
	ezbox.run_cp_app()
	ezbox.copy_and_run_dp_bin()
	
	# init client
	for c in client_list:
		print "init client: " + c.str()
		c.init_client()
	
	
#===============================================================================
# clean functions
#===============================================================================
#------------------------------------------------------------------------------ 
def clean_players(server_list, ezbox, client_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	# init HTTP servers 
	for s in server_list:
		print "clean server: " + s.str()
		s.clean_server()


	# init client
	for c in client_list:
		print "clean client: " + c.str()
		c.clean_client()

	ezbox.clean()

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
 	dir_name = 'test_logs'
 	cmd = "mkdir -p %s" %dir_name
	os.system(cmd)
	for c in client_list:
		c.get_log(dir_name)


