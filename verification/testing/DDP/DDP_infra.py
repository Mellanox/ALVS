import sys
import os
import inspect
#sys.path.append(my_parentdir)

import random

my_currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe() )))
my_parentdir = os.path.dirname(my_currentdir)
sys.path.append(my_parentdir)

from common_infra import *
from optparse import OptionParser
from multiprocessing import Process
import cmd
from cmd import Cmd
dir = os.path.dirname(parentdir)
#===============================================================================
# Classes
#===============================================================================

class DDP_ezbox(ezbox_host):
	
	def __init__(self, setup_id, remote_host):
		# init parent class
		super(DDP_ezbox, self).__init__(setup_id)
		
		self.remote_host = remote_host
		self.install_path = "DDP_install"
	
	def execute_network_command(self,cmd):
		return self.remote_host.execute_command(cmd)
	
	def common_ezbox_bringup(self, test_config={}):
		super(DDP_ezbox, self).common_ezbox_bringup(test_config)
		self.remote_host.start_sync_agent(self.setup['mng_ip'])
	
	def copy_and_install_package(self, ddp_package=None):
		if ddp_package is None:
			# get package name
			ddp_package = "ddp_*_amd64.deb"
		self.copy_package(ddp_package)
		self.install_package(ddp_package)
	
	def ddp_service(self, cmd):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called for " + cmd + " service"
		self.run_app_ssh.execute_command("service ddp " + cmd)

	def service_start(self):
		self.run_app_ssh.execute_command("cat /dev/null > /var/log/syslog")
		self.syslog_ssh.wait_for_msgs(['file truncated'])
		self.ddp_service("start")

	def service_stop(self):
		self.ddp_service("stop")

	def service_restart(self):
		self.ddp_service("restart")
	
	
	def clean(self,stop_ezbox):
		if stop_ezbox:
			self.service_stop()
		self.logout()

	
	def check_if_eth_is_up(self, eth_up_index):
		interface = self.get_interface(eth_up_index)
		if interface['admin_state']==0:
			raise TestFailedException ("ERROR eth %s is not up" %eth_up_index)
		print "eth %s is up" %eth_up_index
			
		
	def check_if_eth_is_down(self, eth_down_index):
		interface = self.get_interface(eth_down_index)
		if interface['admin_state']==1:
			raise TestFailedException ("ERROR eth %s is not down" %eth_down_index)
		print "eth %s is down" %eth_down_index
	
	
class Remote_Host(player):
	def __init__(self, ip, hostname, all_eths,
				username        = "root",
				password        = "3tango",
				exe_path        = "DDP_install",
				exe_script      = "bin/synca",
				dump_pcap_file  = '/tmp/server_dump.pcap',
				exec_params     = "&"):
		# init parent class basic_player
		
		super(Remote_Host, self).__init__(ip, hostname, username, password, None, all_eths,
										 exe_path, exe_script, exec_params, dump_pcap_file)
		self.lib_path = '/usr/lib/'
		self.fp_so = 'bin/libfp.so'
		self.dump_pcap_file = '/tmp/remote_host_dump.pcap'
		
	def start_sync_agent(self,ezbox_mng_ip):
		func_name = sys._getframe().f_code.co_name
		print "FUNCTION %s called " %(func_name)
		self.connect()
		
		self.kill_synca_process()
		self.remove_exe_dir()
		self.create_exe_dir()

		self.copy_file_to_player(self.exe_script, self.exe_path)
		self.copy_file_to_player(self.fp_so, self.lib_path)
		
		all_eths_str = ','.join(self.all_eths)
		self.exec_params='--ip %s --port 1235 --if %s > /dev/null 2>&1 &' %(ezbox_mng_ip, all_eths_str)
		self.execute('./synca','')
		self.logout()
	
	def kill_synca_process(self):
		self.execute_command('pkill -SIGINT synca')

	def capture_ping_form_host(self):
		tcpdump_params = ' tcpdump -w ' + self.dump_pcap_file + ' -i ' + self.eth + ' net 192.168.0.0/16 and icmp &'
		self.capture_packets(tcpdump_params)
	
	def clean_remote_host(self):
		self.logout()


