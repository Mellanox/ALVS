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

from pexpect import pxssh
import pexpect
from ftplib import FTP

# pythons modules 
from test_infra import *
from unittest2 import result
from test.test_zipimport import TEMP_ZIP

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

#===============================================================================
# STATS DEFINES
#===============================================================================
ALVS_SERVICES_MAX_ENTRIES	= 256
ALVS_SERVERS_MAX_ENTRIES	 = ALVS_SERVICES_MAX_ENTRIES*1024
ALVS_NUM_OF_SERVICE_STATS	= 6
ALVS_NUM_OF_SERVER_STATS	 = 8
ALVS_NUM_OF_SERVER_STATS	 = 8
NUM_OF_INTERFACES			= 5
NW_NUM_OF_IF_STATS 			 = 10
ALVS_NUM_OF_ALVS_ERROR_STATS = 20

EMEM_ALVS_ERROR_STATS_POSTED_OFFSET = 0
EMEM_SERVICE_STATS_POSTED_OFFSET = EMEM_ALVS_ERROR_STATS_POSTED_OFFSET + ALVS_NUM_OF_ALVS_ERROR_STATS
EMEM_SERVER_STATS_POSTED_OFFSET  = EMEM_SERVICE_STATS_POSTED_OFFSET + ALVS_SERVICES_MAX_ENTRIES * ALVS_NUM_OF_SERVICE_STATS
EMEM_IF_STATS_POSTED_OFFSET	  = EMEM_SERVER_STATS_POSTED_OFFSET + ALVS_SERVERS_MAX_ENTRIES * ALVS_NUM_OF_SERVER_STATS
EMEM_END_OF_STATS_POSTED		  = EMEM_IF_STATS_POSTED_OFFSET + NW_NUM_OF_IF_STATS*NUM_OF_INTERFACES


