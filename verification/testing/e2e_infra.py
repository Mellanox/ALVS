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

from multiprocessing import Process

	
#===============================================================================
# Function: generic_main
#
# Brief:	Generic main function for tests
#===============================================================================
def generic_main():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	usage = "usage: %prog [-s, -m, -c, -i, -f, -b, -d, --start, --stop, --remote_control]"
	parser = OptionParser(usage=usage, version="%prog 1.0")
	
	bool_choices = ['true', 'false', 'True', 'False']
	parser.add_option("-s", "--setup_num", dest="setup_num",
					  help="Setup number. range (1..7)					(Mandatory parameter)", type="int")
	parser.add_option("-m", "--modify_run_cpus", dest="modify_run_cpus", choices=bool_choices,
					  help="modify run CPUs configuration before running the test. 		(default=True)")
	parser.add_option("-c", "--use_4k_cpus", dest="use_4k_cpus", choices=bool_choices,
					  help="true = use 4k cpu. false = use 512 cpus (default=False).  in case modify_run_cpus is false, use_4k_cpus ignored. (default=False)")
	parser.add_option("-i", "--install_package", dest="install_package", choices=bool_choices,
					  help="Use ALVS package file (alvs_<version>.deb")
	parser.add_option("-f", "--install_file", dest="install_file",
					  help="installation file name (default=alvs_<version>_amd64.deb)")
	parser.add_option("-b", "--copy_binaries", dest="copy_binaries", choices=bool_choices,
					  help="Copy binaries instead of using package installation (default=True). in case install_package is true, copy_binaries ignored")
	parser.add_option("-d", "--use_director", dest="use_director", choices=bool_choices,
					  help="Use director at host 								(default=True)")
	parser.add_option("--start", "--start_ezbox", dest="start_ezbox", choices=bool_choices,
					  help="Start the alvs service at the beginning of the test (defualt=False)")
	parser.add_option("--stop", "--stop_ezbox", dest="stop_ezbox", choices=bool_choices,
					  help="Stop the alvs service at the end of the test 		(defualt=False)")
	parser.add_option("--remote_control", "--remote_control", dest="remote_control", choices=bool_choices,
					  help="Run in remote control mode					 		(defualt=False)")
	(options, args) = parser.parse_args()

	# validate options
	if not options.setup_num:
		raise ValueError('setup_num is not given')
	if (options.setup_num == 0) or (options.setup_num > 8):
		raise ValueError('setup_num: ' + str(options.setup_num) + ' is not in range')

	# set user configuration
	config = {}
	if options.modify_run_cpus:
		config['modify_run_cpus'] = bool_str_to_bool(options.modify_run_cpus)
	if options.use_4k_cpus:
		config['use_4k_cpus']     = bool_str_to_bool(options.use_4k_cpus)
	if options.install_package:
		config['install_package'] = bool_str_to_bool(options.install_package)
	if options.install_file:
		config['install_file']    = options.install_file
	if options.copy_binaries:
		config['copy_binaries']   = bool_str_to_bool(options.copy_binaries)
	if options.use_director:
		config['use_director']    = bool_str_to_bool(options.use_director)
	if options.start_ezbox:
		config['start_ezbox']    = bool_str_to_bool(options.start_ezbox)
	if options.stop_ezbox:
		config['stop_ezbox']    = bool_str_to_bool(options.stop_ezbox)
	if options.remote_control:
		config['remote_control']    = bool_str_to_bool(options.remote_control)

	config['setup_num'] = options.setup_num
	
	print config
	
	return config

#===============================================================================
# init functions
#===============================================================================

#===============================================================================
# Function: generic_init
#
# Brief:	generic init for tests
#
# returns: dictinary with:
#             'next_vm_idx'
#             'vip_list'
#             'setup_list'
#             'server_list'
#             'client_list'
#             'ezbox'
#===============================================================================
def generic_init(setup_num, service_count, server_count, client_count):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	next_vm_idx = 0
	vip_list    = [get_setup_vip(setup_num,i) for i in range(service_count)]
	setup_list  = get_setup_list(setup_num)

	# Create servers list
	server_list=[]
	for i in range(server_count):
		server = HttpServer(ip       = setup_list[next_vm_idx]['ip'],
						    hostname = setup_list[next_vm_idx]['hostname'])
		server_list.append(server)
		next_vm_idx+=1
	
	# Create clients list
	client_list=[]
	for i in range(client_count):
		client = HttpClient(ip       = setup_list[next_vm_idx]['ip'],
						    hostname = setup_list[next_vm_idx]['hostname'])
		client_list.append(client)
		next_vm_idx+=1

	# get EZbox
	ezbox = ezbox_host(setup_num)
	
	
	# conver to dictionary and return it
	dict={}
	dict['next_vm_idx'] = next_vm_idx
	dict['vip_list']    = vip_list
	dict['setup_list']  = setup_list
	dict['server_list'] = server_list
	dict['client_list'] = client_list 
	dict['ezbox']       = ezbox 

	return dict


