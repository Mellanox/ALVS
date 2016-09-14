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

# pythons modules 
from test_infra import *
from unittest2 import result
from cmd import Cmd

#===============================================================================
# Struct IDs
#===============================================================================
STRUCT_ID_NW_INTERFACES				   = 0
STRUCT_ID_NW_LAG					   = 1
STRUCT_ID_ALVS_CONN_CLASSIFICATION	   = 2
STRUCT_ID_ALVS_CONN_INFO			   = 3
STRUCT_ID_ALVS_SERVICE_CLASSIFICATION  = 4
STRUCT_ID_ALVS_SERVICE_INFO			   = 5
STRUCT_ID_ALVS_SCHED_INFO			   = 6
STRUCT_ID_ALVS_SERVER_INFO			   = 7
STRUCT_ID_NW_FIB					   = 8
STRUCT_ID_NW_ARP					   = 9
STRUCT_ID_ALVS_SERVER_CLASSIFICATION   = 10
STRUCT_ID_APPLICATION_INFO			   = 11

#===============================================================================
# STATS DEFINES
#===============================================================================
ALVS_SERVICES_MAX_ENTRIES	 = 256
ALVS_SERVERS_MAX_ENTRIES	 = ALVS_SERVICES_MAX_ENTRIES*1024
ALVS_NUM_OF_SERVICE_STATS	 = 6
ALVS_NUM_OF_SERVER_STATS	 = 8
NUM_OF_INTERFACES			 = 5

ALVS_NUM_OF_SERVERS_ON_DEMAND_STATS = 1
NW_NUM_OF_IF_STATS			 = 20
ALVS_NUM_OF_ALVS_ERROR_STATS = 40

EMEM_ALVS_ERROR_STATS_POSTED_OFFSET = 0
EMEM_SERVICE_STATS_POSTED_OFFSET = EMEM_ALVS_ERROR_STATS_POSTED_OFFSET + ALVS_NUM_OF_ALVS_ERROR_STATS
EMEM_SERVER_STATS_POSTED_OFFSET  = EMEM_SERVICE_STATS_POSTED_OFFSET + ALVS_SERVICES_MAX_ENTRIES * ALVS_NUM_OF_SERVICE_STATS
EMEM_IF_STATS_POSTED_OFFSET	  = EMEM_SERVER_STATS_POSTED_OFFSET + ALVS_SERVERS_MAX_ENTRIES * ALVS_NUM_OF_SERVER_STATS
EMEM_END_OF_STATS_POSTED		  = EMEM_IF_STATS_POSTED_OFFSET + NW_NUM_OF_IF_STATS*NUM_OF_INTERFACES

EMEM_SERVER_STATS_ON_DEMAND_OFFSET = 0

CLOSE_WAIT_DELETE_TIME = 32#16 #todo need to change it back to 16 after a fix to aging time.. 
FIN_FLAG_DELETE_TIME = 60
	