NW_IF_STATS_FRAME_VALIDATION_FAIL   = 0,
NW_IF_STATS_MAC_ERROR			   = 1,
NW_IF_STATS_IPV4_ERROR			  = 2,
NW_IF_STATS_NOT_MY_MAC			  = 3,
NW_IF_STATS_NOT_IPV4				= 4,
NW_IF_STATS_NOT_UDP_OR_TCP		  = 5,
NW_IF_STATS_NO_VALID_ROUTE		  = 6,
NW_IF_STATS_FAIL_ARP_LOOKUP		 = 7,
NW_IF_STATS_FAIL_INTERFACE_LOOKUP   = 8,
NW_NUM_OF_IF_STATS				  = 10
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

	def reset_ezbox(self):
		os.system("/.autodirect/LIT/SCRIPTS/rreboot " + self.setup['name'])
		os.system("verification/testing/wait_for_connection.sh " + self.setup['chip'])

	def execute_command_on_chip(self, cmd, sleep_time):
		os.system("{ echo \"%s\"; sleep %d; } | telnet %s" %(cmd, sleep_time, self.setup['chip']))

	def modify_run_cpus(self, use_4k_cpus=True):
		if use_4k_cpus:
			cpus = "0,16-4095"
			print "Config CPUs: %s" %cpus
		else:
			cpus = "0,16-511"
			print "Config CPUs: %s" %cpus

		self.execute_command_on_chip("fw_setenv krn_possible_cpus %s" %cpus, 1)
		self.execute_command_on_chip("fw_setenv krn_present_cpus %s" %cpus, 1)

	def clean(self, use_director=False):
		if use_director:
			self.clean_director()
		self.terminate_cp_app()
		self.logout()
		
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

	def connect(self):
		self.ssh_object.connect()
		self.run_app_ssh.connect()
		self.syslog_ssh.connect()
		self.syslog_ssh.execute_command("> /var/log/syslog")
		self.syslog_ssh.execute_command('tail -f /var/log/syslog | grep ALVS', False)
		 
	def logout(self):
		self.ssh_object.logout()
		self.run_app_ssh.logout()
		#self.syslog_ssh.logout()

	def execute_command_on_host(self, cmd):
		return self.ssh_object.execute_command(cmd)
	
	def copy_binaries(self, cp_bin, dp_bin=None):

		self.copy_file_to_host(cp_bin, "/tmp/cp_bin")

		if dp_bin!=None:
			self.copy_dp_bin(dp_bin)		
		
	def copy_file_to_host(self, filename, dest):
		os.system("sshpass -p " + self.setup['password'] + " scp " + filename + " " + self.setup['username'] + "@" + self.setup['host'] + ":" + dest)

	def copy_cp_bin(self, cp_bin):
		os.system("sshpass -p " + self.setup['password'] + " scp " + cp_bin + " " + self.setup['username'] + "@" + self.setup['host'] + ":/tmp/cp_bin")
	
	def copy_dp_bin(self, dp_bin):
		for i in range(10):
			try:
				ftp=FTP(self.setup['chip'])
				break
			except:
				print "Fail to connect to chip, trying again"
				time.sleep(5)
				i += 1
		ftp.login()
		ftp.storbinary("STOR /tmp/dp_bin", open(dp_bin, 'rb'))
		ftp.quit()
		os.system("{ echo \"chmod +x tmp/dp_bin\"; sleep 1; } | telnet " + self.setup['chip'])
					

	def reset_chip(self):
		self.run_app_ssh.execute_command("bsp_nps_power -r por")

	def run_dp(self, args=''):
		print "Run DP Bin"
		os.system("{ echo \"./tmp/dp_bin %s &\"; sleep 3; } | telnet %s"%(args,self.setup['chip']))
		print
		
	def run_cp(self):
		self.run_app_ssh.execute_command("/tmp/cp_bin --agt_enabled")
					
	def wait_for_cp_app(self):
		output = self.syslog_ssh.wait_for_msgs(['alvs_db_manager_poll...','Shut down ALVS daemon'])
		if output == 0:
			return True
		elif output == 1:
			return False
		elif output < 0:
			print 'wait_for_cp_app: Error... (end of output)'
			return False
		else:
			print 'wait_for_cp_app: Error... (Unknown output)'
			return False
		 
	def get_cp_app_pid(self):
		retcode, output = self.ssh_object.execute_command("pidof cp_bin")		 
		if output == '':
			return None
		else:
			return output.split(' ')[0]
			
	def terminate_cp_app(self):
		
		retries = 100 
		
		pid = self.get_cp_app_pid()
		while pid != None:
			if not (retries > 0):
				print sys._getframe().f_code.co_name + " failed. Try to reset ezbox"
				exit(1)
			retcodde, output = self.run_app_ssh.execute_command("kill " + pid)
			time.sleep(1)
			pid = self.get_cp_app_pid()
			retries -= 1
			
		 
	def capture_packets(self, tcpdump_params = None):
		if tcpdump_params == None:
			tcpdump_params = 'dst' + self.setup['host']

		self.ssh_object.execute_command('tcpdump -w /tmp/dump.pcap -i ' + self.setup['interface'] + ' ' + tcpdump_params + ' &')

	def stop_capture(self):
		self.ssh_object.execute_command('pkill -HUP -f tcpdump')
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
				   'server_count' : int(''.join(info_res[2:4]), 16),
				   'stats_base' : int(''.join(info_res[8:12]), 16),
				   'flags' : int(''.join(info_res[12:16]), 16),
				   'sched_info' : sched_info
				   }

		return service

	def get_connection(self, vip, vport, cip, cport, protocol):
		print "key: " + "%08x%08x%04x%04x%04x" % (int(cip), int(vip), int(cport), int(vport), int(protocol)) 
		class_res = str(self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_CONN_CLASSIFICATION, 0, {'key' : "%08x%08x%04x%04x%04x" % (int(cip), int(vip), int(cport), int(vport), int(protocol))}).result["params"]["entry"]["result"]).split(' ')
		print class_res
		if (int(class_res[0], 16) >> 4) != 3:
			return None
		info_res = str(self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_CONN_INFO, 0, {'key' : ''.join(class_res[4:8])}).result["params"]["entry"]["result"]).split(' ')
		if (int(info_res[0], 16) >> 4) != 3:
			print 'This should not happen!'
			return None

		conn = {'sync_bit' : (int(info_res[0], 16) >> 3) & 0x1,
				'aging_bit' : (int(info_res[0], 16) >> 2) & 0x1,
				'delete_bit' : (int(info_res[0], 16) >> 1) & 0x1,
				'reset_bit' : int(info_res[0], 16) & 0x1,
				'age_iteration' : int(info_res[1], 16),
				'server_index' : int(''.join(info_res[2:4]), 16),
				'state' : int(info_res[5], 16),
				'stats_base' : int(''.join(info_res[8:12]), 16),
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
		time.sleep(0.5)

	def modify_service(self, vip, port, sched_alg='sh', sched_alg_opt='-b sh-port'):
		self.execute_command_on_host("ipvsadm -E -t %s:%s -s %s %s"%(vip,port, sched_alg, sched_alg_opt))
		time.sleep(0.5)

	def delete_service(self, vip, port):
		self.execute_command_on_host("ipvsadm -D -t %s:%s"%(vip,port))
		time.sleep(0.5)

	def add_server(self, vip, service_port, server_ip, server_port, weight=1, routing_alg_opt=' '):
		self.execute_command_on_host("ipvsadm -a -t %s:%s -r %s:%s -w %d %s"%(vip, service_port, server_ip, server_port, weight, routing_alg_opt))
		time.sleep(0.5)

	def modify_server(self, vip, service_port, server_ip, server_port, weight=1, routing_alg_opt=' '):
		self.execute_command_on_host("ipvsadm -e -t %s:%s -r %s:%s -w %d %s"%(vip, service_port, server_ip, server_port, weight, routing_alg_opt))
		time.sleep(0.5)

	def delete_server(self, vip, service_port, server_ip, server_port):
		self.execute_command_on_host("ipvsadm -d -t %s:%s -r %s:%s"%(vip, service_port, server_ip, server_port))
		time.sleep(0.5)

	def flush_ipvs(self):
		self.execute_command_on_host("ipvsadm -C")
		time.sleep(0.5)

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
	 
	def get_error_stats(self, error_id):
		if error_id < EMEM_ALVS_ERROR_STATS_POSTED_OFFSET or error_id > EMEM_SERVICE_STATS_POSTED_OFFSET:
			return -1
		temp_result = self.cpe.cp.stat.get_posted_counters(channel_id=0, partition=0, range=True, start_counter=error_id, num_counters=1, double_operation=True)
		return temp_result.result['posted_counter_config']['counters']

	def get_services_stats(self, service_id):
		if service_id < EMEM_SERVICE_STATS_POSTED_OFFSET or service_id > EMEM_SERVER_STATS_POSTED_OFFSET:
			return -1
		temp_result = self.cpe.cp.stat.get_posted_counters(channel_id=0, partition=0, range=True, start_counter=service_id, num_counters=1, double_operation=True)
		return temp_result.result['posted_counter_config']['counters']

	def get_servers_stats(self, service_id):
		if service_id < EMEM_SERVER_STATS_POSTED_OFFSET or service_id > EMEM_IF_STATS_POSTED_OFFSET:
			return -1
		temp_result = self.cpe.cp.stat.get_posted_counters(channel_id=0, partition=0, range=True, start_counter=service_id, num_counters=1, double_operation=True)
		return temp_result.result['posted_counter_config']['counters'] 
  
	def get_interface_stats(self, interface_id):
		if interface_id < 0 or interface_id > NUM_OF_INTERFACES:
			return -1
		   
		offset_counter = EMEM_IF_STATS_POSTED_OFFSET + interface_id*NW_NUM_OF_IF_STATS
		temp_result = self.cpe.cp.stat.get_posted_counters(channel_id=0, partition=0, range=True, start_counter=offset_counter, num_counters=1, double_operation=True)
		return temp_result.result['posted_counter_config']['counters'] 



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
		print "Connected"
		
	def logout(self):
		self.ssh_object.logout()

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
		
				if exit_code != None:
					return [True, output]
				else:
					return [False, output]
			else:
				return [True, '']
		except:
			return [False, "command failed: " + cmd]

	def wait_for_msgs(self, msgs):
		while (True):
			index = self.ssh_object.expect_exact([pexpect.EOF, pexpect.TIMEOUT] + msgs)
			if index == 0:
				print self.ssh_object.before + self.ssh_object.match
				return -1
			elif index == 1:
				print self.ssh_object.before
			else:
				return (index-2)

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
 
#===============================================================================
# Setup Functions
#===============================================================================

def get_setup_list(setup_num):
	setup_list = []
	current_dir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
	infile = open(current_dir+'/../setups/setup%d.csv'%setup_num, 'r')
	for line in infile:
		input_list = line.strip().split(',')
		if len(input_list) >= 2:
			setup_list.append({'hostname':input_list[0],'ip':input_list[1]})
	
	return setup_list

def get_ezbox_names(setup_id):
	setup_dict = {1:{'name':'ezbox29', 'host':'ezbox29-host', 'chip':'ezbox29-chip', 'interface':'eth0.6', 'username':'root', 'password':'ezchip', 'data_ip_hex_display': 'C0 A8 00 01', 'mac_address': '00 19 0f 25 68 aa'},
				  2:{'name':'ezbox24', 'host':'ezbox24-host', 'chip':'ezbox24-chip', 'interface':'eth0.5', 'username':'root', 'password':'ezchip', 'data_ip_hex_display': 'C0 A8 00 02', 'mac_address': '00 19 0f 25 da 7a'},
				  3:{'name':'ezbox35', 'host':'ezbox35-host', 'chip':'ezbox35-chip', 'interface':'eth0.7', 'username':'root', 'password':'ezchip', 'data_ip_hex_display': 'C0 A8 00 03', 'mac_address': '00 19 0f 27 23 2d'},
				  4:{'name':'ezbox55', 'host':'ezbox55-host', 'chip':'ezbox55-chip', 'interface':'eth0.8', 'username':'root', 'password':'ezchip', 'data_ip_hex_display': 'C0 A8 00 04', 'mac_address': '00 19 0f 27 23 16'}}
	
	return setup_dict[setup_id]

def get_setup_vip_list(setup_id):
	setup_dict = {1:['192.168.1.x', '192.168.11.x'],
				  2:['192.168.2.x', '192.168.12.x'],
				  3:['192.168.3.x', '192.168.13.x'],
				  4:['192.168.4.x', '192.168.14.x']}
	
	return setup_dict[setup_id]

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