#------------------------------------------------------------------------------ 
def convert_generic_init_to_user_format(dict):
	return ( dict['server_list'],
			 dict['ezbox'],
			 dict['client_list'],
			 dict['vip_list'] )


#------------------------------------------------------------------------------ 
def init_server(server):
	print "init server: " + server.str()
	server.init_server(server.ip)


#------------------------------------------------------------------------------ 
def init_client(client):
	print "init client: " + client.str()
	client.init_client()


#------------------------------------------------------------------------------
def init_ezbox(ezbox, server_list, vip_list, test_config={}):
	print "init EZbox: " + ezbox.setup['name']
	# start ALVS daemon and DP
	ezbox.connect()
	if test_config['start_ezbox']:
		if test_config['install_package']:
			ezbox.copy_and_install_alvs(test_config['install_file'])
		else:
			if test_config['copy_binaries']:
				ezbox.alvs_service_stop()
				ezbox.copy_binaries('bin/alvs_daemon','bin/alvs_dp')
		
		if test_config['stats']:
			stats = '--statistics'
		else:
			stats = ''
		
		if test_config['remote_control']:
			remote_control = '--remote_control'
		else:
			remote_control = ''

		ezbox.update_cp_params("--run_cpus 16-511 --agt_enabled %s %s --port_type=%s "%(stats, remote_control, ezbox.setup['nps_port_type']))
		
		if test_config['modify_run_cpus']:
			# validate chip is up
			ezbox.alvs_service_start()
			ezbox.update_dp_cpus(test_config['use_4k_cpus'])
			if test_config['use_4k_cpus']:
				ezbox.update_cp_params("--run_cpus 16-4095 --agt_enabled %s %s --port_type=%s "%(stats, remote_control, ezbox.setup['nps_port_type']))
 			else:
 				ezbox.update_cp_params("--run_cpus 16-511 --agt_enabled %s %s --port_type=%s "%(stats, remote_control, ezbox.setup['nps_port_type']))
 				
		ezbox.alvs_service_stop()
		ezbox.config_vips(vip_list)
		ezbox.flush_ipvs()
		ezbox.alvs_service_start()
		# wait for CP before initialize director
		ezbox.wait_for_cp_app()
		# wait for DP app
		ezbox.wait_for_dp_app()	
		time.sleep(6)
	else:
		ezbox.config_vips(vip_list)
		ezbox.flush_ipvs()
		
	# init director
	if test_config['use_director']:
		print 'Start Director'
		services = dict((vip, []) for vip in vip_list )
		for server in server_list:
			services[server.vip].append((server.ip, server.weight))
		ezbox.init_keepalived(services)
		#wait for director	
		time.sleep(15)
		#flush director configurations
		ezbox.flush_ipvs()
	ezbox.logout()

#------------------------------------------------------------------------------
def fill_default_config(test_config):
	
	# define default configuration 
	default_config = {'setup_num'       : None,  # supply by user
					  'modify_run_cpus' : True,  # in case modify_run_cpus is false, use_4k_cpus ignored
					  'use_4k_cpus'     : False,
					  'install_package' : True,  # in case install_package is true, copy_binaries ignored
					  'install_file'    : None,
					  'copy_binaries'   : True,
					  'use_director'    : True,
					  'stats'           : False,
					  'start_ezbox'		: False,
					  'stop_ezbox'		: False,
					  'remote_control'	: False}
	
	# Check user configuration
	for key in test_config:
		if key not in default_config:
			print "WARNING: key %s supply by test_config is not supported" %key
			
	# Add missing configuration
	for key in default_config:
		if key not in test_config:
			test_config[key] = default_config[key]
	
	# Print configuration
	print "configuration test_config:"
	for key in test_config:
		print "     " + '{0: <16}'.format(key) + ": " + str(test_config[key])
		

	return test_config


