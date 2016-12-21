import sys
import os
import inspect
#sys.path.append(my_parentdir)

import random

from common_infra import *
from optparse import OptionParser
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
	
	def copy_and_install_ddp(self, ddp_package=None):
		if ddp_package is None:
			# get package name
			version = self.get_version()	#???????
			ddp_package = "ddp_%s_amd64.deb" %(version) # ??????
		#ddp_23.0000.0000_amd64.deb 
		self.copy_package(ddp_package)
		self.install_package(ddp_package)
	
	def ddp_service(self, cmd):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called for " + cmd + " service"
		self.run_app_ssh.execute_command("service ddp " + cmd)

	def ddp_service_start(self):
		self.run_app_ssh.execute_command("cat /dev/null > /var/log/syslog")
		self.syslog_ssh.wait_for_msgs(['file truncated'])
		self.ddp_service("start")

	def ddp_service_stop(self):
		self.ddp_service("stop")

	def ddp_service_restart(self):
		self.ddp_service("restart")
	
	
	def clean(self):
		self.logout()


class Remote_Host(player):
	def __init__(self, ip, hostname, 
				username    = "root",
				password    = "3tango",
				exe_path    = "/tmp/DDP/",
				exe_script  = "bin/synca",
				exec_params = "&"):
		# init parent class basic_player
		super(remote_host, self).__init__(ip, hostname, username, password, exe_path, exe_script, exec_params)
	
	def start_sync_agent(self):
		self.copy_file_to_player(self.exe_script, self.exe_path)
		self.execute()
	
	# stop application:
	def stop_bin(self):
		func_name = sys._getframe().f_code.co_name
		#get PID of process
		pid = self.execute_command("pgrep " + exe_script)
		
		if pid:
			rc = self.execute_command("kill -SIGTERM " + pid)
			if rc is True:
				err_msg = "ERROR: %s Failed to stop sync process " %(func_name)
				raise RuntimeError(err_msg)
	
	
	def clean_host(self):
		self.logout()

class Host(player):
	def __init__(self, hostname, all_eths,
				ip          = None,
				username    = "root",
				password    = "3tango",
				exe_path    = "/temp/host/",
				exe_script  = None,
				exec_params = None,
				mode        = None):
		# init parent class
		super(Host, self).__init__(ip, hostname, username, password, mode, all_eths, exe_path, exe_script, exec_params)
		# Init class variables
		self.pcap_files = []
		self.mac_adress = None
	
	def init_host(self):
		self.connect()
		self.config_interface()
		self.mac_adress=get_mac_adress()
		self.clear_arp_table()
		#self.capture_packets()
		
	def clean_host(self):
		self.logout()

	def send_packet(self, pcap_file):
		logging.log(logging.DEBUG,"Send packet to NPS") 
		cmd = "mkdir -p %s" %self.exe_path
		self.execute_command(cmd)
		self.copy_file_to_player(pcap_file, self.exe_path)
		pcap_file_name = pcap_file[pcap_file.rfind("/")+1:]
		cmd = "tcpreplay --intf1="+self.eth +" " + self.exe_path + "/" + pcap_file_name		#############
		logging.log(logging.DEBUG,"run command on client:\n" + cmd) #todo
		result, output = self.execute_command(cmd)
		if result == False:
			print "Error while sending a packet to NPS"
			print output
			exit(1)
		cmd = "rm -rf " + self.exe_path
		self.execute_command(cmd)

	##send n packets to specific ip and mac address
	def send_n_packets(self,dest_ip,dest_mac,num_of_packets):
		packet_list_to_send = []
		src_port_list = []
		for i in range (num_of_packets):
		# set the packet size	  
			packet_size = 150
	
			random_source_port = '%02x %02x' %(random.randint(1,1255),random.randint(1,255))
			
			data_packet = tcp_packet(mac_da          = dest_mac,
			                         mac_sa          = self.mac_address,
			                         ip_dst          = ip_to_hex_display(dest_ip),
			                         ip_src          = ip_to_hex_display(self.ip),
			                         tcp_source_port = random_source_port,
			                         tcp_dst_port    = '00 50', # port 80
			                         packet_length   = packet_size)
			
			data_packet.generate_packet()
			print data_packet.packet
			packet_list_to_send.append(data_packet.packet)
			    
		#	src_port_list.append(int(random_source_port.replace(' ',''),16))    not sure why need this? 
		pcap_to_send = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name=dir + 'verification/testing/dp/temp_packet_%s.pcap'%self.ip)
		self.pcap_files.append(pcap_to_send)
		self.send_packet(pcap_to_send)
	
	def capture_packets(self, tcpdump_params=''):
		self.dump_pcap_file = '/tmp/dump.pcap'
		self.ssh.execute_command("rm -f " + self.dump_pcap_file)
		cmd = 'pkill -HUP -f tcpdump; tcpdump -w ' + self.dump_pcap_file + ' -n -i '+self.eth+' ether host ' + self.mac_address + ' and dst ' + self.ip + ' &'
		logging.log(logging.DEBUG,"running on host command:\n"+cmd)
		self.ssh.ssh_object.sendline(cmd)
		self.ssh.ssh_object.prompt()
		
	def stop_capture(self):
		self.ssh_object.execute_command('pkill -HUP -f tcpdump')
		time.sleep(1)
		result, output = self.ssh_object.execute_command('tcpdump -r /tmp/dump.pcap | wc -l')
		if result == False:
			err_msg = 'ERROR, reading captured num of packets was failed'
			raise RuntimeError(err_msg)
		try:
			num_of_packets = int(output.split('\n')[1].strip('\r'))
		except:
			err_msg = 'ERROR, reading captured num of packets was failed'
			raise RuntimeError(err_msg)

		return num_of_packets
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
	if (options.setup_num == 0) or (options.setup_num > 8):
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
		

	config['setup_num'] = options.setup_num
	
	return config


