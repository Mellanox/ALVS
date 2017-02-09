import sys
import os
import inspect
import time
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir)
from optparse import OptionParser
from multiprocessing import Process
from common_infra import *

class Players_Factory(object):
	
	def __init__(self):
		pass
	
	def clean_ezbox(ezbox, stop_ezbox):
		print "Clean EZbox: " + ezbox.setup['name']
		# reconnect with new object
		ezbox.connect()
		ezbox.clean(stop_ezbox)
	
	clean_ezbox = staticmethod(clean_ezbox)

	def generic_main():
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		usage = "usage: %prog [-s, -m, -c, -i, -f, -b, -d, -n, --start, --stop, --remote_control]"
		parser = OptionParser(usage=usage, version="%prog 1.0")
		
		bool_choices = ['true', 'false', 'True', 'False']
		cpus_choices=['32','64','128','256','512','1024','2048','4096']
		parser.add_option("-s", "--setup_num", dest="setup_num",
						  help="Setup number. range (1..9)					(Mandatory parameter)", type="int")
		parser.add_option("-m", "--modify_run_cpus", dest="modify_run_cpus", choices=bool_choices,
						  help="modify run CPUs configuration before running the test. 		(default=True)")
		parser.add_option("-i", "--install_package", dest="install_package", choices=bool_choices,
						  help="Use ALVS/ATC/DDP package file (<alvs/atc/ddp>_<version>.deb")
		parser.add_option("-f", "--install_file", dest="install_file",
						  help="installation file name (default=alvs_<version>_amd64.deb)")
		parser.add_option("--start", "--start_ezbox", dest="start_ezbox", choices=bool_choices,
						  help="Start the alvs service at the beginning of the test (defualt=False)")
		parser.add_option("--stop", "--stop_ezbox", dest="stop_ezbox", choices=bool_choices,
						  help="Stop the alvs service at the end of the test 		(defualt=False)")
		parser.add_option("--remote_control", "--remote_control", dest="remote_control", choices=bool_choices,
						  help="Run in remote control mode					 		(defualt=False)")
		parser.add_option("-c", "--number_of_cpus", dest="num_of_cpus",choices=cpus_choices,
						  help="Number of CPUs. power of 2 between 32-4096	(Mandatory parameter)")
		parser.add_option("-d", "--use_director", dest="use_director", choices=bool_choices,
						  help="Use director at host 								(default=True)")
		(options, args) = parser.parse_args()
	
		# validate options
		if not options.setup_num:
			raise ValueError('setup_num is not given')
		if (options.setup_num == 0) or (options.setup_num > 9):
			raise ValueError('setup_num: ' + str(options.setup_num) + ' is not in range')
	
		# set user configuration
		config = {}
		if options.modify_run_cpus:
			config['modify_run_cpus'] = bool_str_to_bool(options.modify_run_cpus)
		if options.install_package:
			config['install_package'] = bool_str_to_bool(options.install_package)
		if options.install_file:
			config['install_file']    = options.install_file
		if options.start_ezbox:
			config['start_ezbox']    = bool_str_to_bool(options.start_ezbox)
		if options.stop_ezbox:
			config['stop_ezbox']    = bool_str_to_bool(options.stop_ezbox)
		if options.remote_control:
			config['remote_control']    = bool_str_to_bool(options.remote_control)
		if options.num_of_cpus:
			config['num_of_cpus'] = options.num_of_cpus
		if options.use_director:
			config['use_director']    = bool_str_to_bool(options.use_director)
	
		config['setup_num'] = options.setup_num
		
		return config
	
	generic_main = staticmethod(generic_main)
		
		
	def init_players(test_resources, test_config={}):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		# init ezbox in another proccess
		if test_config['start_ezbox']:
			ezbox_init_proccess = Process(target=test_resources['ezbox'].common_ezbox_bringup,
									  args=(test_config,))
			ezbox_init_proccess.start()
			
			#change_switch_lag_mode(test_config['setup_num'], enable_lag = False)
		
		# connect Ezbox (proccess work on ezbox copy and not on ezbox object
		test_resources['ezbox'].connect()
		
		
		# Wait for EZbox proccess to finish
		if test_config['start_ezbox']:
			ezbox_init_proccess.join()
		# Wait for all proccess to finish
			if ezbox_init_proccess.exitcode:
				print "ezbox_init_proccess failed. exit code " + str(ezbox_init_proccess.exitcode)
				exit(ezbox_init_proccess.exitcode)
			
		time.sleep(6)
	
	init_players = staticmethod(init_players)
	
	def fill_default_config(test_config,default_config={}):
		
		# define default configuration 
		if not default_config:
			default_config = {'setup_num'		: None,  # supply by user
							  'modify_run_cpus'	: True,  # in case modify_run_cpus is false, use_4k_cpus ignored
							  'use_4k_cpus'		: False,
							  'install_package'	: True,  # in case install_package is true, copy_binaries ignored
							  'install_file'	: None,
							  'stats'			: False,
							  'start_ezbox'		: False,
							  'stop_ezbox'		: False,
							  'remote_control'	: False,
							  'num_of_cpus'		: "512" }# 
		
		# Check user configuration
		for key in test_config:
			if key not in default_config:
				print "WARNING: key %s supply by test_config is not copy_and_install_packagesupported" %key
				
		# Add missing configuration
		for key in default_config:
			if key not in test_config:
				test_config[key] = default_config[key]
		
		# Print configuration
		print "configuration test_config:"
		for key in test_config:
			print "     " + '{0: <16}'.format(key) + ": " + str(test_config[key])
	
		return test_config
	
	fill_default_config = staticmethod(fill_default_config)
	
	def clean_players(test_resources, use_director = False, stop_ezbox = False, process_list=[]):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		if test_resources['ezbox']:
			test_resources['ezbox'].ssh_object.recreate_ssh_object()
			test_resources['ezbox'].run_app_ssh.recreate_ssh_object()
			test_resources['ezbox'].syslog_ssh.recreate_ssh_object()
			process_list.append(Process(target=Players_Factory.clean_ezbox, args=(test_resources['ezbox'], stop_ezbox,)))
		
		start_and_wait_for_processes(process_list)
	
	clean_players = staticmethod(clean_players)
		
					
	
	def start_and_wait_for_processes(process_list):
		# run all proccesses
		for p in process_list:
			p.start()
			
		# Wait for all proccess to finish
		for p in process_list:
			p.join()
			
		for p in process_list:
			if p.exitcode:
				print p, p.exitcode
				exit(p.exitcode)	
	
	start_and_wait_for_processes = staticmethod(start_and_wait_for_processes)