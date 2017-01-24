#!/usr/bin/env python

#===============================================================================
# imports
#===============================================================================
# system  
import cmd
import logging
import os
import sys
import inspect
import time
import struct
import socket
import pprint
import struct

from pexpect import pxssh
import pexpect
from ftplib import FTP
#from subprocess import check_output

# pythons modules 
from cmd import Cmd
from network_interface_enum import *
cp_dir = os.path.dirname(os.path.abspath(__file__)) + "/../../EZdk/tools/EZcpPyLib/lib"
sys.path.append(cp_dir)
from ezpy_cp import EZpyCP

parentdir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ALVSdir = os.path.dirname(parentdir)
#===============================================================================
# Struct IDs
#===============================================================================
STRUCT_ID_NW_INTERFACES						= 0
STRUCT_ID_NW_LAG_GROUPS						= 1 
STRUCT_ID_NW_FIB							= 2
STRUCT_ID_NW_ARP							= 3
STRUCT_ID_APPLICATION_INFO					= 11

#===============================================================================
# STATS DEFINES
#===============================================================================
NW_NUM_OF_IF_STATS			 = 20

#===============================================================================
# Classes
#===============================================================================

class arp_entry:
     
    def __init__(self, ip_address, mac_address, interface=None, flags=None, mask=None, type=None):
        self.ip_address = ip_address
        self.mac_address = mac_address
        self.interface = interface
        self.flags = flags
        self.mask = mask
        self.type = type

