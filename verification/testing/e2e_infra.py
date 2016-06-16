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
from time import gmtime, strftime

from common_infra import *
from server_infra import *
from client_infra import *
from test_infra import *

	
#===============================================================================
# init functions
#===============================================================================

#------------------------------------------------------------------------------ 
def init_players(server_list, ezbox, client_list, vip_list):
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
	ezbox.config_vips(vip_list)

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
	current_time = strftime("%Y-%m-%d_%H-%M-%S", gmtime())
 	dir_name = 'test_logs_%s' %current_time
 	cmd = "mkdir -p %s" %dir_name
	os.system(cmd)
	for c in client_list:
		c.get_log(dir_name)

	return dir_name