#===============================================================================
# Function: generic_main
#
# Brief:	Generic main function for tests
#===============================================================================
def generic_main():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	usage = "usage: %prog [-s, -m, -c, -i, -f, -b, -d, -n, --start, --stop, --remote_control]"
	parser = OptionParser(usage=usage, version="%prog 1.0")
	
	bool_choices = ['true', 'false', 'True', 'False']
	cpus_choices=['32','64','128','256','512','1024','2048','4096']
	parser.add_option("-s", "--setup_num", dest="setup_num",
					  help="Setup number. range (1..8)					(Mandatory parameter)", type="int")
	parser.add_option("-m", "--modify_run_cpus", dest="modify_run_cpus", choices=bool_choices,
					  help="modify run CPUs configuration before running the test. 		(default=True)")
	parser.add_option("-i", "--install_package", dest="install_package", choices=bool_choices,
					  help="Use DDP package file ")
	parser.add_option("-f", "--install_file", dest="install_file",
					  help="installation file name (default=ddp_<version>_amd64.deb)")
	parser.add_option("--start", "--start_ezbox", dest="start_ezbox", choices=bool_choices,
					  help="Start the ddp service at the beginning of the test (defualt=False)")
	parser.add_option("--stop", "--stop_ezbox", dest="stop_ezbox", choices=bool_choices,
					  help="Stop the ddp service at the end of the test 		(defualt=False)")
	parser.add_option("--remote_control", "--remote_control", dest="remote_control", choices=bool_choices,
					  help="Run in remote control mode					 		(defualt=False)")
	parser.add_option("-c", "--number_of_cpus", dest="num_of_cpus",choices=cpus_choices,
					  help="Number of CPUs. power of 2 between 32-4096	(Mandatory parameter)")
	(options, args) = parser.parse_args()

	# validate options
	if not options.setup_num:
		raise ValueError('setup_num is not given')
	if (options.setup_num == 0) or (options.setup_num > 9):
		raise ValueError('setup_num: ' + str(options.setup_num) + ' is not in range')

	# set user configuration
	config = {}
	if options.modify_run_cpus:
		config['modify_run_cpus']	= bool_str_to_bool(options.modify_run_cpus)
	if options.install_package:
		config['install_package']	= bool_str_to_bool(options.install_package)
	if options.install_file:
		config['install_file']		= options.install_file
	if options.start_ezbox:
		config['start_ezbox']		= bool_str_to_bool(options.start_ezbox)
	if options.stop_ezbox:
		config['stop_ezbox']		= bool_str_to_bool(options.stop_ezbox)
	if options.remote_control:
		config['remote_control']	= bool_str_to_bool(options.remote_control)
	if options.num_of_cpus:
		config['num_of_cpus']		= options.num_of_cpus
		

	config['setup_num'] = options.setup_num
	
	return config

#------------------------------------------------------------------------------
def generic_init_ddp(setup_num, host_count):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	next_vm_idx = 0
	setup_list  = get_setup_list(setup_num)
	remote_host_info = get_remote_host(setup_num)
	
	
	print "remote host ip :" +remote_host_info['hostname']
	
	remote_host = Remote_Host(ip 		= remote_host_info['all_ips'],
							  hostname 	= remote_host_info['hostname'],
							  all_eths	= remote_host_info['all_eths'])
	
	# Create host list
	host_list=[]
	for i in range(host_count):
		host = Host(ip 			= setup_list[next_vm_idx]['all_ips'],
					hostname 	= setup_list[next_vm_idx]['hostname'],
					all_eths 	= setup_list[next_vm_idx]['all_eths'])
		host_list.append(host)
		next_vm_idx+=1
	
	ezbox = DDP_ezbox(setup_num, remote_host)
	
	
	
	# conver to dictionary and return it
	dict={}
	dict['next_vm_idx']	= next_vm_idx
	dict['setup_list']	= setup_list
	dict['host_list']	= host_list 
	dict['remote_host']	= remote_host
	dict['ezbox']		= ezbox 

	return dict
#------------------------------------------------------------------------------
def init_host(host):
	print "init host: " , host.str()
	host.init_host()
#------------------------------------------------------------------------------
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
	test_resources['remote_host'].connect()
	
	for h in test_resources['host_list']:
		init_host(h)
	
	# Wait for EZbox proccess to finish
	if test_config['start_ezbox']:
		ezbox_init_proccess.join()
	# Wait for all proccess to finish
		if ezbox_init_proccess.exitcode:
			print "ezbox_init_proccess failed. exit code " + str(ezbox_init_proccess.exitcode)
			exit(ezbox_init_proccess.exitcode)
		
	time.sleep(6)

#------------------------------------------------------------------------------
def clean_host(host):
	print "clean host: " ,  host.str()
	# reconnect with new object
	host.connect()
	host.clean_host()
#------------------------------------------------------------------------------
def clean_remote_host(remote_host):
	print "clean remote host host: " ,  remote_host.str()
	# reconnect with new object
	remote_host.connect()
	remote_host.clean_remote_host()
#------------------------------------------------------------------------------
def clean_ezbox(ezbox, stop_ezbox):
	print "Clean EZbox: " + ezbox.setup['name']
	# reconnect with new object
	ezbox.connect()
	ezbox.clean(stop_ezbox)

#------------------------------------------------------------------------------
def fill_default_config(test_config):
	
	# define default configuration 
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
# TODO
def clean_players(test_resources, use_director = False, stop_ezbox = False):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
		# recreate ssh object for the new proccess (patch)
		process_list = []
		remote_host = test_resources['remote_host']
		
		if not remote_host.ssh.connection_established:
			remote_host.ssh.connect()
		remote_host.ssh.recreate_ssh_object()
		process_list.append(Process(target=clean_remote_host, args=(remote_host,)))
		
		for host in test_resources['host_list']:
			if not host.ssh.connection_established:
				host.ssh.connect()
			host.ssh.recreate_ssh_object()
			process_list.append(Process(target=clean_host, args=(host,)))
				
		if test_resources['ezbox']:
			test_resources['ezbox'].ssh_object.recreate_ssh_object()
			test_resources['ezbox'].run_app_ssh.recreate_ssh_object()
			test_resources['ezbox'].syslog_ssh.recreate_ssh_object()
			process_list.append(Process(target=clean_ezbox, args=(test_resources['ezbox'], stop_ezbox,)))
		
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