class ezbox_host(object):
	
	def __init__(self, setup_id):

		self.setup = get_ezbox_names(setup_id)
		self.ssh_object = SshConnect(self.setup['host'], self.setup['username'], self.setup['password'])
		self.run_app_ssh = SshConnect(self.setup['host'], self.setup['username'], self.setup['password'])
		
		self.syslog_ssh = SshConnect(self.setup['host'], self.setup['username'], self.setup['password'])
		self.cpe = EZpyCP(self.setup['host'], 1234)

	def common_ezbox_bringup(self, test_config={}):
		print "init EZbox: " + self.setup['name']
		# start daemon and DP
		self.connect()
		cpus_range = "16-" + str(int(test_config['num_of_cpus']) - 1)
		if test_config['install_package']:
			self.copy_and_install_package(test_config['install_file'])
		
		if test_config['stats']:
			stats = '--statistics'
		else:
			stats = ''
			
		if test_config['modify_run_cpus']:
			self.service_start()
			self.modify_run_cpus(cpus_range)
			self.wait_for_dp_app()
		
		self.update_debug_params("--run_cpus %s --agt_enabled %s " % (cpus_range, stats))
		self.update_port_type("--port_type=%s " % (self.setup['nps_port_type']))
		self.service_stop()
		self.service_start()
		self.wait_for_dp_app()


	def reset_ezbox(self):
		os.system("/.autodirect/LIT/SCRIPTS/rreboot " + self.setup['name'])
		
	def send_ping(self, ip):
		self.ssh_object.sendline('ping -c1 -w10 '+ ip + ' > /dev/null 2>&1')
		exit_code = self.get_execute_command_exit_code(False)
		if(exit_code!=0):
			raise Exception('commmand failed')

	def execute_command_on_chip(self, chip_cmd):
		return self.ssh_object.execute_chip_command(chip_cmd)
	
	def update_debug_params(self, params=None):
		cmd = "find /etc/default/alvs | xargs grep -l DEBUG | xargs sed -i '/DEBUG=/c\DEBUG=\"%s\"' " %params
		self.execute_command_on_host(cmd)
		
	def update_port_type(self, params=None):
		cmd = "find /etc/default/alvs | xargs grep -l PORT_TYPE | xargs sed -i '/PORT_TYPE=/c\PORT_TYPE=\"%s\"' " %params
		self.execute_command_on_host(cmd)

	def modify_run_cpus(self, cpus_range):
		cpus="0,"+cpus_range
		self.execute_command_on_chip("fw_setenv krn_possible_cpus %s" %cpus)
		self.execute_command_on_chip("fw_setenv krn_present_cpus %s" %cpus)

	def update_dp_cpus(self, cpus_range):
		self.modify_run_cpus(cpus_range)

	def add_arp_static_entry(self, ip_address, mac_address):
		self.execute_network_command("arp -s %s %s"%(ip_address, mac_address))

	def connect(self):
		self.ssh_object.connect()
		self.run_app_ssh.connect()
		self.syslog_ssh.connect()
		self.syslog_ssh.execute_command('tail -f /var/log/syslog | grep anl', False)

	def logout(self):
		self.ssh_object.logout()
		self.run_app_ssh.logout()
		self.syslog_ssh.logout()

	def execute_command_on_host(self, cmd):
		return self.ssh_object.execute_command(cmd)

	def execute_network_command(self,cmd):
		return self.execute_command_on_host(cmd)
	
	def copy_file_to_host(self, filename, dest):
		rc =  os.system("sshpass -p " + self.setup['password'] + " scp " + filename + " " + self.setup['username'] + "@" + self.setup['host'] + ":" + dest)
		if rc:
			err_msg =  "failed to copy %s to %s" %(filename, dest)
			raise RuntimeError(err_msg)

	def copy_package(self, package):
		func_name = sys._getframe().f_code.co_name
		print "FUNCTION %s: copy %s to %s " %(func_name, package, self.install_path)
		
		rc = self.execute_command_on_host("rm -rf %s" %self.install_path)
		if rc is True:
			print "ERROR: %s Failed to remove install folder (%s)" %(func_name, self.install_path)

		rc = self.execute_command_on_host("mkdir -p %s" %self.install_path)
		if rc is True:
			err_msg =  "ERROR: %s Failed to create install folder (%s)" %(func_name, self.install_path)
			raise RuntimeError(err_msg)

		rc = self.copy_file_to_host(package, self.install_path)
		if rc is True:
			err_msg =  "ERROR: %s Failed to copy package" %(func_name)
			raise RuntimeError(err_msg)

	def install_package(self, package):
		func_name = sys._getframe().f_code.co_name
		print "FUNCTION %s: called with %s" %(func_name, package)
		
		self.remove_old_package()
		try:
			cmd = "echo 'N' | DEBIAN_FRONTEND=noninteractive dpkg --force-confold -i %s/%s" %(self.install_path, package)
			self.ssh_object.ssh_object.sendline(cmd)
			time.sleep(15) #TODO what to do with "Reading database..." that messing with expect?
			self.ssh_object.ssh_object.prompt(60)
			
			# check exit code
			self.ssh_object.ssh_object.sendline("echo $?")
			self.ssh_object.ssh_object.prompt(timeout=30)
			exit_code = self.ssh_object.ssh_object.before.split('\n')[1]
			try:
				exit_code = int(exit_code)
			except:
				exit_code = 1
		except:
			err_msg = "Unexpected error: %s" %sys.exc_info()[0]
			raise RuntimeError(err_msg)
			
		if exit_code != 0:
			err_msg =  "ERROR: installation failed (%s)" % exit_code
			raise RuntimeError(err_msg)

	def remove_old_package(self):
		package_list = ['alvs', 'ddp','tc']
		for i in range(3):
			try:
				cmd = "dpkg -r %s" %(package_list[i])
				self.ssh_object.ssh_object.sendline(cmd)
				self.ssh_object.ssh_object.prompt(60)
				time.sleep(15)
				# check exit code
				self.ssh_object.ssh_object.sendline("echo $?")
				self.ssh_object.ssh_object.prompt(30)
				exit_code = self.ssh_object.ssh_object.before.split('\n')[1]
				try:
					exit_code = int(exit_code)
				except:
					exit_code = 1
			except:
				err_msg = "Unexpected error: %s" %sys.exc_info()[0]
				raise RuntimeError(err_msg)
				break
			
		if exit_code != 0:
			err_msg =  "ERROR: removing packge failed (%s)" % exit_code
			raise RuntimeError(err_msg)

	def reset_chip(self):
		self.run_app_ssh.execute_command("bsp_nps_power -r por")
	
	def wait_for_cp_app(self):
		sys.stdout.write("Waiting for CP Application to load...")
		sys.stdout.flush()
		output = self.syslog_ssh.wait_for_msgs(['alvs_db_manager_poll...'])
		print
		
		if output != 0:
			err_msg =  '\nwait_for_cp_app: Error... wait for CP failed'
			raise RuntimeError(err_msg)
		
	def wait_for_dp_app(self):
		sys.stdout.write("Waiting for DP application to load...")
		sys.stdout.flush()
		output = self.syslog_ssh.wait_for_msgs(['Application version:'])
		print

		if output != 0:
			err_msg = "\n" + sys._getframe().f_code.co_name + ": Error... wait for DP failed"
			raise RuntimeError(err_msg)

	def capture_packets(self, tcpdump_params = None):
		if tcpdump_params == None:
			tcpdump_params = 'dst ' + self.setup['host']

		self.ssh_object.execute_command('pkill -HUP -f tcpdump; tcpdump -w /tmp/dump.pcap -i ' + self.setup['interface'] + ' ' + tcpdump_params + ' &')

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

	def swap32(self,i):
		return struct.unpack("<I", struct.pack(">I", i))[0]

	def get_applications_info(self):
		iterator_params_dict = (self.cpe.cp.struct.iterator_create(STRUCT_ID_APPLICATION_INFO, { 'channel_id': 0 })).result['iterator_params']
		num_entries = (self.cpe.cp.struct.get_num_entries(STRUCT_ID_APPLICATION_INFO, channel_id = 0)).result['num_entries']['number_of_entries']

		apps_info = []
		for i in range(0, num_entries):
			iterator_params_dict = self.cpe.cp.struct.iterator_get_next(STRUCT_ID_APPLICATION_INFO, iterator_params_dict).result['iterator_params']
			key = str(iterator_params_dict['entry']['key'])
			lid = int(key, 16)
			result = str(iterator_params_dict['entry']['result']).split(' ')
			
			app_info = {'master_bit' : (int(result[0], 16) >> 1) & 0x1,
						 'backup_bit' : (int(result[0], 16) >> 0) & 0x1,
						 'm_sync_id' : int(''.join(result[2]), 16),
						 'b_sync_id' : int(''.join(result[3]), 16)
						 }
			apps_info.append(app_info)
			
		self.cpe.cp.struct.iterator_delete(STRUCT_ID_APPLICATION_INFO, iterator_params_dict)
		return apps_info

	def get_interface(self, lid):
		res = str(self.cpe.cp.struct.lookup(STRUCT_ID_NW_INTERFACES, 0, {'key' : "%02x" % lid}).result["params"]["entry"]["result"]).split(' ')
		interface = {'admin_state' : (int(res[0], 16) >> 3) & 0x1,
					 'path_type' : (int(res[0], 16) >> 1) & 0x3,
					 'is_direct_output_lag' : int(res[0], 16) & 0x1,
					 'direct_output_if' : int(res[1], 16),
					 'add_bitmap' : int(''.join(res[2:4]), 16),
					 'mac_address' : ':'.join(res[4:10]),
					 'output_channel' : int(res[10], 16),
					 'sft_en' : int(res[11], 16),
					 'stats_base' : int(''.join(res[12:16]), 16),
					 }
		
		return interface

	def get_arp_entry(self, ip):
		res = str(self.cpe.cp.struct.lookup(STRUCT_ID_NW_ARP, 0, {'key' : "%08x" % ip}).result["params"]["entry"]["result"]).split(' ')
		arp_entry = {
					 'is_lag' : int(res[0], 16) & 0x1,
					 'output_index' : int(''.join(res[1]), 16),
					 'mac_address' : ':'.join(res[2:8]),
					 }
		
		return arp_entry
	

	def get_arp_table(self,incomplete=True):
		parsed_arp_entries=[]
		linux_arp= self.execute_command_on_host('arp -n')
		if(linux_arp[0]==False):
			print linux_arp
			exit(1)
		arp_entries = linux_arp[1].split('\n')
		del arp_entries[0]
		for entry in arp_entries:
			if entry == '':
				break
			if incomplete:
				if "incomplete" in entry: #not valid arp entry
					continue
			entry_items=entry.split()
			entry_items[2] = entry_items[2].upper()
			temp_entry = arp_entry(ip_address = entry_items[0], 
								   type = entry_items[1], 
								   mac_address = entry_items[2], 
								   interface = entry_items[-1])
			parsed_arp_entries.append(temp_entry)
		
		return parsed_arp_entries
		

		
       

	
	def compare_arp_tables(self,linux_arp,cp_arp):

		linux_arp_entries_dict={}

		
		##building dictonery for linux table entries
		for x in linux_arp:
			linux_arp_entries_dict[x.ip_address]=x.mac_address
			
		if len(linux_arp_entries_dict) != len(cp_arp):
			print "arp_tables not in the same length:  %d  %d"%(len(linux_arp_entries_dict),len(cp_arp))
			return False
		#calculate dif between dictoneries
		linux_arp_entries_dict_keys=set(linux_arp_entries_dict.keys())
		cp_arp_entries_dict_keys=set(cp_arp.keys())
		intersect_keys = cp_arp_entries_dict_keys.intersection(linux_arp_entries_dict_keys)
		same = set(o for o in intersect_keys if linux_arp_entries_dict[o] == cp_arp[o])
		return len(same)==len(linux_arp_entries_dict_keys)
	
	def get_unused_ip_on_subnet(self,netmask='255.255.255.0',ip=None):
		arp_entries=self.get_arp_table(False)
		if ip == None:#caleld from alvs test so need to use ezbox data ip
			ip=self.setup['data_ip_hex_display']
			bytes = ["".join(x) for x in zip(*[iter(ip.replace(" ", ""))]*2)]
			bytes = [int(x, 16) for x in bytes]
			ip=".".join(str(x) for x in bytes)
		int_ip_address=ip2int(ip)
		int_netmask=ip2int(netmask)
		first_ip = int2ip(int_ip_address & int_netmask)
		temp_ip = first_ip
		new_list = []
		while True:
			temp_ip = add2ip(temp_ip,1)
			if ip == temp_ip:
				continue
			
			found=False
			for entry in arp_entries:
				if entry.ip_address == temp_ip:
					found=True
					continue
			if (ip2int(first_ip) & int_netmask) < (ip2int(temp_ip) & int_netmask):
				break
			
			if found == False:
				new_list.append(temp_ip)
		
		return new_list
	

	def check_cp_entry_exist(self,ip_address,mac_address):
		alvs_arp_entries_dict=self.get_all_arp_entries()
		value = alvs_arp_entries_dict.get(ip_address)
		if value==None or value!=mac_address:
			return False
		else:
			return True
	
	def get_lag_group(self, lag_group_id):
		res = str(self.cpe.cp.struct.lookup(STRUCT_ID_NW_LAG_GROUPS, 0, {'key' : "%02x" % lag_group_id}).result["params"]["entry"]["result"]).split(' ')
		lag_group = {
					 'admin_state' : (int(res[0], 16) >> 3) & 0x1,
					 'members_count' : int(''.join(res[1]), 16),
					 'lag_members' : '|'.join([str(int(x,16)) for x in res[2:10]]),
					 }
		
		return lag_group
	
	def get_all_arp_entries(self):
		iterator_params_dict = (self.cpe.cp.struct.iterator_create(STRUCT_ID_NW_ARP, { 'channel_id': 0 })).result['iterator_params']
		num_entries = (self.cpe.cp.struct.get_num_entries(STRUCT_ID_NW_ARP, channel_id = 0)).result['num_entries']['number_of_entries']

		entries_dictionary = {}
		entries = []
		for i in range(0, num_entries):
			iterator_params_dict = self.cpe.cp.struct.iterator_get_next(STRUCT_ID_NW_ARP, iterator_params_dict).result['iterator_params']
			
			key = str(iterator_params_dict['entry']['key']).split(' ')
			ip = int(''.join(key[0:4]), 16)
			
			result = str(iterator_params_dict['entry']['result']).split(' ')
			entries_dictionary[int2ip(ip)] = ':'.join(result[2:])
