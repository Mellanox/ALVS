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
from os import listdir
from os.path import isfile, join

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

def client_checker(log_dir, expected_dict={}):
	if len(expected_dict) == 0:
		return True
	
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
	
	
	if 'client_count' in expected_dict:
		if len(responses) != expected_dict['client_count']:
			print 'ERROR: wrong number of logs. log count = %d, client count = %d' %(len(responses),expected_dict['client_count'])
			return False
		
	
	for index,client_responses in enumerate(responses):
		print 'testing client %d ...' %(index+1)
		if 'no_404' in expected_dict:
			if expected_dict['no_404'] == True:
				if '404 ERROR' in client_responses:
					print 'ERROR: client received 404 response. count = %d' % client_responses['404 ERROR']
					return False
		if 'server_count_per_client' in expected_dict:
			if len(client_responses) != expected_dict['server_count_per_client']:
				print 'ERROR: client received responses from different number of expected servers. expected = %d , received = %d' %(expected_dict['server_count_per_client'], len(client_responses))
				return False
		total = 0
		for ip, count in client_responses.items():
			print 'response count from server %s = %d' %(ip,count)
			total += count
		if 'client_response_count' in expected_dict:
			if total != expected_dict['client_response_count']:
				print 'ERROR: client received wrong number of responses. expected = %d , received = %d' %(expected_dict['client_response_count'], total)
				return False
	
	return True		

