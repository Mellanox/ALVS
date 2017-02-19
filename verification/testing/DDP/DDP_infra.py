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
from test_failed_exception import *
from synca_commands import *
#===============================================================================
# Classes
#===============================================================================

class DDP_ezbox(ezbox_host):
	
	def __init__(self, setup_id, remote_host):
		# init parent class
		super(DDP_ezbox, self).__init__(setup_id,"ddp")
		
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
		print 'admin state is: ' , interface['admin_state']
		if interface['admin_state']==0:
			raise TestFailedException ("ERROR eth %s is not up" %eth_up_index)
		print "eth %s is up" %eth_up_index
			
	def check_if_eth_is_down(self, eth_down_index):
		interface = self.get_interface(eth_down_index)
		print 'admin state is: ' , interface['admin_state']
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
		self.synca_ssh = SshConnect(hostname, username, password)
		self.synca_commands = synca_commands(self.synca_ssh) 
		
	def start_sync_agent(self,ezbox_mng_ip):
		func_name = sys._getframe().f_code.co_name
		print "FUNCTION %s called " %(func_name)
		self.prepare_env()
		self.run_synca(ezbox_mng_ip)
		
	def prepare_env(self):
		self.connect()
		self.kill_synca_process()
		self.remove_exe_dir()
		self.create_exe_dir()
		self.copy_file_to_player(self.exe_script, self.exe_path)
		self.copy_file_to_player(self.fp_so, self.lib_path)
		self.ssh.recreate_ssh_object()
		
	def run_synca(self, ezbox_mng_ip):
		print "connecting to synca ssh"
		self.synca_ssh.connect()
		all_eths_str = ','.join(self.all_eths)
		cmd = 'cd /root/DDP_install; ./synca --ip %s --port 1235 --if %s --cli' %(ezbox_mng_ip, all_eths_str)
		self.synca_commands.execute_synca_command(cmd)
		
	def kill_synca_process(self):
		self.execute_command('pkill -SIGINT synca')

	def capture_ping_form_host(self):
		tcpdump_params = ' tcpdump -w ' + self.dump_pcap_file + ' -i ' + self.eth + ' net 192.168.0.0/16 and icmp &'
		self.capture_packets(tcpdump_params)
	
	def clean_remote_host(self):
		self.logout()