#			 entry = {'mac' : ' '.join(result[2:]), #int(result[4:], 16),
#					  'key' : {'ip' : ip}
#					  }
#			 
#			 entries.append(entry)

		self.cpe.cp.struct.iterator_delete(STRUCT_ID_NW_ARP, iterator_params_dict)			 
		return entries_dictionary

	def get_all_interfaces(self):
		iterator_params_dict = (self.cpe.cp.struct.iterator_create(STRUCT_ID_NW_INTERFACES, { 'channel_id': 0 })).result['iterator_params']
		num_entries = (self.cpe.cp.struct.get_num_entries(STRUCT_ID_NW_INTERFACES, channel_id = 0)).result['num_entries']['number_of_entries']

		interfaces = []
		for i in range(0, num_entries):
			iterator_params_dict = self.cpe.cp.struct.iterator_get_next(STRUCT_ID_NW_INTERFACES, iterator_params_dict).result['iterator_params']
			 
			key = str(iterator_params_dict['entry']['key'])
			lid = int(key, 16)
			 
			result = str(iterator_params_dict['entry']['result']).split(' ')
			interface = {'is_direct_output_lag' : (int(result[0], 16) >> 0) & 0x1,
						 'path_type' : (int(result[0], 16) >> 1) & 0x3,
						 'oper_status' : (int(result[0], 16) >> 3) & 0x1,
						 'direct_output_if' : int(''.join(result[1]), 16),
						 'app_bitmap' : int(''.join(result[2:4]), 16),
						 'mac_address' : int(''.join(result[4:10]), 16),
						 'output_channel' : int(''.join(result[10]), 16),
						 'sft_en' : (int(result[11], 16) >> 7) & 0x1,
						 'stats_base' : int(''.join(result[12:16]), 16),
						 'key' : lid
						 }

			interfaces.append(interface)
			
		self.cpe.cp.struct.iterator_delete(STRUCT_ID_NW_INTERFACES, iterator_params_dict)
		return interfaces

	def clean_arp_table(self):
		logging.log(logging.INFO, "clean arp table")
		cmd = 'ip -s -s neigh flush all'
		result,output = self.execute_network_command(cmd)		  # run a command
#		 self.ssh_object.prompt()			   # match the prompt
#		 output = self.ssh_object.before		# print everything before the prompt.
#		 logging.log(logging.DEBUG, output)
		 
		cmd = 'ip -s -s neigh flush nud perm'
		result,output = self.execute_network_command(cmd)		  # run a command