#------------------------------------------------------------------------------
def init_players(test_resources, test_config={}):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	# init ezbox in another proccess
	ezbox_init_proccess = Process(target=init_ezbox,
								  args=(test_resources['ezbox'], test_resources['server_list'], test_resources['vip_list'], test_config))
	ezbox_init_proccess.start()
	
	# connect Ezbox (proccess work on ezbox copy and not on ezbox object
	test_resources['ezbox'].connect()
	
	for s in test_resources['server_list']:
		init_server(s)
	for c in test_resources['client_list']:
		init_client(c)
	
	# Wait for EZbox proccess to finish
	ezbox_init_proccess.join()
	# Wait for all proccess to finish
	if ezbox_init_proccess.exitcode:
		print "ezbox_init_proccess failed. exit code " + str(ezbox_init_proccess.exitcode)
		exit(ezbox_init_proccess.exitcode)

#===============================================================================
# clean functions
#===============================================================================
#------------------------------------------------------------------------------ 
def clean_server(server):
	print "clean server: " + server.str()
	
	# reconnect with new object
	server.connect()
	server.clean_server()


#------------------------------------------------------------------------------ 
def clean_client(client):
	print "clean client: " + client.str()

	# reconnect with new object
	client.connect()
	client.clean_client()
	


#------------------------------------------------------------------------------ 
def clean_ezbox(ezbox, use_director, stop_ezbox):
	print "Clean EZbox: " + ezbox.setup['name']
	
	# reconnect with new object
	ezbox.connect()
	ezbox.clean(use_director, stop_ezbox)


#------------------------------------------------------------------------------ 
def clean_players(test_resources, use_director = False, stop_ezbox = False):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	# Add servers, client & EZbox to proccess list
	# in adition, recreate ssh object for the new proccess (patch)
	process_list = []
	for s in test_resources['server_list']:
		if s.ssh.connection_established:
			s.ssh.recreate_ssh_object()
			process_list.append(Process(target=clean_server, args=(s,)))
		
	for c in test_resources['client_list']:
		if c.ssh.connection_established:
			c.ssh.recreate_ssh_object()
			process_list.append(Process(target=clean_client, args=(c,)))
			
	if test_resources['ezbox']:
		test_resources['ezbox'].ssh_object.recreate_ssh_object()
		test_resources['ezbox'].run_app_ssh.recreate_ssh_object()
		test_resources['ezbox'].syslog_ssh.recreate_ssh_object()
		process_list.append(Process(target=clean_ezbox, args=(test_resources['ezbox'], use_director, stop_ezbox,)))
	
	# run clean for all player parallely
	for p in process_list:
		p.start()
		
	# Wait for all proccess to finish
	for p in process_list:
		p.join()
		
	for p in process_list:
		if p.exitcode:
			print p, p.exitcode
			exit(p.exitcode)


#===============================================================================
# Run functions (servers and clients)
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
	
	

def collect_logs(test_resources, setup_num = None):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	current_time = strftime("%Y-%m-%d_%H-%M-%S", gmtime())
	
	if not os.path.exists("logs"):
		os.makedirs("logs")
		
	dir_name = 'logs/test_logs_'
	
	if (setup_num != None):
		dir_name += 'setup%s_' %setup_num
	dir_name += current_time
	cmd = "mkdir -p %s" %dir_name
	os.system(cmd)
	for c in test_resources['client_list']:
		c.get_log(dir_name)

	return dir_name

def default_general_checker_expected(expected):
	default_expected = {'host_stat_clean'     : True,
						'syslog_clean'        : True,
						'no_debug'            : True,
						'no_open_connections' : True,
						'no_error_stats'      : True}

	# Check user configuration
	for key in expected:
		if key not in default_expected:
			print "WARNING: key %s supply by user is not supported" %key
			
	# Add missing configuration
	for key in default_expected:
		if key not in expected:
			expected[key] = default_expected[key]
	
	return expected