#===============================================================================
# Classes
#===============================================================================
class ezbox_host:
	def __init__(self, setup_id):
		self.setup = get_ezbox_names(setup_id)
		self.ssh_object = SshConnect(self.setup['host'], self.setup['username'], self.setup['password'])
		self.run_app_ssh = SshConnect(self.setup['host'], self.setup['username'], self.setup['password'])
		self.syslog_ssh = SshConnect(self.setup['host'], self.setup['username'], self.setup['password'])
		self.cpe = EZpyCP(self.setup['host'], 1234)
		self.install_path = "alvs_install"

	def reset_ezbox(self):
		os.system("/.autodirect/LIT/SCRIPTS/rreboot " + self.setup['name'])

	def execute_command_on_chip(self, chip_cmd):
		return self.ssh_object.execute_chip_command(chip_cmd)
		
	def modify_run_cpus(self, use_4k_cpus=True):
		if use_4k_cpus:
			cpus = "0,16-4095"
			print "Config CPUs: %s" %cpus
		else:
			cpus = "0,16-511"
			print "Config CPUs: %s" %cpus

		self.execute_command_on_chip("fw_setenv krn_possible_cpus %s" %cpus)
		self.execute_command_on_chip("fw_setenv krn_present_cpus %s" %cpus)

	def update_dp_params(self, params=None):
		cmd = "find /etc/default/alvs | xargs grep -l ALVS_DP_ARGS | xargs sed -i '/ALVS_DP_ARGS=/c\ALVS_DP_ARGS=\"%s\"' " %params
		self.execute_command_on_host(cmd)

	def update_dp_cpus(self, use_4k_cpus=True):
		# upfate DP params
		if use_4k_cpus:
			self.update_dp_params("--run_cpus 16-4095")
		else:
			self.update_dp_params("--run_cpus 16-511")
		
		# modify NPS present & posible CPUs
		self.modify_run_cpus(use_4k_cpus)

	def update_cp_params(self, params=None):
		cmd = "find /etc/default/alvs | xargs grep -l ALVS_CP_ARGS | xargs sed -i '/ALVS_CP_ARGS=/c\ALVS_CP_ARGS=\"%s\"' " %params
		self.execute_command_on_host(cmd)

	def clean(self, use_director=False, stop_ezbox=False):
		if use_director:
			self.clean_director()
			
		if stop_ezbox:
			self.alvs_service_stop()
		else:
			self.zero_all_ipvs_stats()
			self.flush_ipvs()
		self.clean_vips()
		self.logout()
		
	def init_keepalived(self, services):
		conf_filename = 'keepalived.conf'
		conf_folder = '/etc/keepalived/'
		outfile = open(conf_filename, 'w')
		outfile.write('########################################################################\n')
		outfile.write('# Automaticaly generated configuration file from E2E test environment. #\n')
		outfile.write('########################################################################\n')
		outfile.write('global_defs {\n')
		outfile.write('	lvs_id LVS_MAIN\n')
		outfile.write('}\n')
		for vip in services.keys():
			outfile.write('virtual_server %s 80 {\n'%vip)
			outfile.write('	delay_loop 5\n')
			outfile.write('	lb_algo sh\n') #todo
			outfile.write('	lb_kind DR\n')
			outfile.write('	protocol TCP\n')
			for server, weight in services[vip]:
				outfile.write('	real_server %s 80 {\n'%server)
				outfile.write('		weight %d\n'%weight)
				outfile.write('		HTTP_GET {\n')
				outfile.write('			url {\n')
				outfile.write('				path /test.html\n')
				i=0
				while True:
					try:
						result, output = self.execute_command_on_host("genhash -s %s -p 80 -u /test.html"%server)
						if result == False:
							print "ERROR while executing command on host: genhash -s %s -p 80 -u /test.html"%server
						
						digest = output.split()[2]
						break
					except:
						print "error while figure out genhash value, server %s"%server
						if i==10:
							print "ERROR"
							exit(1)
						i+=1
						
				outfile.write('				digest %s\n'%digest)
				outfile.write('			}\n')
				outfile.write('			connect_timeout 3\n')
				outfile.write('			nb_get_retry 3\n')
				outfile.write('			delay_before_retry 2\n')
				outfile.write('		}\n')
				outfile.write('	}\n')
			outfile.write('}\n')
		outfile.close()
		self.copy_file_to_host(conf_filename, conf_folder+conf_filename)
		cmd = 'rm -f '+conf_filename
		os.system(cmd)
		self.execute_command_on_host('/etc/init.d/keepalived start')
	
	def clean_keepalived(self):
		conf_filename = 'keepalived.conf'
		conf_folder = '/etc/keepalived/'
		outfile = open(conf_filename, 'w')
		outfile.write('########################################################################\n')
		outfile.write('# Automaticaly generated configuration file from E2E test environment. #\n')
		outfile.write('########################################################################\n')
		outfile.write('# Empty configuration\n')
		outfile.close()
		self.execute_command_on_host('/etc/init.d/keepalived stop')
		self.copy_file_to_host(conf_filename, conf_folder+conf_filename)
		cmd = 'rm -f '+conf_filename
		os.system(cmd)
		
	def init_director(self, services):
		conf_filename = 'ldirectord.cf'
		conf_folder = '/etc/ha.d/'
		outfile = open(conf_filename, 'w')
		outfile.write('########################################################################\n')
		outfile.write('# Automaticaly generated configuration file from E2E test environment. #\n')
		outfile.write('########################################################################\n')
		outfile.write('checktimeout=3\n')
		outfile.write('checkinterval=1\n')
		outfile.write('autoreload=yes\n')
		outfile.write('logfile="/var/log/ldirectord.log"\n')
		outfile.write('quiescent=no\n')
		for vip in services.keys():
			outfile.write('virtual=%s:80\n'%vip)
			for server, weight in services[vip]:
				outfile.write('	real=%s:80 gate %d\n'%(server, weight))
				outfile.write('	service=http\n')
				outfile.write('	request="test.html"\n')
				outfile.write('	receive="Still alive"\n')
				outfile.write('	scheduler=sh\n')
				outfile.write('	protocol=tcp\n')
				outfile.write('	checktype=negotiate\n')
		outfile.close()
		self.copy_file_to_host(conf_filename, conf_folder+conf_filename)
		cmd = 'rm -f '+conf_filename
		os.system(cmd)
		self.execute_command_on_host('/etc/init.d/ldirectord start')

	def clean_director(self):
		conf_filename = 'ldirectord.cf'
		conf_folder = '/etc/ha.d/'
		outfile = open(conf_filename, 'w')
		outfile.write('########################################################################\n')
		outfile.write('# Automaticaly generated configuration file from E2E test environment. #\n')
		outfile.write('########################################################################\n')
		outfile.write('# Empty configuration\n')
		outfile.close()
		self.execute_command_on_host('/etc/init.d/ldirectord stop')
		self.copy_file_to_host(conf_filename, conf_folder+conf_filename)
		cmd = 'rm -f '+conf_filename
		os.system(cmd)

	def config_vips(self, vip_list):
		for index, vip in enumerate(vip_list):
			self.execute_command_on_host("ifconfig %s:%d %s netmask 255.255.0.0"%(self.setup['interface'], index+2, vip))

	def clean_vips(self):
		interface = self.setup['interface']
		rc, ret_val = self.execute_command_on_host("ifconfig")
		ret_list = ret_val.split('\n')
		for line in ret_list:
			if interface in line:
				split_line = line.split(' ')
				vip_if = split_line[0]
				if interface != vip_if:
					index = (vip_if.split(':'))[1]
					if index != '0':
						self.execute_command_on_host("ifconfig %s down"%(vip_if))

	def connect(self):
		self.ssh_object.connect()
		self.run_app_ssh.connect()
		self.syslog_ssh.connect()
		self.syslog_ssh.execute_command('tail -f /var/log/syslog | grep alvs', False)
		 
	def logout(self):
		self.ssh_object.logout()
		self.run_app_ssh.logout()
		self.syslog_ssh.logout()

	def execute_command_on_host(self, cmd):
		return self.ssh_object.execute_command(cmd)
	
	def copy_binaries(self, alvs_daemon, alvs_dp=None):
		self.copy_cp_bin(alvs_daemon)
		if alvs_dp!=None:
			self.copy_dp_bin(alvs_dp)
		
	def copy_file_to_host(self, filename, dest):
		rc =  os.system("sshpass -p " + self.setup['password'] + " scp " + filename + " " + self.setup['username'] + "@" + self.setup['host'] + ":" + dest)
		if rc:
			print "ERROR: failed to copy %s to %s" %(filename, dest)
			exit(1) 

	def copy_cp_bin(self, alvs_daemon='bin/alvs_daemon', debug_mode=False):
		if debug_mode == True:
			alvs_daemon += '_debug'
		self.copy_file_to_host(alvs_daemon, "/usr/sbin/alvs_daemon")
	
	def copy_dp_bin(self, alvs_dp='bin/alvs_dp', debug_mode=False):
		if debug_mode == True:
			alvs_dp += '_debug'
		self.copy_file_to_host(alvs_dp, "/usr/lib/alvs/alvs_dp")

	def install_alvs(self):
		try:
			cmd = "./alvs_install.py"
			self.ssh_object.ssh_object.sendline(cmd)
			# wait for: "Do you want to continue [Y/n]?"
			self.ssh_object.ssh_object.expect("[Y/n]?", 20)
			# continue instalation
			self.ssh_object.ssh_object.sendline('Y')
			# clean before / expect
			try:
				self.ssh_object.ssh_object.prompt(1)
			except:
				pass
			# wait for: "Do you want to overwrite configuration [Y/n]?"
			self.ssh_object.ssh_object.expect("[Y/n]?", 30)
			# dont overide configuration & continue
			self.ssh_object.ssh_object.sendline('n')
			self.ssh_object.ssh_object.prompt(300)
			# look for success string ("ALVS installation completed successfully")
			if "installation completed successfully" in self.ssh_object.ssh_object.before:
				print "installation completed successfully"
			else:
				print "ERROR: instalation failed"
				print self.ssh_object.ssh_object.before
				exit(1)
			
		except:
			rc = "Unexpected error: %s" %sys.exc_info()[0]
			print rc
			exit(1)

	def copy_install_tar(self, install_tar):
		func_name = sys._getframe().f_code.co_name
		print "FUNCTION %s: copy %s to %s " %(func_name, install_tar, self.install_path)
		self.execute_command_on_host("rm -rf %s" %self.install_path)
		self.execute_command_on_host("mkdir -p %s" %self.install_path)
		self.copy_file_to_host(install_tar, self.install_path)

	def install_tar(self, install_tar):
		func_name = sys._getframe().f_code.co_name
		print "FUNCTION %s: called" %(func_name)
		
		# CD folder & extract tar
		self.execute_command_on_host("cd %s/" %self.install_path)
		cmd = "tar xfv %s" %(install_tar)
		self.execute_command_on_host(cmd)
		
		self.install_alvs()
		
		# back to previos folder
		self.execute_command_on_host("cd -")

	def copy_and_install_alvs(self, install_tar='alvs.tar.gz'):
		self.copy_install_tar(install_tar)
		self.install_tar(install_tar)

	def alvs_service(self, cmd):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called for " + cmd + " service"
		self.run_app_ssh.execute_command("/etc/init.d/alvs " + cmd)

	def alvs_service_start(self):
		self.alvs_service("start")
		self.run_app_ssh.execute_command("echo \"\" > /var/log/syslog")

	def alvs_service_stop(self):
		self.alvs_service("stop")

	def alvs_service_restart(self):
		self.alvs_service("restart")

	def reset_chip(self):
		self.run_app_ssh.execute_command("bsp_nps_power -r por")

	def wait_for_cp_app(self):
		sys.stdout.write("Waiting for CP Application to load...")
		sys.stdout.flush() 
		output = self.syslog_ssh.wait_for_msgs(['alvs_db_manager_poll...'])
		print
		if output == 0:
			return True
		elif output < 0:
			print '\nwait_for_cp_app: Error... (end of output)'
			exit(1)
		else:
			print '\nwait_for_cp_app: Error... (Unknown output)'
			exit(1)
		
	def wait_for_dp_app(self):
		sys.stdout.write("Waiting for DP application to load...")
		sys.stdout.flush()
		output = self.syslog_ssh.wait_for_msgs(['starting ALVS DP application'])
		print

		if output == 0:
			return True
		elif output == 1:
			return False
		elif output < 0:
			print "\n" + sys._getframe().f_code.co_name + ": Error... (end of output)"
			return False
		else:
			print "\n" + sys._getframe().f_code.co_name + ": Error... (Unknown output)"
			return False

	def get_cp_app_pid(self):
		retcode, output = self.ssh_object.execute_command("pidof alvs_daemon")		 
		if output == '':
			return None
		else:
			return output.split(' ')[0]
			
	def capture_packets(self, tcpdump_params = None):
		if tcpdump_params == None:
			tcpdump_params = 'dst ' + self.setup['host']

		self.ssh_object.execute_command('pkill -HUP -f tcpdump; tcpdump -w /tmp/dump.pcap -i ' + self.setup['interface'] + ' ' + tcpdump_params + ' &')

	def stop_capture(self):
		self.ssh_object.execute_command('pkill -HUP -f tcpdump')
		time.sleep(1)
		result, output = self.ssh_object.execute_command('tcpdump -r /tmp/dump.pcap | wc -l')
		if result == False:
			print 'ERROR, reading captured num of packets was failed'
			exit(1)
		try:
			num_of_packets = int(output.split('\n')[1].strip('\r'))
		except:
			print 'ERROR, reading captured num of packets was failed'
			exit(1)

		return num_of_packets
	
	def start_state_sync_daemon(self, state, syncid = 0, mcast_if = "eth0"):
		cmd = "ipvsadm --start-daemon %s --syncid %d --mcast-interface %s" %(state, syncid, mcast_if)
		result, output = self.ssh_object.execute_command(cmd)
		if result == False:
			print 'ERROR, starting state sync %s daemon was failed' %state
			return False
		return True
	
	def stop_state_sync_daemon(self, state):
		cmd = "ipvsadm --stop-daemon %s" %(state)
		result, output = self.ssh_object.execute_command(cmd)
		if result == False:
			print 'ERROR, stopping state sync %s daemon was failed' %state
			return False
		return True
	
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
	
	def get_num_of_services(self):
		return self.cpe.cp.struct.get_num_entries(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION, channel_id = 0).result['num_entries']['number_of_entries']

	def get_num_of_connections(self):
		return self.cpe.cp.struct.get_num_entries(STRUCT_ID_ALVS_CONN_CLASSIFICATION, channel_id = 0).result['num_entries']['number_of_entries']
	
	def get_service(self, vip, port, protocol):
		class_res = self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION, 0, {'key' : "%08x%04x%04x" % (vip, port, protocol)})
		warning_code=class_res.result.get('warning_code',None)
		if warning_code != None :
			return None
		class_res = class_res.result["params"]["entry"]["result"].split(' ')
		info_res = self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_SERVICE_INFO, 0, {'key' : class_res[1]})
		warning_code=info_res.result.get('warning_code',None)
		if warning_code != None :
			return None
		info_res = info_res.result["params"]["entry"]["result"].split(' ')

		sched_info = []
		for ind in range(256):
			sched_index = int(class_res[1], 16) * 256 + ind
			sched_res = str(self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_SCHED_INFO, 0, {'key' : "%04x" % sched_index}).result["params"]["entry"]["result"]).split(' ')
			if (int(sched_res[0], 16) >> 4) == 3:
				sched_info.append(int(''.join(sched_res[4:8]), 16))

		service = {'sched_alg' : int(info_res[0], 16) & 0xf,
				   'sched_info_entries' : int(''.join(info_res[2:4]), 16),
				   'stats_base' : int(''.join(info_res[8:12]), 16),
				   'flags' : int(''.join(info_res[12:16]), 16),
				   'sched_info' : sched_info
				   }

		return service

	def get_connection(self, vip, vport, cip, cport, protocol):
		class_res = self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_CONN_CLASSIFICATION, 0, {'key' : "%08x%08x%04x%04x%04x" % (int(cip), int(vip), int(cport), int(vport), int(protocol))})
		if 'warning_code' in class_res.result:
			return None
			
		class_res = str(class_res.result["params"]["entry"]["result"]).split(' ')

		info_res = self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_CONN_INFO, 0, {'key' : ''.join(class_res[4:8])})
		if 'warning_code' in info_res.result:
			print 'This should not happen!'
			return None 

		info_res = str(info_res.result["params"]["entry"]["result"]).split(' ')

		conn = {'sync_bit' : (int(info_res[0], 16) >> 3) & 0x1,
				'aging_bit' : (int(info_res[0], 16) >> 2) & 0x1,
				'delete_bit' : (int(info_res[0], 16) >> 1) & 0x1,
				'reset_bit' : int(info_res[0], 16) & 0x1,
				'bound' : int(info_res[1], 16) & 0x1,
				'server_port' : int(''.join(info_res[2:4]), 16),
				'server' : int(''.join(info_res[4:8]), 16),
				'state' : int(info_res[26], 16),
				'age_iteration' : int(info_res[27], 16),
				'flags' : int(''.join(info_res[28:32]), 16)
				}

		return conn

	def get_interface(self, lid):
		res = str(self.cpe.cp.struct.lookup(STRUCT_ID_NW_INTERFACES, 0, {'key' : "%02x" % lid}).result["params"]["entry"]["result"]).split(' ')
		interface = {'oper_status' : (int(res[0], 16) >> 3) & 0x1,
					 'path_type' : (int(res[0], 16) >> 1) & 0x3,
					 'is_vlan' : int(res[0], 16) & 0x1,
					 'lag_id' : int(res[1], 16),
					 'default_vlan' : int(''.join(res[2:4]), 16),
					 'mac_address' : int(''.join(res[4:10]), 16),
					 'output_channel' : int(res[10], 16),
					 'stats_base' : int(''.join(res[12:16]), 16),
					 }
		
		return interface

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
			interface = {'oper_status' : (int(result[0], 16) >> 3) & 0x1,
						 'path_type' : (int(result[0], 16) >> 1) & 0x3,
						 'is_vlan' : int(result[0], 16) & 0x1,
						 'lag_id' : int(result[1], 16),
						 'default_vlan' : int(''.join(result[2:4]), 16),
						 'mac_address' : int(''.join(result[4:10]), 16),
						 'output_channel' : int(result[10], 16),
						 'stats_base' : int(''.join(result[12:16]), 16),
						 'key' : {'lid' : lid}
						 }

			interfaces.append(interface)
			
		self.cpe.cp.struct.iterator_delete(STRUCT_ID_NW_INTERFACES, iterator_params_dict)
		return interfaces
	
	def get_all_services(self):
		iterator_params_dict = (self.cpe.cp.struct.iterator_create(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION, { 'channel_id': 0 })).result['iterator_params']
		num_entries = (self.cpe.cp.struct.get_num_entries(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION, channel_id = 0)).result['num_entries']['number_of_entries']

		services = []
		for i in range(0, num_entries):
			iterator_params_dict = self.cpe.cp.struct.iterator_get_next(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION, iterator_params_dict).result['iterator_params']
			 
			key = str(iterator_params_dict['entry']['key']).split(' ')
			vip = int(''.join(key[0:4]), 16)
			port = int(''.join(key[4:6]), 16)
			protocol = int(''.join(key[6:8]), 16)
			 
			class_res = str(iterator_params_dict['entry']['result']).split(' ')
			info_res = str(self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_SERVICE_INFO, 0, {'key' : class_res[2]}).result["params"]["entry"]["result"]).split(' ')
			if (int(info_res[0], 16) >> 4) != 3:
				print 'This should not happen!'
				return {}			
			
			sched_info = []
			for ind in range(256):
				sched_index = int(class_res[2], 16) * 256 + ind
				sched_res = str(self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_SCHED_INFO, 0, {'key' : "%04x" % sched_index}).result["params"]["entry"]["result"]).split(' ')
				if (int(sched_res[0], 16) >> 4) == 3:
					sched_info[ind] = int(''.join(sched_res[4:8]), 16)
			
			service = {'sched_alg' : int(info_res[0], 16) & 0xf,
					   'server_count' : int(''.join(info_res[2:4]), 16),
					   'stats_base' : int(''.join(info_res[8:12]), 16),
					   'flags' : int(''.join(info_res[12:16]), 16),
					   'sched_info' : sched_info,
					   'key' : {'vip' : vip, 'port' : port, 'protocol' : protocol}
					   }
			
			services.append(service)
			
		self.cpe.cp.struct.iterator_delete(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION, iterator_params_dict)
		return services

	def get_all_servers(self):
		iterator_params_dict = (self.cpe.cp.struct.iterator_create(STRUCT_ID_ALVS_SERVER_CLASSIFICATION, { 'channel_id': 0 })).result['iterator_params']
		num_entries = (self.cpe.cp.struct.get_num_entries(STRUCT_ID_ALVS_SERVER_CLASSIFICATION, channel_id = 0)).result['num_entries']['number_of_entries']

		servers = []
		for i in range(0, num_entries):
			iterator_params_dict = self.cpe.cp.struct.iterator_get_next(STRUCT_ID_ALVS_SERVER_CLASSIFICATION, iterator_params_dict).result['iterator_params']
			 
			key = str(iterator_params_dict['entry']['key']).split(' ')
			server_ip = int(''.join(key[0:4]), 16)
			virt_ip = int(''.join(key[4:8]), 16)
			server_port = int(''.join(key[8:10]), 16)
			virt_port = int(''.join(key[10:12]), 16)
			protocol = int(''.join(key[12:14]), 16)
			 
			res = str(iterator_params_dict['entry']['result']).split(' ')
			index = int(''.join(res[4:8]), 16)
			
			server = {'index' : index,
					  'key' : {'server_ip' : server_ip, 'virt_ip' : virt_ip, 'server_port' : server_port, 'virt_port' : virt_port, 'protocol' : protocol}
					  }
			
			servers.append(server)
			
		self.cpe.cp.struct.iterator_delete(STRUCT_ID_ALVS_SERVER_CLASSIFICATION, iterator_params_dict)
		return servers

	def get_all_connections(self):
		iterator_params_dict = (self.cpe.cp.struct.iterator_create(STRUCT_ID_ALVS_CONN_CLASSIFICATION, { 'channel_id': 0 })).result['iterator_params']
		num_entries = (self.cpe.cp.struct.get_num_entries(STRUCT_ID_ALVS_CONN_CLASSIFICATION, channel_id = 0)).result['num_entries']['number_of_entries']

		conns = []
		for i in range(0, num_entries):
			iterator_params_dict = self.cpe.cp.struct.iterator_get_next(STRUCT_ID_ALVS_CONN_CLASSIFICATION, iterator_params_dict).result['iterator_params']
			 
			key = str(iterator_params_dict['entry']['key']).split(' ')
			cip = int(''.join(key[0:4]), 16)
			vip = int(''.join(key[4:8]), 16)
			cport = int(''.join(key[8:10]), 16)
			vport = int(''.join(key[10:12]), 16)
			protocol = int(''.join(key[12:14]), 16)
			 
			class_res = str(iterator_params_dict['entry']['result']).split(' ')
			info_res = str(self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_CONN_INFO, 0, {'key' : class_res[2]}).result["params"]["entry"]["result"]).split(' ')
			if (int(info_res[0], 16) >> 4) != 3:
				print 'This should not happen!'
				return {}			
			
			conn = {'sync_bit' : (int(info_res[0], 16) >> 3) & 0x1,
					'aging_bit' : (int(info_res[0], 16) >> 2) & 0x1,
					'delete_bit' : (int(info_res[0], 16) >> 1) & 0x1,
					'reset_bit' : int(info_res[0], 16) & 0x1,
					'age_iteration' : int(info_res[1], 16),
					'server_index' : int(''.join(info_res[2:4]), 16),
					'state' : int(info_res[5], 16),
					'stats_base' : int(''.join(info_res[8:12]), 16),
					'flags' : int(''.join(info_res[28:32]), 16),
					'key' : {'vip' : vip, 'vport' : vport, 'cip' : cip, 'cport' : cport, 'protocol' : protocol}
					}
			
			conns.append(conn)
			
		self.cpe.cp.struct.iterator_delete(STRUCT_ID_ALVS_CONN_CLASSIFICATION, iterator_params_dict)
		return conns

	def add_service(self, vip, port, sched_alg='sh', sched_alg_opt='-b sh-port'):
		self.execute_command_on_host("ipvsadm -A -t %s:%s -s %s %s"%(vip,port, sched_alg, sched_alg_opt))
		time.sleep(1)

	def modify_service(self, vip, port, sched_alg='sh', sched_alg_opt='-b sh-port'):
		self.execute_command_on_host("ipvsadm -E -t %s:%s -s %s %s"%(vip,port, sched_alg, sched_alg_opt))
		time.sleep(1)

	def delete_service(self, vip, port):
		self.execute_command_on_host("ipvsadm -D -t %s:%s"%(vip,port))
		time.sleep(0.5)

	def add_server(self, vip, service_port, server_ip, server_port, weight=1, routing_alg_opt=' '):
		self.execute_command_on_host("ipvsadm -a -t %s:%s -r %s:%s -w %d %s"%(vip, service_port, server_ip, server_port, weight, routing_alg_opt))
		time.sleep(1)

	def modify_server(self, vip, service_port, server_ip, server_port, weight=1, routing_alg_opt=' ', u_thresh = 0, l_thresh = 0):
		self.execute_command_on_host("ipvsadm -e -t %s:%s -r %s:%s -w %d %s -x %d -y %d"%(vip, service_port, server_ip, server_port, weight, routing_alg_opt, u_thresh, l_thresh))
		time.sleep(1)

	def delete_server(self, vip, service_port, server_ip, server_port):
		self.execute_command_on_host("ipvsadm -d -t %s:%s -r %s:%s"%(vip, service_port, server_ip, server_port))
		time.sleep(0.5)

	def flush_ipvs(self):
		self.execute_command_on_host("ipvsadm -C")
		time.sleep(0.5)

	def zero_all_ipvs_stats(self):
		logging.log(logging.INFO, "zero all ipvs stats")
		result, output = self.execute_command_on_host('ipvsadm --zero')
		if result == False:
			print "ERROR, failed to execute zero command"
			print output
			return False
		
		return True
		
	def get_ipvs_stats(self):
		logging.log(logging.INFO, "get all ipvs stats")
		result, output = self.execute_command_on_host('ipvsadm --list --stats')
		if result == False:
			print "ERROR, failed to execute zero command"
			print output
			return False
		
		return True
	
	def clean_arp_table(self):
		logging.log(logging.INFO, "clean arp table")
		cmd = 'ip -s -s neigh flush all'
		result,output = self.execute_command_on_host(cmd)		  # run a command