#		 self.ssh_object.prompt()			   # match the prompt
#		 output = self.ssh_object.before		# print everything before the prompt.
#		 logging.log(logging.DEBUG, output)

	def get_interface_stats(self, interface_id):
		if interface_id < 0 or interface_id > NUM_OF_INTERFACES:
			return -1 
		
		counter_offset = IF_STATS_POSTED_OFFSET + interface_id*NW_NUM_OF_IF_STATS
		
		# return only the lsb (small amount of packets on tests)
		interface_stats = self.cpe.cp.stat.get_posted_counters(channel_id=0, 
															   partition=0, 
															   range=True, 
															   start_counter=counter_offset, 
															   num_counters=NW_NUM_OF_IF_STATS, 
															   double_operation=False).result['posted_counter_config']['counters']

		stats_dict = {'NW_IF_STATS_FRAME_VALIDATION_FAIL':interface_stats[0]['byte_value'],
					  'NW_IF_STATS_MAC_ERROR':interface_stats[1]['byte_value'],
					  'NW_IF_STATS_IPV4_ERROR':interface_stats[2]['byte_value'],
					  'NW_IF_STATS_NOT_MY_MAC':interface_stats[3]['byte_value'],
					  'NW_IF_STATS_NOT_IPV4':interface_stats[4]['byte_value'],
					  'NW_IF_STATS_NOT_TCP':interface_stats[5]['byte_value'],
					  'NW_IF_STATS_NO_VALID_ROUTE':interface_stats[6]['byte_value'],
					  'NW_IF_STATS_FAIL_ARP_LOOKUP':interface_stats[7]['byte_value'],
					  'NW_IF_STATS_FAIL_INTERFACE_LOOKUP':interface_stats[8]['byte_value']}
					
		return stats_dict
	
	def add_arp_entry(self, ip_address, mac_address):
		mac_address = str(mac_address).upper()
		cmd = "arp -s " + ip_address + " " + mac_address
		result,output = self.execute_network_command(cmd)
		return [result,output]
	
	def delete_arp_entry(self, ip_address):
		cmd = "arp -d " + ip_address
		result,output = self.execute_network_command(cmd)
		return [result,output]
	
	def change_arp_entry_state(self, ip_address, new_state, check_if_state_changed = False, old_state = None):
		cmd ="ip neigh change " + ip_address + " dev "+self.setup['interface']+" nud " + new_state
		if check_if_state_changed == True:
			cmd = cmd + " && ip neigh show nud " + new_state + " | grep " + ip_address
		result,output = self.execute_network_command(cmd)
		return [result,output]
	
	def verify_arp_state(self, ip_address, entry_state):
		cmd ="ip neigh show nud " + entry_state + " | grep " + ip_address
		result,output = self.execute_network_command(cmd)
		return [result,output]
		
	def add_fib_entry(self, ip, mask, gateway=None, drop=None):
		
		mask = str(mask)
		
		if gateway != None and drop != None:
			return False
		
		# add drop entry 
		if gateway == None and drop == True:
			cmd = 'ip route add blackhole '+ ip + '/' + mask

		# add neighbour entry 
		if gateway == None and drop != True:
			cmd = 'ip route add unicast '+ ip + '/' + mask + ' dev ' + self.setup['interface']
			
		# add gateway entry 
		if gateway != None and drop != True:
			cmd = 'ip route add unicast '+ ip + '/' + mask + ' via ' + gateway
			
		result,output = self.execute_network_command(cmd)
		
		return [result,output]
	
	def delete_fib_entry(self, ip, mask):
		
		mask = str(mask)
		
		cmd = 'ip route del '+ ip + '/' + mask
		result,output = self.execute_network_command(cmd)
		
		return [result,output]
	
	def flush_fib_entries(self):
		# delete all fib entries (except the default entries)
		result,output = self.execute_network_command("ip route flush root 192.168.0.0/16")
		if result == False:
			return [result,output]
		
		result,output = self.execute_network_command("ip route add 192.168.0.0/16 dev " + self.setup['interface'])
		return [result,output] 
	
	def check_syslog_message(self, message):
		time.sleep(3)
		os.system("sshpass -p ezchip scp root@%s:/var/log/syslog temp_syslog"%(self.setup['host']))
		time.sleep(3)
		syslog_lines = [line.rstrip('\n') for line in open('temp_syslog')]
		for index,line in enumerate(syslog_lines):
			if message in line:
				os.system("rm -f temp_syslog")
				return True
			
		os.system("rm -f temp_syslog")
		return False

class SshConnect:
	def __init__(self, hostname, username, password):

		# Init class variables
		self.ip_address	= hostname
		self.username   = username
		self.password   = password
		self.ssh_object = pxssh.pxssh()
		self.connection_established = False


	def connect(self, prompt_reset = True):
		print "Connecting to : " + self.ip_address + ", username: " + self.username + " password: " + self.password
		self.ssh_object.login(self.ip_address, self.username, self.password, login_timeout=120, auto_prompt_reset = prompt_reset)
		self.connection_established = True
		print self.ip_address + " Connected"
	
	def logout(self):
		if self.connection_established:
			self.ssh_object.send('\003')	#send ctrl+c
			self.ssh_object.logout()
			self.connection_established = False

	def recreate_ssh_object(self):
		self.logout()
		self.ssh_object = pxssh.pxssh()


	#############################################################
	# brief:	 exucute commnad on SSH object
	#
	# in params:	 cmd - coommnad string for execute
	#			
	# return params: success - True for success, False for Error
	#				output -  Command output results	 
	def execute_command(self, cmd, wait_prompt=True):
		try:
			self.ssh_object.sendline(cmd)
			if wait_prompt:
				self.ssh_object.prompt(timeout=120)
				output = '\n'.join(self.ssh_object.before.split('\n')[1:])
				
				# get exit code
				self.ssh_object.sendline("echo $?")
				self.ssh_object.prompt(timeout=120)
				exit_code = self.ssh_object.before.split('\n')[1]
		 
				try:
					exit_code = int(exit_code)
				except:
					exit_code = None
					return [False, output]
				