def generic_init(setup_num, host_count):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	next_vm_idx = 0
	setup_list  = get_setup_list(setup_num)
	remote_host_info = get_remote_host(setup_num)
	
	remote_host = Remote_Host(ip       = remote_host_info['ip'],
							  hostname = remote_host_info['hostname'])
	# Create host list
	host_list=[]
	for i in range(host_count):
		host = Host(ip       = setup_list[next_vm_idx]['all_ips'],
					hostname = setup_list[next_vm_idx]['hostname'],
					all_eths = setup_list[next_vm_idx]['all_eths'])
		host_list.append(host)
		next_vm_idx+=1
	
	ezbox = DDP_ezbox(setup_num, remote_host)
	
	# conver to dictionary and return it
	dict={}
	dict['next_vm_idx'] = next_vm_idx
	dict['setup_list']  = setup_list
	dict['host_list']   = host_list 
	dict['remote_host'] = remote_host
	dict['ezbox']       = ezbox 

	return dict

def init_host(host):
	print "init host: " + host.str()
	host.init_host()
	
def init_players(test_resources, test_config={}):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
# 	if test_config['start_ezbox']:
# 		change_switch_lag_mode(test_config['setup_num'], enable_lag = False)
	
	# init ezbox in another proccess
	ezbox_init_proccess = Process(target=init_ezbox,
								  args=(test_resources['ezbox'], test_config))
	ezbox_init_proccess.start()
	
	# connect Ezbox (proccess work on ezbox copy and not on ezbox object
	test_resources['ezbox'].connect()
	
	for h in test_resources['host_list']:
		init_host(h)
	
	# Wait for EZbox proccess to finish
	ezbox_init_proccess.join()
	# Wait for all proccess to finish
	if ezbox_init_proccess.exitcode:
		print "ezbox_init_proccess failed. exit code " + str(ezbox_init_proccess.exitcode)
		exit(ezbox_init_proccess.exitcode)
		
def clean_host(host):
	print "clean host: " + host.str()
	# reconnect with new object
	host.connect()
	host.clean_host()
	
#------------------------------------------------------------------------------ 
def clean_ezbox(ezbox, stop_ezbox):
	print "Clean EZbox: " + ezbox.setup['name']
	# reconnect with new object
	ezbox.connect()
	ezbox.clean(stop_ezbox)

def init_ezbox(ezbox, test_config={}):
	print "init EZbox: " + ezbox.setup['name']
	# start DDP daemon and DP
	if test_config['start_ezbox']:
		ezbox.remote_host.start_sync_agent()
		ezbox.connect()
		cpus_range = "16-" + str(int(test_config['num_of_cpus']) - 1)
		if test_config['install_package']:
			ezbox.copy_and_install_ddp(test_config['install_file'])
		if test_config['stats']:
			stats = '--statistics'
		else:
			stats = ''

		if test_config['modify_run_cpus']:
	 		ezbox.modify_run_cpus(cpus_range)
		params = "--run_cpus %s --agt_enabled %s --port_type=%s " % (cpus_range, stats, ezbox.setup['nps_port_type'])
		print "CP params = " + params
		ezbox.update_cp_params(params)
		# wait for CP before initialize director
		ezbox.wait_for_cp_app()
		# wait for DP app
		ezbox.wait_for_dp_app()	
		time.sleep(6)
		
		ezbox.logout()

#------------------------------------------------------------------------------
def fill_default_config(test_config):
	
	# define default configuration 
	default_config = {'setup_num'       : None,  # supply by user
					  'modify_run_cpus' : True,  # in case modify_run_cpus is false, use_4k_cpus ignored
					  'use_4k_cpus'     : False,
					  'install_package' : True,  # in case install_package is true, copy_binaries ignored
					  'install_file'    : None,
					  'stats'           : False,
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

# TODO
def clean_players(test_resources, use_director = False, stop_ezbox = False):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
		# recreate ssh object for the new proccess (patch)
		process_list = []
		remote_host = test_resources['remote_host']
		
		if not remote_host.ssh.connection_established:
			remote_host.ssh.connect()
		remote_host.ssh.recreate_ssh_object()
		process_list.append(Process(target=clean_host, args=(remote_host,)))
		
		for host in test_resources['host_list']:
			if not host.ssh.connection_established:
				host.ssh.connect()
			host.ssh.recreate_ssh_object()
			process_list.append(Process(target=clean_host, args=(host,)))
				
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



