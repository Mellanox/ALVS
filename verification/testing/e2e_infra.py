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
import re
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
def init_players(server_list, ezbox, client_list, vip_list, use_director = False):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	# init HTTP servers 
	for s in server_list:
		print "init server: " + s.str()
		s.init_server(s.ip)

	# start ALVS daemon and DP
	ezbox.connect()
 	ezbox.reset_chip()
	ezbox.config_vips(vip_list)
 	ezbox.terminate_cp_app()
	ezbox.flush_ipvs()
 	ezbox.copy_binaries('bin/alvs_daemon','bin/alvs_dp')
 	ezbox.run_cp()
 	ezbox.run_dp(args='--run_cpus 16-127')
 	ezbox.wait_for_cp_app()

	if use_director:
		services = dict((vip, []) for vip in vip_list )
		for server in server_list:
			services[server.vip].append((server.ip, server.weight))
		ezbox.init_director(services)
	ezbox.flush_ipvs()

	
	
	# init client
	for c in client_list:
		print "init client: " + c.str()
		c.init_client()
	

	
#===============================================================================
# clean functions
#===============================================================================
#------------------------------------------------------------------------------ 
def clean_players(server_list, ezbox, client_list, use_director = False):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	# init HTTP servers 
	for s in server_list:
		print "clean server: " + s.str()
		s.clean_server()


	# init client
	for c in client_list:
		print "clean client: " + c.str()
		c.clean_client()

	ezbox.clean(use_director)

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

'''
	general_checker: This checker should run in all tests before clean_players
'''
def general_checker(server_list, ezbox, client_list, expected={'host_stat_clean':True}):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	rc = True
	if 'host_stat_clean' in expected:
		rc = host_stats_checker(ezbox)
		if expected['host_stat_clean'] == False:
			rc = (not rc)
		
	return rc

'''
	host_stats_checker: checkes all ipvs statistics on host are zero.
'''
def host_stats_checker(ezbox):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	rc, ret_val = ezbox.execute_command_on_host('ipvsadm -Ln --stats')
	if rc == True:
		ret_val = re.sub(' +',' ',ret_val)
		ret_list = ret_val.split('\n')
		for line in ret_list:
			split_line = line.split(' ')
			split_line=[s.strip() for s in split_line]
			if 'TCP' == split_line[0]:
				vip, port = split_line[1].split(':')
				stats = split_line[2:]
				for stat in stats:
					if stat != '0':
						rc = False
						print 'Error - statistics for service %s:%s are not zero: %s' %(vip,port, ' '.join(stats))
						break
			if '->' == split_line[0]:
				ip, port = split_line[1].split(':')
				stats = split_line[2:]
				for stat in stats:
					if stat != '0':
						rc = False
						print 'Error - statistics for server %s:%s are not zero: %s' %(ip,port, ' '.join(stats))
						break
	else:
		print 'Error - running "ipvsadm -Ln --stats" on host failed '
	return rc


'''
	client_checker: Supports up to 100 steps (0-99)
'''
def client_checker(log_dir, expected={}, step_count = 1):
	rc = True
	
	if len(expected) == 0:
		return rc
	
	file_list = [log_dir+'/'+f for f in listdir(log_dir) if isfile(join(log_dir, f))]
	all_responses = dict((i,{}) for i in range(step_count))
	for filename in file_list:
		if filename[-1].isdigit():
			step = int(filename[-1])
			if filename[-2].isdigit():
				step += int(filename[-2]) * 10
		else: 
			step = 0
		client_ip = filename[filename.find('client_')+7 : filename.find('.log')]
		client_responses = {}
		logfile=open(filename, 'r')
		for line in logfile:
			if len(line) > 2 and line[0] != '#':
				split_line = line.split(':')
				key = split_line[1].strip()
				client_responses[key] = client_responses.get(key, 0) + 1
		all_responses[step][client_ip] = client_responses	 
	
	for step, responses in all_responses.items():
		if step_count > 1:
			expected_dict = expected[step]
			print 'step %d :'%step
		else:
			expected_dict = expected
		if 'client_count' in expected_dict:
			if len(responses) != expected_dict['client_count']:
				print 'ERROR: wrong number of logs. log count = %d, client count = %d' %(len(responses),expected_dict['client_count'])
				rc = False
		
		connection_closed = False
		#expected_servers
		for client_ip,client_responses in responses.items():
			print 'testing client %s ...' %client_ip
			total = 0
			for ip, count in client_responses.items():
				print 'response count from server %s = %d' %(ip,count)
				total += count

			if 'Connection closed ERROR' in client_responses:
				connection_closed = True
			
			if 'no_404' in expected_dict:
				if expected_dict['no_404'] == True:
					if '404 ERROR' in client_responses:
						print 'ERROR: client received 404 response. count = %d' % client_responses['404 ERROR']
						rc = False
				else:
					if '404 ERROR' not in client_responses:
						print 'ERROR: client did not receive 404 response. '
						rc = False
					
			if 'server_count_per_client' in expected_dict:
				if len(client_responses) != expected_dict['server_count_per_client']:
					print 'ERROR: client received responses from different number of expected servers. expected = %d , received = %d' %(expected_dict['server_count_per_client'], len(client_responses))
					rc = False
			if 'client_response_count' in expected_dict:
				if total != expected_dict['client_response_count']:
					print 'ERROR: client received wrong number of responses. expected = %d , received = %d' %(expected_dict['client_response_count'], total)
					rc = False
			if 'expected_servers' in expected_dict:
				expected_servers_failed = False
				expected_servers = expected_dict['expected_servers']
				
				expected_servers_list  = [s.ip for s in expected_dict['expected_servers']]
								
				for ip, count in client_responses.items():
					if ip not in expected_servers_list:
						print 'ERROR: client received response from unexpected server. server ip = %s ' %(ip)
						expected_servers_failed = True
						rc = False
				
				if (expected_servers_failed):
					expected_servers_str = ""
					for ip in expected_servers_list:
						expected_servers_str += ip + " "
					print "list of expected servers list: "  + expected_servers_str

			if 'expected_servers_per_client' in expected_dict:
				expected_servers = expected_dict['expected_servers_per_client'][client_ip]
				for ip, count in client_responses.items():
					if ip not in expected_servers:
						print 'ERROR: client received response from unexpected server. server ip = %s , list of expected servers: %s' %(ip, expected_servers)
						rc =  False
 
		if 'no_connection_closed' in expected_dict:
			if expected_dict['no_connection_closed'] == True and connection_closed == True:
				print 'ERROR: client received connection closed Error.'
				rc = False
			else:
				if expected_dict['no_connection_closed'] == False and connection_closed == False:
					print 'ERROR: client did not receive connection closed Error. '
					rc = False
	return rc		