# 				if exit_code != None:
				if exit_code == 0:
					return [True, output]
				else:
					return [False, output]
			else:
				return [True, '']
		except:
			return [False, "command failed: " + cmd]

	#############################################################
	# brief:	get exit code after command executed
	#
	# in params: None
	#			
	# return:	None	 
	def get_execute_command_exit_code(self, is_telnet=False):
		try:
			self.ssh_object.sendline("echo $?")
			if is_telnet:
				self.ssh_object.expect(' #')
			else:
				self.ssh_object.prompt(timeout=120)
			exit_code = self.ssh_object.before.split('\n')[1]
			exit_code = int(exit_code)
				
		except:
			exit_code = None
		return exit_code

	#############################################################
	# brief:	exucute commnad on chip (NPS)
	#
	# details:	command use ssh_objet, telnet chip, send command & exit telnet
	#
	# in params: cmd - coommnad string for execute
	#			
	# return params: success - True for success, False for Error
	#				output -  Command output results	 
	def execute_chip_command(self, chip_cmd, wait_prompt=True):
		print "Execute on chip: %s" %chip_cmd
		
		nps_internal_ip = "169.254.42.44"
		exit_code       = None
		try:
			# telnet chip
			print "Waiting for a ping respone from nps"
			cmd = "telnet %s" %nps_internal_ip
			for i in range(100):
				self.ssh_object.sendline('ping -c1 -w10 nps_if > /dev/null 2>&1')
				exit_code = self.get_execute_command_exit_code(False)
				if exit_code==0:
					break
			
			if(exit_code!=0):
				raise Exception('commmand failed')
			
			
			print "Trying to telnet nps"
			for i in range(15):
				self.ssh_object.sendline(cmd)
				index=self.ssh_object.expect([' #',pexpect.TIMEOUT],timeout=10)
				if index==0:
					break
				else:
					print "Got timeout or connection refused, Trying again"
			
			if(index!=0):
				raise Exception('commmand failed')		
			
			exit_code = self.get_execute_command_exit_code(True)
 			if (exit_code != 0):
 				raise Exception('commmand failed')
			
			
			
			# excute command
			self.ssh_object.sendline(chip_cmd)
			self.ssh_object.expect(' #')
			output = '\n'.join(self.ssh_object.before.split('\n')[1:])
			exit_code = self.get_execute_command_exit_code(True)
 			if (exit_code != 0):
 				raise Exception('commmand failed')
			
		
			
			# Exit telnet
			self.ssh_object.sendline('exit')
			self.ssh_object.prompt(timeout=120)
			exit_code = self.get_execute_command_exit_code(False)
 			if (exit_code != 1):
 				raise Exception('commmand failed')
			
			
			
			return [True, output]
		except:
			if exit_code != None:
				print "exit code " + str(exit_code)
			else:
				print "exit code is None..."
			return [False, "command failed: " + chip_cmd]

	def wait_for_msgs(self, msgs):
		for i in range(30):
			index = self.ssh_object.expect_exact([pexpect.EOF, pexpect.TIMEOUT] + msgs)
			if index == 0:
				print "\n" + self.ssh_object.before + self.ssh_object.match
				return -1
			elif index == 1:
				sys.stdout.write(".") # timeout
				sys.stdout.flush()
			else:
				return (index-2)
		print "Error while waiting for message"
		return -1

class player(object):

	def __init__(self, ip, hostname, username, password, interface = None, 
				all_eths = None, exe_path=None, exe_script=None, exec_params=None, dump_pcap_file = None):
		self.all_ips         = ip
		self.ip              = ip
		self.hostname        = hostname
		self.username        = username
		self.password        = password
		self.exe_path        = exe_path
		self.exe_script      = exe_script
		self.exec_params     = exec_params
		self.interface       = interface
		self.all_eths        = all_eths
		self.eth             = (None if all_eths is None else all_eths[0])
		self.ssh             = SshConnect(hostname, username, password)
		self.dump_pcap_file  = dump_pcap_file

	def config_interface(self):
 		if self.interface == Network_Interface.REGULAR:
 			eth_up = self.all_eths[0]
 			eth_down = self.all_eths[1:]
 		else:	# routing interface, interface = "number of network"
 			eth_down = self.all_eths[:self.interface] + self.all_eths[self.interface+1:]
 			eth_up = self.all_eths[self.interface]
 		
 		interfaces = self.read_ifconfig()
 		if eth_up not in interfaces:
 			self.ifconfig_eth_up(eth_up)
 		for eth in eth_down:
 			if eth in interfaces:
 				self.ifconfig_eth_down(eth)
		
 		self.eth = eth_up
	
	def change_interface(self, interface):
		self.ip = self.all_ips[interface]
		self.interface = interface
		
		
	def send_ping(self, ip, timeout=10, count=1, size=56):
		cmd = 'ping -c%s -w%s -s%s %s > /dev/null 2>&1' % (count, timeout, size, ip)
		self.ssh.ssh_object.sendline(cmd)
		self.ssh.ssh_object.prompt(timeout=120)
		exit_code = self.ssh.get_execute_command_exit_code()
		return exit_code
	
	def capture_packets(self, params):
		cmd = 'pkill -HUP -f tcpdump;' + params
		logging.log(logging.DEBUG,"running command:\n"+cmd)
		self.ssh.ssh_object.sendline(cmd)
		self.ssh.ssh_object.prompt()
	
	def stop_capture(self):
		cmd = 'pkill -HUP -f tcpdump'
		self.ssh.ssh_object.sendline(cmd)
		self.ssh.ssh_object.prompt()
		output = self.ssh.ssh_object.before
		
		# send a dummy command to clear all unnecessary outputs
		self.ssh.ssh_object.sendline("echo $?")
		self.ssh.ssh_object.prompt()
		
		num_of_packets_received = check_packets_on_pcap(self.dump_pcap_file, self.ssh.ssh_object)
		
		return num_of_packets_received
	
	def read_ifconfig(self):
		result, exit_code = self.ssh.execute_command("ifconfig")
		if result == False:
			err_msg = "Error: command ifconfig failed, exit code: \n%s" %exit_code
			raise RuntimeError(err_msg)
		return exit_code
	
	def ifconfig_eth_up(self, eth_up):
		result, exit_code = self.ssh.execute_command("ifup " + eth_up + " --force")
		result2, exit_code2 = self.ssh.execute_command("ifconfig " + eth_up + " up")
		if result == False and result2 == False:
			err_msg = "Error: command ifup " + eth_up + " failed, exit code: \n%s" %exit_code
			raise RuntimeError(err_msg)

	def ifconfig_eth_down(self, eth_down):
		result, exit_code = self.ssh.execute_command("ifdown " + eth_down + " --force")
		result2, exit_code2 = self.ssh.execute_command("ifconfig " + eth_down + " down")
		if result == False and result2 == False:
			err_msg = "Error: command ifdown " + eth_down + " failed, exit code: \n%s" %exit_code
			raise RuntimeError(err_msg)

	def get_mac_adress(self):
		[result, mac_address] = self.execute_command("cat /sys/class/net/" + self.eth + "/address")
		if result == False:
			print "Error while retreive local address"
			print mac_address
			exit(1)
		mac_address = mac_address.strip()
		return mac_address

	def execute(self, script=None,exe_prog="python"):
		if self.exe_script:
			if script==None:
				script=self.exe_script
			sshpass_cmd = "sshpass -p " + self.password + " ssh -o StrictHostKeyChecking=no " + self.username + "@" + self.hostname
			exec_cmd	 = "cd " + self.exe_path + "; " + exe_prog + " " + script
			cmd = sshpass_cmd + " \"" + exec_cmd + " " + self.exec_params + "\""
			os.system(cmd)
		return

	def connect(self):
		self.ssh.connect()

	def clear_arp_table(self):
		self.ssh.execute_command('ip neigh flush all')
		
	def logout(self):
		self.ssh.logout()
		
	def str(self):
		return self.ip

	def execute_command(self, cmd):
		return self.ssh.execute_command(cmd)
	
	def remove_exe_dir(self):
		self.execute_command('rm -rf %s'%self.exe_path)
	
	def create_exe_dir(self):
		self.execute_command('mkdir -p %s'%self.exe_path)
	
	def copy_file_to_player(self, filename, dest):
		cmd = "sshpass -p " + self.password + " scp " + filename + " " + self.username + "@" + self.hostname + ":" + dest
		rc =  os.system(cmd)
		if rc:
			print "ERROR: failed to copy %s to %s" %(filename, dest)
		return rc