#		 self.ssh_object.prompt()			   # match the prompt
#		 output = self.ssh_object.before		# print everything before the prompt.
#		 logging.log(logging.DEBUG, output)
		 
		cmd = 'ip -s -s neigh flush nud perm'
		result,output = self.execute_command_on_host(cmd)		  # run a command

#		 self.ssh_object.prompt()			   # match the prompt
#		 output = self.ssh_object.before		# print everything before the prompt.
#		 logging.log(logging.DEBUG, output)
	
	def clear_stats(self):
		error_stats = self.cpe.cp.stat.set_posted_counters(channel_id=0, partition=0, range=True, start_counter=EMEM_ALVS_ERROR_STATS_POSTED_OFFSET, num_counters=ALVS_NUM_OF_ALVS_ERROR_STATS, double_operation=False, clear=True)
		
	def get_error_stats(self):
		error_stats = self.cpe.cp.stat.get_posted_counters(channel_id=0, partition=0, range=True, start_counter=EMEM_ALVS_ERROR_STATS_POSTED_OFFSET, num_counters=ALVS_NUM_OF_ALVS_ERROR_STATS, double_operation=False).result['posted_counter_config']['counters']
		
		stats_dict = {'ALVS_ERROR_UNSUPPORTED_ROUTING_ALGO':error_stats[0]['byte_value'],
					  'ALVS_ERROR_CANT_EXPIRE_CONNECTION':error_stats[1]['byte_value'],
					  'ALVS_ERROR_CANT_UPDATE_CONNECTION_STATE':error_stats[2]['byte_value'],
					  'ALVS_ERROR_CONN_INFO_LKUP_FAIL':error_stats[3]['byte_value'],
					  'ALVS_ERROR_CONN_CLASS_ALLOC_FAIL':error_stats[4]['byte_value'],
					  'ALVS_ERROR_CONN_INFO_ALLOC_FAIL':error_stats[5]['byte_value'],
					  'ALVS_ERROR_CONN_INDEX_ALLOC_FAIL':error_stats[6]['byte_value'],
					  'ALVS_ERROR_SERVICE_CLASS_LKUP_FAIL':error_stats[7]['byte_value'],
					  'ALVS_ERROR_SCHEDULING_FAIL':error_stats[8]['byte_value'],
					  'ALVS_ERROR_SERVER_INFO_LKUP_FAIL':error_stats[9]['byte_value'],
					  'ALVS_ERROR_SERVER_IS_UNAVAILABLE':error_stats[10]['byte_value'],
					  'ALVS_ERROR_SERVER_INDEX_LKUP_FAIL':error_stats[11]['byte_value'],
					  'ALVS_ERROR_CONN_CLASS_LKUP_FAIL':error_stats[12]['byte_value'],
					  'ALVS_ERROR_SERVICE_INFO_LOOKUP':error_stats[13]['byte_value'],
					  'ALVS_ERROR_UNSUPPORTED_SCHED_ALGO':error_stats[14]['byte_value'],
					  'ALVS_ERROR_CANT_MARK_DELETE':error_stats[15]['byte_value'],
					  'ALVS_ERROR_DEST_SERVER_IS_NOT_AVAIL':error_stats[16]['byte_value'],
					  'ALVS_ERROR_SEND_FRAME_FAIL':error_stats[17]['byte_value'],
					  'ALVS_ERROR_CONN_MARK_TO_DELETE':error_stats[18]['byte_value'],
					  'ALVS_ERROR_SERVICE_CLASS_LOOKUP':error_stats[19]['byte_value'],
					  'ALVS_ERROR_UNSUPPORTED_PROTOCOL':error_stats[20]['byte_value'],
#					  'ALVS_ERROR_NO_ACTIVE_SERVERS':error_stats[21]['byte_value'],
					  'ALVS_ERROR_CREATE_CONN_MEM_ERROR':error_stats[22]['byte_value'],
					  'ALVS_ERROR_STATE_SYNC':error_stats[23]['byte_value']}
		
		return stats_dict # return only the lsb (small amount of packets on tests)

	def get_services_stats(self, service_id):
		if service_id < 0 or service_id > ALVS_SERVICES_MAX_ENTRIES:
			return -1
		
		counter_offset = EMEM_SERVICE_STATS_POSTED_OFFSET + service_id*ALVS_NUM_OF_SERVICE_STATS
		
		# return only the lsb (small amount of packets on tests)
		service_stats = self.cpe.cp.stat.get_posted_counters(channel_id=0, 
															 partition=0, 
															 range=True, 
															 start_counter=counter_offset, 
															 num_counters=ALVS_NUM_OF_SERVICE_STATS, 
															 double_operation=False).result['posted_counter_config']['counters']
	
		
		stats_dict = {'SERVICE_STATS_IN_PKT':service_stats[0]['byte_value'],
					  'SERVICE_STATS_IN_BYTES':service_stats[1]['byte_value'],
					  'SERVICE_STATS_OUT_PKT':service_stats[2]['byte_value'],
					  'SERVICE_STATS_OUT_BYTES':service_stats[3]['byte_value'],
					  'SERVICE_STATS_CONN_SCHED':service_stats[4]['byte_value'],
					  'SERVICE_STATS_REF_CNT':service_stats[5]['byte_value']}
		return stats_dict
	
	def get_all_active_servers(self):
		self.ssh_object.ssh_object.sendline("echo -e \"select nps_index from servers where active = 1;\" | sqlite3 /" + "alvs.db")
		self.ssh_object.ssh_object.prompt()
		server_index_list = map(lambda str: str.split('|'), self.ssh_object.ssh_object.before.split('\n')[1:-1])
		server_index_list = [int(s[0].strip()) for s in server_index_list if isinstance(s[0].strip(), int)]
		return server_index_list

	def get_servers_stats(self, server_id):
		if server_id < 0 or server_id > ALVS_SERVERS_MAX_ENTRIES:
			return -1
		
		counter_offset = EMEM_SERVER_STATS_POSTED_OFFSET + server_id*ALVS_NUM_OF_SERVER_STATS
		
		# return only the lsb (small amount of packets on tests)
		server_stats = self.cpe.cp.stat.get_posted_counters(channel_id=0, 
															partition=0, 
															range=True, 
															start_counter=counter_offset, 
															num_counters=ALVS_NUM_OF_SERVER_STATS, 
															double_operation=False).result['posted_counter_config']['counters']
		
		connection_total = self.get_server_connections_total_stats(server_id)
		
		stats_dict = {'SERVER_STATS_IN_PKT':server_stats[0]['byte_value'],
					  'SERVER_STATS_IN_BYTES':server_stats[1]['byte_value'],
					  'SERVER_STATS_OUT_PKT':server_stats[2]['byte_value'],
					  'SERVER_STATS_OUT_BYTES':server_stats[3]['byte_value'],
					  'SERVER_STATS_CONN_SCHED':server_stats[4]['byte_value'],
					  'SERVER_STATS_REFCNT':server_stats[5]['byte_value'],
					  'SERVER_STATS_INACTIVE_CONN':server_stats[6]['byte_value'],
					  'SERVER_STATS_ACTIVE_CONN':server_stats[7]['byte_value'],
					  'SERVER_STATS_CONNECTION_TOTAL':connection_total}
		
		return stats_dict

	def get_server_connections_total_stats(self, server_id):
		if server_id < 0 or server_id > ALVS_SERVERS_MAX_ENTRIES:
			return -1
		
		counter_offset = EMEM_SERVER_STATS_ON_DEMAND_OFFSET + server_id * ALVS_NUM_OF_SERVERS_ON_DEMAND_STATS
		sched_connections_on_server =  self.cpe.cp.stat.get_long_counter_values(channel_id=0, 
																				partition=0, 
																				range=1, 
		                                             							start_counter=counter_offset, 
		                                             	    					num_counters=ALVS_NUM_OF_SERVERS_ON_DEMAND_STATS, 
		                                             		  	     			read=0, 
		                                             		  	     			use_shadow_group=0).result['long_counter_config']['counters']

		return sched_connections_on_server[0]['value']

	def get_interface_stats(self, interface_id):
		if interface_id < 0 or interface_id > NUM_OF_INTERFACES:
			return -1
		
		counter_offset = EMEM_IF_STATS_POSTED_OFFSET + interface_id*NW_NUM_OF_IF_STATS
		
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
	
	def print_all_stats(self):
		print
		
		# print error stats
		print "Error Statistics"
		error_stats = self.get_error_stats()
		for key, value in error_stats.iteritems():
			if value>0:
				print "Error stats (%s): %d"%(key,value)
				logging.log(logging.INFO,"Error stats (%s): %d"%(key,value))
			
		# print service stats
		print "Services Statistics:"
		for i in range(ALVS_SERVICES_MAX_ENTRIES):
			service_stats = self.get_services_stats(i)
			for key, value in service_stats.iteritems():
				if value>0:
					print "Service id %d stats (%s): %d"%(i,key,value)	
					logging.log(logging.INFO,"Service id %d stats (%s): %d"%(i,key,value))
		
		# print servers stats
		print "Server Statistics:"
		# only prints the first 1000 servers (all the servers 256k takes a lot of time..)
		for i in range(1000): 
			server_stats = self.get_servers_stats(i)
			for key, value in server_stats.iteritems():
				if value>0:
					print "Server id %d stats (%s): %d"%(i,key,value)
					logging.log(logging.INFO,"Server id %d stats (%s): %d"%(i,key,value))
		
		# print interface stats
		print "Interface Statistics:"
		for i in range(NUM_OF_INTERFACES):
			interface_stats = self.get_interface_stats(i)
			for key, value in interface_stats.iteritems():
				if value>0:
					print "Interface %d stats (%s): %d"%(i,key,value)
					logging.log(logging.INFO,"Interface %d stats (%s): %d"%(i,key,value))			
		print
		
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
			
		result,output = self.execute_command_on_host(cmd)
		
		return [result,output]
	
	def delete_fib_entry(self, ip, mask):
		
		mask = str(mask)
		
		cmd = 'ip route del '+ ip + '/' + mask
		result,output = self.execute_command_on_host(cmd)
		
		return [result,output]
	
	def flush_fib_entries(self):
		# delete all fib entries (except the default entries)
		result,output = self.execute_command_on_host("ip route flush root 192.168.0.0/16")
		if result == False:
			return [result,output]
		
		result,output = self.execute_command_on_host("ip route add 192.168.0.0/16 dev " + self.setup['interface'])
		return [result,output] 
	
	def read_stats_on_syslog(self):