'''	
	general_checker: This checker should run in all tests before clean_players
'''
def general_checker(test_resources, expected={}):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	expected = default_general_checker_expected(expected)
	
	host_rc = True
	syslog_rc = True
	stats_rc = True
	if 'host_stat_clean' in expected:
		rc = host_stats_checker(test_resources['ezbox'])
		if expected['host_stat_clean'] == False:
			host_rc = (True if rc == False else False)
		else:
			host_rc = rc
	if host_rc == False:
		print "Error: host statistic failed. expected host_stat_clean = " + str(expected['host_stat_clean'])
		
	if 'syslog_clean' in expected:
		#no_debug = expected.get('no_debug', True)
		no_debug = False	# Allow debug in syslog
		rc = syslog_checker(test_resources['ezbox'], no_debug)
		if expected['syslog_clean'] == False:
			syslog_rc = (True if rc == False else False)
		else:
			syslog_rc = rc
	if syslog_rc == False:
		print "Error: syslog checker failed . expected syslog_clean = " + str(expected['syslog_clean'])
	
	if 'no_open_connections' in expected or 'no_error_stats' in expected:
		stats_rc = statistics_checker(test_resources['ezbox'], no_errors=expected.get('no_error_stats', False), no_connections=expected.get('no_open_connections', False))
	if stats_rc == False:
		print "Error: Statistic checker failed."

		
	return (host_rc and syslog_rc and stats_rc)

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
						print 'Note: statistics for service %s:%s are not zero: %s' %(vip,port, ' '.join(stats))
						break
			if '->' == split_line[0]:
				ip, port = split_line[1].split(':')
				stats = split_line[2:]
				for stat in stats:
					if stat != '0':
						rc = False
						print 'Note: statistics for server %s:%s are not zero: %s' %(ip,port, ' '.join(stats))
						break
	else:
		print 'Error - running "ipvsadm -Ln --stats" on host failed '
	return rc

'''
	SYSlog_checker: checks SYSlog for errors.
'''
def syslog_checker(ezbox, no_debug):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	syslog_dict = {}
	rc=[True for i in range(8)]
	syslog_dict['daemon error'] = ezbox.execute_command_on_host('cat /var/log/syslog | grep alvs_daemon | grep "<err"')
	syslog_dict['daemon crtic'] = ezbox.execute_command_on_host('cat /var/log/syslog | grep alvs_daemon | grep "<crit>"')
	syslog_dict['daemon emerg'] = ezbox.execute_command_on_host('cat /var/log/syslog | grep alvs_daemon | grep "<emerg>"')
	syslog_dict['dp error'] = ezbox.execute_command_on_host('cat /var/log/syslog | grep alvs_dp | grep "<err"')
	syslog_dict['dp crtic'] = ezbox.execute_command_on_host('cat /var/log/syslog | grep alvs_dp | grep "<crit>"')
	syslog_dict['dp emerg'] = ezbox.execute_command_on_host('cat /var/log/syslog | grep alvs_dp | grep "<emerg>"')
	if no_debug:
		syslog_dict['daemon debug'] = ezbox.execute_command_on_host('cat /var/log/syslog | grep alvs_daemon | grep "<debug>"')
		syslog_dict['dp debug'] = ezbox.execute_command_on_host('cat /var/log/syslog | grep alvs_dp | grep "<debug>"')
	
	
	ret = True
	
	for key, value in syslog_dict.items():
		if value[0] == True and value[1] != None:
			print '%s found in syslog:' %key
			print value[1]
			ret = False
		if value[0] == False and value[1] == None:
			print 'commamd failed'
			ret = False
		 
	return ret

'''
	statistics_checker: checkes statistics counters for errors and open connections
'''
def statistics_checker(ezbox, no_errors=True, no_connections=True):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	connection_rc = True
	error_rc = True
	if no_connections:
		server_index_list = ezbox.get_all_active_servers()
		for server_index in server_index_list:
			stats_dict = ezbox.get_servers_stats(server_index)
			#active servers + not active servers = total servers
			if(stats_dict['SERVER_STATS_CONNECTION_TOTAL'] != stats_dict['SERVER_STATS_ACTIVE_CONN'] + stats_dict['SERVER_STATS_INACTIVE_CONN']):
				print 'ERROR: Bad connection fo server %d\n. Active connection count = %d. Inactive connection count = %d. Sched connection count = %d'\
				%(server_index, stats_dict['SERVER_STATS_ACTIVE_CONN'], stats_dict['SERVER_STATS_INACTIVE_CONN'], stats_dict['SERVER_STATS_CONNECTION_TOTAL'])
				connection_rc = False
				
	if no_errors:
		error_stats = ezbox.get_error_stats()
		for error_name, count in error_stats.items():
			if error_name not in ['ALVS_ERROR_SERVICE_CLASS_LOOKUP', 'ALVS_ERROR_DEST_SERVER_IS_NOT_AVAIL', 'ALVS_ERROR_CONN_MARK_TO_DELETE']:
				if count > 0:
					print 'ERROR: %s errors found. count = %d' %(error_name, count)
					error_rc = False
	
	return (connection_rc and error_rc)