class Host(player):
	def __init__(self, hostname, all_eths,
				ip          = None,
				username    = "root",
				password    = "3tango",
				exe_path    = "/temp/host/",
				dump_pcap_file = '/tmp/host_dump.pcap',
				exe_script  = None,
				exec_params = None,
				mode        = None):
		# init parent class
		super(Host, self).__init__(ip, hostname, username, password, mode, all_eths,
								 exe_path, exe_script, exec_params, dump_pcap_file)
		# Init class variables
		self.pcap_files = []
		self.mac_adress = None
	
	def init_host(self):
		self.connect()
		self.config_interface()
		self.mac_adress= self.get_mac_adress()
	
	def clean_host(self):
		self.logout()

	def copy_and_send_pcap(self, pcap_file):
		copy_pcap(pcap_file)
		send_pcap(pcap_file)

	def copy_pcap(self, pcap_file):
		logging.log(logging.DEBUG,"Copy packet")
		cmd = "mkdir -p %s" %self.exe_path
		self.execute_command(cmd)
		self.copy_file_to_player(pcap_file, self.exe_path)
		
	def send_pcap(self, pcap_file):
		logging.log(logging.DEBUG,"Send packet")
		pcap_file_name = pcap_file[pcap_file.rfind("/")+1:]
		cmd = "tcpreplay --intf1="+self.eth +" " + self.exe_path + "/" + pcap_file_name
		logging.log(logging.DEBUG,"run command on client:\n" + cmd) #todo
		result, output = self.execute_command(cmd)
		if result == False:
			print "Error while sending a packet to NPS"
			print output
			exit(1)
		self.remove_exe_dir()
	
	##send n packets to specific ip and mac address
	def send_n_packets(self,dest_ip,dest_mac,num_of_packets):
		packet_list_to_send = []
		src_port_list = []
		for i in range (num_of_packets):
		# set the packet size      
			packet_size = 150
	
			random_source_port = '%02x %02x' %(random.randint(1,1255),random.randint(1,255))
			
			data_packet = tcp_packet(mac_da          = dest_mac,
									mac_sa           = self.mac_address,
									ip_dst           = ip_to_hex_display(dest_ip),
									ip_src           = ip_to_hex_display(self.ip),
									tcp_source_port  = random_source_port,
									tcp_dst_port     = '00 50', # port 80
									packet_length    = packet_size)
			
			data_packet.generate_packet()
			print data_packet.packet
			packet_list_to_send.append(data_packet.packet)
		
		#    src_port_list.append(int(random_source_port.replace(' ',''),16))    not sure why need this? 
		pcap_to_send = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name=dir + 'verification/testing/dp/temp_packet_%s.pcap'%self.ip)
		self.pcap_files.append(pcap_to_send)
		self.send_packet(pcap_to_send)
	
	def capture_packets_on_host(self):
		params = ' tcpdump -w ' + self.dump_pcap_file + ' -n -i ' + self.eth + ' ether host ' + self.mac_address + ' &'
		self.capture_packets(params)