# 		os.system("sshpass -p ezchip scp root@%s:/var/log/syslog temp_syslog"%(self.setup['host']))
		syslog_lines = [line.rstrip('\n') for line in open('temp_syslog')]
		interface_stats = {}
		global_syslog_stats = {}
		all_servers_stats = {}
		service_stats = {}
				
		for index,line in enumerate(syslog_lines):
			if 'Statistics of global errors' in line:
				print "found global, index = " + str(index)
				global_syslog_stats = {}
				for i in range(index+1,index+100):
					temp = syslog_lines[i].find('<info>      ')
					if temp>0:
						try:
							global_syslog_stats[syslog_lines[i].split()[6]] = int(syslog_lines[i].split()[9])
						except:
							print "error parsing global stats syslog message"
							pass
					else:
						break
			if 'Statistics of Network Interface' in line or 'Statistics of Host Interface' in line:
				interface_stats = {}
				if 'Statistics of Host Interface' in line:
					interface_num = 'host'
				else:
					interface_num = int(line.split()[line.split().index('Interface')+1])
				print interface_num
				interface_stats[interface_num] = {}
				for i in range(index+1,index+100):
					temp = syslog_lines[i].find('<info>      ')
					if temp>0:
						try:
							interface_stats[interface_num][syslog_lines[i].split()[6]] = int(syslog_lines[i].split()[9])
						except:
							print "error parsing interface stats syslog message"
							pass
					else:
						break
			if 'Print Statistics for Services:' in line:
				service_stats = {}
				for i in range(index+2,index+258):
					try:
						line_split = syslog_lines[i].split()
						if ':80' in line_split[6]:
							service = line_split[6][:-3]
							service_stats[service] = {}
							service_stats[service]['IN_PACKETS'] = int(line_split[8])
							service_stats[service]['IN_BYTES'] = int(line_split[10])
							service_stats[service]['OUT_PACKETS'] = int(line_split[12])
							service_stats[service]['OUT_BYTES'] = int(line_split[14])
							service_stats[service]['CONN_SCHED'] = int(line_split[16])
						else:
							break
					except:
						print "Error parsing service stats"

			if 'Print Statistics for Servers:' in line:
				all_servers_stats = {}
				server_index = 0
				service = line.split()[11]
				all_servers_stats[service] = {}
				for i in range(index+2,index+258):
					try:
						line_split = syslog_lines[i].split()
						if 'Server' in line_split[6] and ':80' in line_split[7]:
							server_stats = {}
							server_stats['IN_PACKETS'] = int(line_split[8])
							server_stats['IN_BYTES'] = int(line_split[10])
							server_stats['OUT_PACKETS'] = int(line_split[12])
							server_stats['OUT_BYTES'] = int(line_split[14])
							server_stats['CONN_SCHED'] = int(line_split[16])
							server_stats['CONN_TOTAL'] = int(line_split[18])
							server_stats['ACTIVE_CONN'] = int(line_split[20])
							server_stats['INACTIVE_CONN'] = int(line_split[22])
							all_servers_stats[service][line_split[7]] = server_stats
							
						else:
							break
					except:
						print "Error parsing servers stats"
						pass	

		return {'interface_stats':interface_stats,
				'global_stats':global_syslog_stats,
				'server_stats':all_servers_stats,
				'service_stats':service_stats}		
			