'''
	client_checker: Supports up to 100 steps (0-99)
	checkers:
		client_count 	= how many clients were in the test
		no_404			= no 404 error is expected (true/false)
		server_count_per_client	= how many servers are expected per client
		client_response_count	= how many responses are expected per client
		expected_servers		= what servers are expected in test
		expected_servers_per_client	= what servers are expected per client 
		check_distribution			= check distribution is according to weights
		no_connection_closed		= no connection closed is expected (true/false)
'''
def client_checker(log_dir, expected={}, step_count = 1):
	
	rc = True
	
	if len(expected) == 0:
		return rc
	
	file_list = [log_dir+'/'+f for f in listdir(log_dir) if isfile(join(log_dir, f))]
	all_responses = dict((i,{}) for i in range(step_count))
	all_vip_responses = dict((i,{}) for i in range(step_count))
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
				server = split_line[1].strip()
				vip = split_line[0].strip()
				client_responses[server] = client_responses.get(server, 0) + 1
				if vip not in all_vip_responses[step].keys():
					all_vip_responses[step][vip] = {}
				all_vip_responses[step][vip][server] = all_vip_responses[step][vip].get(server, 0) + 1
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
				expected_servers_list  = [s.ip for s in expected_dict['expected_servers']]
								
				for ip, count in client_responses.items():
					if ip not in expected_servers_list and ip not in ['Connection closed ERROR','404 ERROR']:
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
			
			if 'check_distribution' in expected_dict:
				servers = expected_dict['check_distribution'][0]
				services = expected_dict['check_distribution'][1]
				sd_percent = expected_dict['check_distribution'][2]
				
				for vip in all_vip_responses[step].keys():
					print "check distribution for service: %s" %vip
					servers_per_service = [s for s in servers if s.vip == vip]
					total_per_service = 0
					for server in all_vip_responses[step][vip].keys():
						total_per_service += all_vip_responses[step][vip][server]
						
					sd = total_per_service*sd_percent
					print 'standard deviation is currently %.02f percent' %(sd_percent)
					
					totalWeights = 0.0
					for s in servers_per_service:
						if "rr" in expected_dict['check_distribution']:
							totalWeights += 1
						else:
							totalWeights += s.weight
					
					for s in servers_per_service:
						if "rr" in expected_dict['check_distribution']:
							w = 1
						else:
							w = s.weight
						responses_num_without_sd = (total_per_service*w)/totalWeights
						if s.ip not in all_vip_responses[step][vip].keys():
							actual_responses = 0
						else:
							actual_responses = all_vip_responses[step][vip][s.ip]
						diff = abs(actual_responses - responses_num_without_sd)
						if diff > sd:
							print 'ERROR: client should received: %d responses from server: %s with standard deviation: %d. Actual number of responses was: %d' %(responses_num_without_sd,s.ip,sd,actual_responses)
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

def pcap_checker(results, expected):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	rc = True
	for step,result in enumerate(results):
		print "checking step %d:" %step
		if 'count_pcap_checker' in result:
			if result['count_pcap_checker'] !=  expected[step]['count_pcap_checker']:
				print 'ERROR: the result for count_pcap_checker is %s, and was expected %s' %(result['count_pcap_checker'],expected[step]['count_pcap_checker'])
				rc = False
			else:
				print "pcap count OK"
		if 'compare_stats' in result:
			if result['compare_stats'] <  expected[step]['compare_stats']:
				print 'ERROR: the result for compare_stats is %s, and was expected %s' %(result['compare_stats'],expected[step]['compare_stats'])
				rc = False
			else:
				print "Statistics OK"
		if 'compare_packets_count' in result:
			#multiple comparisons
			comp_count = 0
			for (curr_packets_count, curr_expected_packets_count) in zip((result['compare_packets_count'] ,expected[step]['compare_packets_count'])):
				#packets count per server
				for server in range(len(curr_packets_count)):
					if curr_packets_count[server] < curr_expected_packets_count[server]:
						print 'ERROR: in comparison number %d for server %d:\n The num of packets was captured is %d\n And num of packets was expected to catch is: %d ' %(comp_count, server, curr_packets_count[server], curr_expected_packets_count[server])
						rc = False
						
				comp_count +=1
	return rc
				