class tcp_packet:
	
	packets_counter = 0
	
	def __init__(self, mac_da, mac_sa, ip_src, ip_dst, tcp_source_port, tcp_dst_port, tcp_reset_flag = False, tcp_fin_flag = False, tcp_sync_flag = False, packet_length = 64):
		self.mac_sa = mac_sa
		self.mac_da = mac_da
		
		self.ip_src = ip_src
		# change the format of the ip to "xx xx xx xx" instead of "xxx.xxx.xxx.xxx"
		if '.' in self.ip_src:
			ip_src = ip_src.split('.')
			self.ip_src = '%02x %02x %02x %02x'%(int(ip_src[0]), int(ip_src[1]), int(ip_src[2]), int(ip_src[3]))
		
		self.ip_dst = ip_dst
		self.tcp_source_port = tcp_source_port
		self.tcp_dst_port = tcp_dst_port
		self.tcp_reset_flag = tcp_reset_flag
		self.tcp_fin_flag = tcp_fin_flag
		self.tcp_sync_flag = tcp_sync_flag
		self.packet_length = packet_length
		self.packet = ''
		self.pcap_file_name = ALVSdir + '/verification/testing/dp/pcap_files/packet' + str(self.packets_counter) + '.pcap'
		tcp_packet.packets_counter += 1
	
	def generate_packet(self):
		logging.log(logging.DEBUG, "generating the packet, packet size is %d"%self.packet_length)
		l2_header = self.mac_da + ' ' + self.mac_sa + ' ' + '08 00'
		data = '45 00 00 2e 00 00 40 00 40 06 00 00 ' + self.ip_src + ' ' + self.ip_dst  
		data = data.split()
		data = map(lambda x: int(x,16), data)
		ip_checksum = checksum(data)
		ip_checksum = '%04x'%ip_checksum
		ip_header = '45 00 00 2e 00 00 40 00 40 06 ' + ip_checksum[0:2] + ' ' + ip_checksum[2:4] + ' ' + self.ip_src + ' ' + self.ip_dst
		
		flag = 0
		if self.tcp_fin_flag:
			flag += 1
		if self.tcp_sync_flag:
			flag += 2	
		if self.tcp_reset_flag:
			flag += 4
		flag = '%02x'%flag

		data = self.tcp_source_port + ' ' + self.tcp_dst_port + ' 00 00 00 00 00 00 00 00 50 ' + flag + ' FF FC 00 00 00 00'
		data = data.split()
		data = map(lambda x: int(x,16), data)
		tcp_checksum = checksum(data)
		tcp_checksum = '%04x'%tcp_checksum
		tcp_header = self.tcp_source_port + ' ' + self.tcp_dst_port + ' 00 00 00 00 00 00 00 00 50 ' + flag + ' FF FC ' + tcp_checksum[0:2] + ' ' + tcp_checksum[2:4] + ' 00 00'
		
		packet = l2_header + ' ' + ip_header + ' ' + tcp_header
		temp_length = len(packet.split())
		zero_padding_length = self.packet_length - temp_length
		for i in range(zero_padding_length):
			packet = packet + ' 00'
		
		self.packet = packet[:]
		string_to_pcap_file(self.packet, self.pcap_file_name)
		
		return self.packet



#===============================================================================
# Setup Functions
#===============================================================================
g_setups_dir  = "/.autodirect/sw_regression/nps_sw/MARS/MARS_conf/setups/ALVS_gen/setup_params"

def get_setup_list(setup_num):
	setup_list = []
	
	# Open list file
	file_name = '%s/setup%d_vms.csv' %(g_setups_dir, setup_num)
	infile    = open(file_name, 'r')
	
	# Extract list from file  
	for line in infile:
		input_list = line.strip().split(',')
		if len(input_list) >= 2:
			setup_list.append({'hostname':input_list[0],
								'mng_ip':input_list[2],
								'all_eths':input_list[3::2],# all interfaces [ALVS, routing1, routing2, routing3, routing4]
								'all_ips':[input_list[1]] + input_list[4::2]})# all IP's 
	return setup_list

def get_remote_host(setup_num):
		file_name = '%s/remote_hosts_vms.csv' %(g_setups_dir)
		infile    = open(file_name, 'r')
		
		for index,line in enumerate(infile):
			if (index == setup_num - 1):
				remote_host_info = line.strip().split(',')
				dict = {'mng_ip'        : remote_host_info[0],
						'hostname' 		: remote_host_info[1],
						'all_eths'		:remote_host_info[3::2],
						'all_ips'		:remote_host_info[2::2]}
				break
		return dict	 

def change_switch_lag_mode(setup_num, enable_lag = True):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	file_name = '%s/switch.csv' %g_setups_dir
	infile    = open(file_name, 'r')
	
	for index,line in enumerate(infile):
		if (index == setup_num - 1):
			if line[0] not in '#':
				switch_info = line.strip().split(',')
				dict = ({'ip'          : switch_info[0],
						'ports'        : switch_info[1:5],
						'vlans'        : switch_info[5:-1],
						'port_channel' : switch_info[-1]})
				execute_command_on_switch(dict['ip'], 'no interface port-channel %s'%dict['port_channel'])
				if(enable_lag):
					# create port channel
					execute_command_on_switch(dict['ip'], 'interface port-channel %s'%dict['port_channel'])
					execute_command_on_switch(dict['ip'], 'interface port-channel %s\" \"switchport access vlan 101'%dict['port_channel'])
					# add ports to port channel without vlans
					for port in dict['ports']:
						port = 'interface ethernet 1/%s\" \"'%port
						execute_command_on_switch(dict['ip'], port + 'switchport mode access')
						execute_command_on_switch(dict['ip'], port + 'switchport access vlan 101')
						execute_command_on_switch(dict['ip'], port + 'channel-group %s mode on'%dict['port_channel'])
				else:
					for index, port in enumerate(dict['ports']):
						#create vlan
						execute_command_on_switch(dict['ip'], 'vlan %s'%dict['vlans'][index])
						#add vlan to port
						port = 'interface ethernet 1/%s\" \"'%port
						execute_command_on_switch(dict['ip'], port + 'switchport mode access')
						execute_command_on_switch(dict['ip'], port + 'switchport access vlan %s'%dict['vlans'][index])
			break
	
def execute_command_on_switch(ip, exec_param):
	ssh_cmd = 'sshpass -p admin ssh -o StrictHostKeyChecking=no admin@' + ip
	exec_cmd = ' cli -h \'\"enable\" \"configure terminal\" \"' + exec_param + '\"\' > /dev/null 2>&1 ' 
	cmd = ssh_cmd + exec_cmd
	rc = os.system(cmd)
	if rc != 0:
		err_msg = " ERROR: failed to configure switch. command: %s" %cmd
		raise RuntimeError(err_msg)
	
def get_ezbox_names(setup_id):
	setup_dict = []
	
	# Open list file
	file_name = '%s/ezboxes.csv' %(g_setups_dir)
	infile    = open(file_name, 'r')
	
	# Extract list from file  
	for line in infile:
		input_list = line.strip().split(',')
		if len(input_list) >= 2:
			setup_dict.append({'name':             input_list[0],
							'host':                input_list[1],
							'chip':                input_list[2],
							'interface':           input_list[3],
							'username':            input_list[4],
							'password':            input_list[5],
							'data_ip_hex_display': input_list[6],
							'mac_address':         input_list[7],
							'nps_port_type':       input_list[8],
							'mng_ip':              input_list[9],
							'trex_hostname':       input_list[10]})
	
	return setup_dict[setup_id-1]