class SshConnect:
	def __init__(self, hostname, username, password):

		# Init class variables
		self.ip_address	= hostname
		self.username   = username
		self.password   = password
		self.ssh_object = pxssh.pxssh()

	def connect(self):
		print "Connecting to : " + self.ip_address + ", username: " + self.username + " password: " + self.password
		self.ssh_object.login(self.ip_address, self.username, self.password, login_timeout=120)
		print self.ip_address + " Connected"
		
	def logout(self):
		self.ssh_object.send('\003')	#send ctrl+c
		self.ssh_object.logout()

	def recreate_ssh_object(self):
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
			cmd = "telnet %s" %nps_internal_ip
			self.ssh_object.sendline(cmd)
			self.ssh_object.expect(' #')
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
		for i in range(40):
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
	def __init__(self, ip, hostname, username, password, exe_path=None, exe_script=None, exec_params=None):
		self.ip = ip
		self.hostname = hostname
		self.username = username
		self.password = password
		self.exe_path = exe_path
		self.exe_script = exe_script
		self.exec_params = exec_params
		self.ssh		= SshConnect(hostname, username, password)

	def execute(self, exe_prog="python"):
		if self.exe_script:
			sshpass_cmd = "sshpass -p " + self.password+ " ssh -o StrictHostKeyChecking=no " + self.username + "@" + self.hostname
			exec_cmd	= "cd " + self.exe_path + "; " + exe_prog + " " + self.exe_script
			cmd = sshpass_cmd + " \"" + exec_cmd + " " + self.exec_params + "\""
			print cmd
			os.system(cmd)

	def connect(self):
		self.ssh.connect()

	def clear_arp_table(self):
		self.ssh.execute_command('ip neigh flush all')
		
	def logout(self):
		self.ssh.logout()
		
	def str(self):
		return self.ip

	def execute_command(self, cmd):
		self.ssh.execute_command(cmd)
		
	def copy_file_to_player(self, filename, dest):

		rc =  os.system("sshpass -p " + self.password + " scp " + filename + " " + self.username + "@" + self.hostname + ":" + dest)
		if rc:
			print "ERROR: failed to copy %s to %s" %(filename, dest)
		
		return rc


#===============================================================================
# Setup Functions
#===============================================================================
g_setups_dir  = "/mswg/release/nps/solutions/ALVS/setups"
def get_setup_list(setup_num):
	setup_list = []
	
	# Open list file
	file_name = '%s/setup%d_vms.csv' %(g_setups_dir, setup_num)
	infile    = open(file_name, 'r')
	
	# Extract list from file  
	for line in infile:
		input_list = line.strip().split(',')
		if len(input_list) >= 2:
			setup_list.append({'hostname':input_list[0],'ip':input_list[1]})
	
	return setup_list

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
							'nps_port_type':       input_list[8]})
	
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

def check_connections(ezbox):
    print 'connection count = %d'  %ezbox.get_num_of_connections()
    pass

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
	
def compile(clean=True, debug=False):
	print
	if clean:
		os.system("make clean")
	if debug:
		output = os.system("make DEBUG=yes")
	else:
		output = os.system("make")
		
	if int(output) != 0:
		print "ERROR while compiling"
		exit(1)
		
	print
	print "Compilation Passed"
	print
	