def get_setup_vip_list(setup_id):
	setup_dict = []
	
	# Open list file
	file_name = '%s/vips.csv' %(g_setups_dir)
	infile    = open(file_name, 'r')
	
	# Extract list from file  
	for line in infile:
		input_list = line.strip().split(',')
		if len(input_list) >= 2:
			setup_dict.append( [ input_list[0], input_list[1] ] )
	
	return setup_dict[setup_id-1]

def get_vip(vip_list,index):
	return vip_list[index/256].replace('x',str(index%256))

def get_setup_vip(setup_num,index):
	return get_vip(get_setup_vip_list(setup_num),index)


#===============================================================================
# Checker Functions
#===============================================================================


def ip2int(addr):															   
	return struct.unpack("!I", socket.inet_aton(addr))[0]					   
 
def int2ip(addr):															   
	return socket.inet_ntoa(struct.pack("!I", addr)) 


def bool_str_to_bool(str):
	return True if str.lower() == 'true' else False

def mask_ip(ip, mask):
	mask = bin(0xFFFFFFFF<<32-mask)
	int_ip = ip2int(ip)
	new_ip = int_ip & int(mask,2)
	return int2ip(new_ip)

def compare_pcap_files(file_name_1, file_name_2):
	num_of_packets_1 = os.popen("tcpdump -r %s"%file_name_1).read().strip('\n')
	num_of_packets_2 = os.popen("tcpdump -r %s"%file_name_1).read().strip('\n')
	
	
	if len(num_of_packets_1) != len(num_of_packets_2):
		print "ERROR, num of packets on pcap files is not equal"
		print "first pcap num of packets " + num_of_packets_1
		print "second pcap num of packets " + num_of_packets_2
		exit(1) 
	
	
	data_1 = os.popen("tcpdump -r %s -XX "%file_name_1).read().split('\n')
	data_2 = os.popen("tcpdump -r %s -XX "%file_name_2).read().split('\n')
	
	
	for i in range(len(data_1)):
		print i
		print data_1[i]
		print data_2[i]
		
		if data_1[i] in num_of_packets_1: # this line we are not comparing (description of the packet, includes imestamp)
			continue
	   
		if data_1[i] != data_2[i]:
			return False
		
	return True


	
	
	
def check_packets_on_pcap(pcap_file_name, ssh_object=None):
	cmd = "tcpdump -r %s | wc -l "%pcap_file_name
	logging.log(logging.DEBUG,"executing: "+cmd)
	
	if ssh_object == None:
		output = os.popen(cmd)
		output = output.read()
		try:
			num_of_packets_received = int(output.split('\n')[1])
			return num_of_packets_received
		except:
			print "ERROR, cannot get num of packets that was received"
			print "output: " + output
			return -1
	else:
		ssh_object.sendline(cmd)
		ssh_object.prompt()
		output = ssh_object.before
		logging.log(logging.DEBUG,"output: "+output)
	
		try:
			num_of_packets_received = int(output.split('\n')[2])
			return num_of_packets_received
		except:
			print "ERROR, cannot get num of packets that was received"
			print "output: " + output
			return -1
		
def create_pcap_file(packets_list, output_pcap_file_name):
	# create a temp pcap directory for pcap files
	if not os.path.exists(os.path.dirname(output_pcap_file_name)):
		os.makedirs(os.path.dirname(output_pcap_file_name))
	else:
		os.system("rm -f "+ output_pcap_file_name)

	# create temp text file
	tmp_file_name = 'temp.txt'
	os.system("touch "+ tmp_file_name)
	
	for packet_str in packets_list:
		cmd = "echo 0000	" + packet_str + " >> " + tmp_file_name
		os.system(cmd)

	os.system("echo 0000 >> " + tmp_file_name)
	
	cmd = "text2pcap " + tmp_file_name + " " + output_pcap_file_name + ' &> /dev/null'
	os.system(cmd)
	os.system("rm -f " + tmp_file_name)
	
	return output_pcap_file_name

def string_to_pcap_file(packet_string, output_pcap_file):
	# create a temp pcap directory for pcap files
	if not os.path.exists(os.path.dirname(output_pcap_file)):
		os.makedirs(os.path.dirname(output_pcap_file))
	os.system("rm -f " + output_pcap_file)
	cmd = "echo 0000	" + packet_string + " > tmp.txt"   
	os.system(cmd)
# 	cmd = "text2pcap " + "tmp.txt " + output_pcap_file + ' &> /dev/null'
	cmd = "text2pcap " + "tmp.txt " + output_pcap_file
	os.system(cmd)
	os.system("rm -f tmp.txt")

def ip_to_hex_display(ip):
	splitted_ip = ip.split('.')
	return '%02x %02x %02x %02x'%(int(splitted_ip[0]), int(splitted_ip[1]), int(splitted_ip[2]), int(splitted_ip[3]))
	
def int2ip(addr):
	return socket.inet_ntoa(struct.pack("!I", addr)) 

def mac2int(addr):
	temp = addr.replace(':','')
	return int(temp,16)

def int2mac(addr):
	mac = hex(addr)
	mac = mac[2:]
	mac = '0'*(12-len(mac)) + mac
	temp = mac[0:2] + ":" + mac[2:4] + ":" + mac[4:6] + ":" + mac[6:8] + ":" + mac[8:10] + ":" + mac[10:12]
	return temp

def add2ip(addr,num):
	temp = ip2int(addr)
	temp = temp + num
	return int2ip(temp)

def add2mac(addr,num):
	temp = mac2int(addr)
	temp = temp + num
	return int2mac(temp)

def ip2int(addr):
	return struct.unpack("!I", socket.inet_aton(addr))[0]

def carry_around_add(a, b):
	c = a + b
	return (c & 0xffff) + (c >> 16)

def checksum(msg):
	s = 0
	for i in range(0, len(msg), 2):
		w = msg[i+1] + (msg[i] << 8)
		s = carry_around_add(s, w)
	return ~s & 0xffff


