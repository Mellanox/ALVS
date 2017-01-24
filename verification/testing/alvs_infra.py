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
import urllib2
from time import gmtime, strftime
from os import listdir
from os.path import isfile, join

from common_infra import *
from network_interface_enum import *
from pexpect import pxssh
import pexpect

from multiprocessing import Process

#===============================================================================
# Struct IDs
#===============================================================================
STRUCT_ID_ALVS_CONN_CLASSIFICATION			= 4
STRUCT_ID_ALVS_CONN_INFO 					= 5
STRUCT_ID_ALVS_SERVICE_CLASSIFICATION		= 6
STRUCT_ID_ALVS_SERVICE_INFO					= 7
STRUCT_ID_ALVS_SCHED_INFO					= 8
STRUCT_ID_ALVS_SERVER_INFO					= 9
STRUCT_ID_ALVS_SERVER_CLASSIFICATION		= 10
STRUCT_ID_APPLICATION_INFO					= 11

#===============================================================================
# STATS DEFINES
#===============================================================================
ALVS_SERVICES_MAX_ENTRIES	 = 256
ALVS_SERVERS_MAX_ENTRIES	 = ALVS_SERVICES_MAX_ENTRIES*1024
ALVS_NUM_OF_SERVICE_STATS	 = 6
ALVS_NUM_OF_SERVER_STATS	 = 8
NUM_OF_NW_INTERFACES		 = 4
NUM_OF_REMOTE_INTERFACES	 = 4
ALVS_NUM_OF_SERVERS_ON_DEMAND_STATS = 1
NW_NUM_OF_IF_STATS			 = 20
ALVS_NUM_OF_ALVS_ERROR_STATS = 40


NW_IF_STATS_POSTED_OFFSET = 0
REMOTE_IF_STATS_POSTED_OFFSET = (NW_IF_STATS_POSTED_OFFSET + (NUM_OF_NW_INTERFACES * NW_NUM_OF_IF_STATS))
HOST_IF_STATS_POSTED_OFFSET = (REMOTE_IF_STATS_POSTED_OFFSET + (NUM_OF_REMOTE_INTERFACES * NW_NUM_OF_IF_STATS))


ALVS_ERROR_STATS_POSTED_OFFSET = (HOST_IF_STATS_POSTED_OFFSET + (1 * NW_NUM_OF_IF_STATS))
ALVS_SERVICE_STATS_POSTED_OFFSET = (ALVS_ERROR_STATS_POSTED_OFFSET + ALVS_NUM_OF_ALVS_ERROR_STATS)
ALVS_SERVER_STATS_POSTED_OFFSET = (ALVS_SERVICE_STATS_POSTED_OFFSET + ALVS_SERVICES_MAX_ENTRIES * ALVS_NUM_OF_SERVICE_STATS)

ALVS_SERVER_STATS_ON_DEMAND_OFFSET = 2


#===============================================================================
# Classes
#===============================================================================

class alvs_ezbox(ezbox_host):
	
	def __init__(self, setup_id):
		# init parent class
		super(alvs_ezbox, self).__init__(setup_id)
		self.install_path = "alvs_install"
	
	def clean(self, use_director=False, stop_ezbox=False):
		self.zero_all_ipvs_stats()
		if use_director:
			self.clean_keepalived()
			
		self.flush_ipvs()
		
		if stop_ezbox:
			self.service_stop()
		
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
						print "error while figure out genhash value, server %s, trying again.."%server
						if i==10:
							print "ERROR"
							raise
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
					name = (vip_if.split(':'))[1]
					if name != 'nps_if':
						self.execute_command_on_host("ifconfig %s down"%(vip_if))

	def get_version(self):
		cmd = "grep -e \"\\\"\\$Revision: .* $\\\"\" "+ALVSdir+"/src/common/version.h"
		cmd += " | cut -d\":\" -f 2 | cut -d\" \" -f2 | cut -d\".\" -f1-2| uniq"
		version = os.popen(cmd).read().strip()
		version += ".0000" 
		
		return version
	
	def copy_and_install_package(self, alvs_package=None):
		if alvs_package is None:
			# get package name
			alvs_package = "alvs_*_amd64.deb"
		self.copy_package(alvs_package)
		self.install_package(alvs_package)

	def alvs_service(self, cmd):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called for " + cmd + " service"
		self.run_app_ssh.execute_command("service alvs " + cmd)

	def service_start(self):
		self.run_app_ssh.execute_command("cat /dev/null > /var/log/syslog")
		self.syslog_ssh.wait_for_msgs(['file truncated'])
		self.alvs_service("start")

	def service_stop(self):
		self.alvs_service("stop")

	def service_restart(self):
		self.alvs_service("restart")

	def get_num_of_services(self):
		return self.cpe.cp.struct.get_num_entries(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION, channel_id = 0).result['num_entries']['number_of_entries']

	
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
	
	def get_server(self, vip, service_port, server_ip, server_port, protocol):
		class_res = self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_SERVER_CLASSIFICATION, 0, {'key' : "%08x%08x%04x%04x%04x" % (ip2int(server_ip), ip2int(vip), int(server_port), int(service_port), protocol)})
		warning_code=class_res.result.get('warning_code',None)
		if warning_code != None :
			return None
		class_res = class_res.result["params"]["entry"]["result"].split(' ')
		info_res = self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_SERVER_INFO, 0, {'key' : ''.join(class_res[4:8])})
		warning_code=info_res.result.get('warning_code',None)
		if warning_code != None :
			return None
		info_res = info_res.result["params"]["entry"]["result"].split(' ')

		server = {'index' : int(''.join(class_res[4:8]), 16),
				  'u_thresh' : int(''.join(info_res[20:22]), 16),
				  'l_thresh' : int(''.join(info_res[22:24]), 16)
				 }

		return server

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

	def get_num_of_connections(self):
		return self.cpe.cp.struct.get_num_entries(STRUCT_ID_ALVS_CONN_CLASSIFICATION, channel_id = 0).result['num_entries']['number_of_entries']

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
		time.sleep(0.5)

	def modify_service(self, vip, port, sched_alg='sh', sched_alg_opt='-b sh-port'):
		self.execute_command_on_host("ipvsadm -E -t %s:%s -s %s %s"%(vip,port, sched_alg, sched_alg_opt))
		time.sleep(0.5)

	def delete_service(self, vip, port):
		self.execute_command_on_host("ipvsadm -D -t %s:%s"%(vip,port))
		time.sleep(0.5)

	def add_server(self, vip, service_port, server_ip, server_port, weight=1, routing_alg_opt=' ', u_thresh = 0, l_thresh = 0):
		self.execute_command_on_host("ipvsadm -a -t %s:%s -r %s:%s -w %d %s -x %d -y %d"%(vip, service_port, server_ip, server_port, weight, routing_alg_opt, u_thresh, l_thresh))
		time.sleep(0.5)

	def modify_server(self, vip, service_port, server_ip, server_port, weight=1, routing_alg_opt=' ', u_thresh = 0, l_thresh = 0):
		self.execute_command_on_host("ipvsadm -e -t %s:%s -r %s:%s -w %d %s -x %d -y %d"%(vip, service_port, server_ip, server_port, weight, routing_alg_opt, u_thresh, l_thresh))
		time.sleep(0.5)

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
			print "ERROR, failed to execute ipvs list command"
			print output
			return False
		
		return True

	def clear_stats(self):
		error_stats = self.cpe.cp.stat.set_posted_counters(channel_id=0, partition=0, range=True, start_counter=ALVS_ERROR_STATS_POSTED_OFFSET, num_counters=ALVS_NUM_OF_ALVS_ERROR_STATS, double_operation=False, clear=True)
		
	def get_error_stats(self):
		error_stats = self.cpe.cp.stat.get_posted_counters(channel_id=0, partition=0, range=True, start_counter=ALVS_ERROR_STATS_POSTED_OFFSET, num_counters=ALVS_NUM_OF_ALVS_ERROR_STATS, double_operation=False).result['posted_counter_config']['counters']
		
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
		
		counter_offset = ALVS_SERVICE_STATS_POSTED_OFFSET + service_id*ALVS_NUM_OF_SERVICE_STATS
		
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
		
		counter_offset = ALVS_SERVER_STATS_POSTED_OFFSET + server_id*ALVS_NUM_OF_SERVER_STATS
		
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
		
		counter_offset = ALVS_SERVER_STATS_ON_DEMAND_OFFSET + server_id * ALVS_NUM_OF_SERVERS_ON_DEMAND_STATS
		sched_connections_on_server =  self.cpe.cp.stat.get_long_counter_values(channel_id=0, 
																				partition=0, 
																				range=1, 
		                                             							start_counter=counter_offset, 
		                                             	    					num_counters=ALVS_NUM_OF_SERVERS_ON_DEMAND_STATS, 
		                                             		  	     			read=0, 
		                                             		  	     			use_shadow_group=0).result['long_counter_config']['counters']

		
		return sched_connections_on_server[0]['value']

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

	
	def read_stats_on_syslog(self):
		time.sleep(3)
		os.system("sshpass -p ezchip scp root@%s:/var/log/syslog temp_syslog"%(self.setup['host']))
		time.sleep(3)
		syslog_lines = [line.rstrip('\n') for line in open('temp_syslog')]
		interface_stats = {}
		global_syslog_stats = {}
		all_servers_stats = {}
		service_stats = {}
				
		for index,line in enumerate(syslog_lines):
			
			if 'Statistics of global errors' in line:
				# create initilized stats dictinary 
				interface_stats = {}
				global_syslog_stats = {}
				all_servers_stats = {}
				service_stats = {}

				for i in range(index+1,index+ALVS_NUM_OF_ALVS_ERROR_STATS+1):
					if i >= len(syslog_lines):
						break
					temp = syslog_lines[i].find('<info>      ')
					if temp>0 and "No Errors On Global Counters" not in syslog_lines[i]:
						try:
							global_syslog_stats[syslog_lines[i].split()[6]] = int(syslog_lines[i].split()[8])
						except:
							print "error parsing global stats syslog message"
							pass
					else:
						break

			if 'Statistics of Network Interface' in line or 'Statistics of Host Interface' in line:
				if 'Statistics of Host Interface' in line:
					interface_num = 'host'
				else:
					interface_num = int(line.split()[line.split().index('Interface')+1])
					
				# create and initialize interface stats dictionary
				if interface_num == 'host':
					interface = 4
				else:
					interface = int(interface_num)
					
				for i in range(index+1,index+100):
					if i >= len(syslog_lines):
						break
					if "Statistics of Network Interface" in syslog_lines[i] or "Statistics of Host Interface" in syslog_lines[i]:
						break 
					interface_stats[interface_num] = {}
					temp = syslog_lines[i].find('<info>      ')
					if temp>0 and "No Errors On Counters" not in syslog_lines[i]:
						try:
							interface_stats[interface_num][syslog_lines[i].split()[6]] = int(syslog_lines[i].split()[8])
						except:
							print "error parsing interface stats syslog message"
							pass
					else:
						break
					
			if 'Statistics for Services' in line:
				service_stats = {}
				for i in range(index+2,index+258):
					if i >= len(syslog_lines):
						break
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
				
			if 'Statistics for Servers' in line:
				server_index = 0
				service = line.split()[11]
				all_servers_stats[service] = {}
				for i in range(index+2,index+258):
					if i>=len(syslog_lines):
						break
					try:
						line_split = syslog_lines[i].split()
						if 'Server' in line_split[6] and ':80' in line_split[7]:
							server = line_split[7]
							server_stats = {}
							server_stats['IN_PACKETS'] = int(line_split[8])
							server_stats['IN_BYTES'] = int(line_split[10])
							server_stats['OUT_PACKETS'] = int(line_split[12])
							server_stats['OUT_BYTES'] = int(line_split[14])
							server_stats['CONN_SCHED'] = int(line_split[16])
							server_stats['CONN_TOTAL'] = int(line_split[18])
							server_stats['ACTIVE_CONN'] = int(line_split[20])
							server_stats['INACTIVE_CONN'] = int(line_split[22])
							all_servers_stats[service][server] = server_stats
							
						else:
							break
					except:
						print "Error parsing servers stats"
						pass	
				
		os.system("rm -f temp_syslog")
		
		return {'interface_stats':interface_stats,
				'global_stats':global_syslog_stats,
				'server_stats':all_servers_stats,
				'service_stats':service_stats}		


class HttpServer(player):
	def __init__(self, ip, hostname, all_eths,
				username    = "root",
				password    = "3tango",
				exe_path    = None,
				exe_script  = None,
				exec_params = None,
				vip         = None,
				interface   = Network_Interface.REGULAR,
				net_mask    = "255.255.255.255",
				dump_pcap_file = '/tmp/server_dump.pcap',
				weight      = 1,
				u_thresh    = 0,
				l_thresh    = 0):
		# init parent class player
		super(HttpServer, self).__init__(ip, hostname, username, password, interface,
										 all_eths, exe_path, exe_script, exec_params, dump_pcap_file)
		# Init class variables
		self.net_mask = net_mask
		self.vip = vip
		self.weight = weight
		self.u_thresh = u_thresh
		self.l_thresh = l_thresh
		self.mac_address = None
	def init_server(self, index_str):
		self.connect()
		self.config_interface()
		self.mac_address = self.get_mac_adress()
		self.clear_arp_table()
		self.start_http_daemon()
		self.configure_loopback()
		self.disable_arp()
		self.set_index_html(index_str)
		self.set_test_html()

	def clean_server(self, verbose = True):
		self.stop_http_daemon(verbose)
		self.take_down_loopback(verbose)
		self.enable_arp()
		self.delete_index_html(verbose)
		self.delete_test_html(verbose)
		self.logout()
		
	def start_http_daemon(self):
		if len(self.all_eths) > 1:
			rc, output = self.ssh.execute_command("service apache2 start")
		else:
			rc, output = self.ssh.execute_command("service httpd start")
		if rc != True:
			print "ERROR: Start HTTP daemon failed. rc=" + str(rc) + " " + output
			return
	
	def stop_http_daemon(self, verbose = True):
		if len(self.all_eths) > 1:
			rc, output = self.ssh.execute_command("service apache2 stop")
		else:
			rc, output = self.ssh.execute_command("service httpd stop")
		if rc != True and verbose:
			print "ERROR: Stop HTTP daemon failed. rc=" + str(rc) + " " + output

	def capture_packets_from_service(self, service_vip):
		params = ' tcpdump -w ' + self.dump_pcap_file + ' -n -i ' + self.eth + ' ether host ' + self.mac_address + ' and dst ' + service_vip + ' &'
		self.capture_packets(params)
	
	def update_vip(self,new_vip):
		self.take_down_loopback()
		self.vip= new_vip
		self.configure_loopback()
	
	def configure_loopback(self):
		cmd = "ifconfig lo:0 " + str(self.vip) + " netmask " + str(self.net_mask)
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: Configuting loopback failed. rc=" + str(rc) + " " + output
			return
	
	def take_down_loopback(self, verbose = True):
		cmd = "ifconfig lo:0 down"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True and verbose:
			print "ERROR: Take down loopback failed. rc=" + str(rc) + " " + output

	def disable_arp(self):
		# Disable IP forwarding
		rc, output = self.ssh.execute_command("echo \"0\" > /proc/sys/net/ipv4/ip_forward")
		if rc != True:
			print "ERROR: Disable IP forwarding failed. rc=" + str(rc) + " " + output
			return

		# Enable arp ignore
		rc, output = self.ssh.execute_command("echo \"1\" > /proc/sys/net/ipv4/conf/all/arp_ignore")
		if rc != True:
			print "ERROR: Enable IP forwarding failed. rc=" + str(rc) + " " + output
			return
		cmd = "echo \"1\" >/proc/sys/net/ipv4/conf/" + self.eth + "/arp_ignore"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: Enable arp ignore ("+ self.eth + ") failed. rc=" + str(rc) + " " + output
			return
		
		# Enable arp announce
		rc, output = self.ssh.execute_command("echo \"2\" > /proc/sys/net/ipv4/conf/all/arp_announce")
		if rc != True:
			print "ERROR: Enable arp announce (all) failed. rc=" + str(rc) + " " + output
			return
		cmd = "echo \"2\" > /proc/sys/net/ipv4/conf/" + self.eth + "/arp_announce"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: Enable arp announce ("+ self.eth + ") failed. rc=" + str(rc) + " " + output
			return


	def enable_arp(self):
		# Enable IP forwarding
		rc, output = self.ssh.execute_command("echo \"1\" >/proc/sys/net/ipv4/ip_forward")
		if rc != True:
			print "ERROR: Enable IP forwarding failed. rc=" + str(rc) + " " + output
			return

		# Disable arp ignore
		rc, output = self.ssh.execute_command("echo \"0\" > /proc/sys/net/ipv4/conf/all/arp_ignore")
		if rc != True:
			print "ERROR: Disable IP forwarding failed. rc=" + str(rc) + " " + output
			return
		cmd = "echo \"0\" >/proc/sys/net/ipv4/conf/" + self.eth + "/arp_ignore"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: Disable arp ignore (" + self.eth +") failed. rc=" + str(rc) + " " + output
			return
		
		# Disable arp announce
		rc, output = self.ssh.execute_command("echo \"0\" > /proc/sys/net/ipv4/conf/all/arp_announce")
		if rc != True:
			print "ERROR: Disable arp announce (all) failed. rc=" + str(rc) + " " + output
			return
		cmd = "echo \"0\" > /proc/sys/net/ipv4/conf/" + self.eth + "/arp_announce"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: Disable arp announce (" + self.eth + ") failed. rc=" + str(rc) + " " + output
			return


	def set_index_html(self, str):
		cmd = "echo " + str + " >/var/www/html/index.html"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: setting index.html failed. rc=" + str(rc) + " " + output
			return
		
		
	def set_large_index_html(self):
		# This command repeates index.html into a ~3MB file
		cmd = "for i in {1..18}; do cat /var/www/html/index.html /var/www/html/index.html > /var/www/html/index2.html && mv -f /var/www/html/index2.html /var/www/html/index.html; done"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: setting index.html failed. rc=" + str(rc) + " " + output
			return

	def set_extra_large_index_html(self):
		# This command repeates index.html into a ~52MB file
		cmd = "for i in {1..22}; do cat /var/www/html/index.html /var/www/html/index.html > /var/www/html/index2.html && mv -f /var/www/html/index2.html /var/www/html/index.html; done"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: setting index.html failed. rc=" + str(rc) + " " + output
			return
		
	def set_test_html(self):
		cmd = 'echo "Still alive" >/var/www/html/test.html'
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: setting test.html failed. rc=" + str(rc) + " " + output
			return

	def delete_index_html(self, verbose = True):
		cmd = "rm -f /var/www/html/index.html"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True and verbose:
			print "ERROR: deleting index.html failed. rc=" + str(rc) + " " + output

	def delete_test_html(self, verbose = True):
		cmd = "rm -f /var/www/html/test.html"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True and verbose:
			print "ERROR: deleting test.html failed. rc=" + str(rc) + " " + output


class HttpClient(player):
	def __init__(self, ip, hostname, all_eths,
				username    = "root",
				password    = "3tango",
				interface   = Network_Interface.REGULAR,
				exe_path    = "/root/tmp/",
				src_exe_path    = os.path.dirname(os.path.realpath(__file__))+"/system_level",
				exe_script  = "basic_client_requests.py",
				exec_params = ""):
		
		# init parent class player
		super(HttpClient, self).__init__(ip, hostname, username, password, interface, all_eths, exe_path, exe_script, exec_params)
		
		# Init class variables
		self.logfile_name  = '/root/client_%s.log'%ip
		self.exec_params  += ' -l %s' %self.logfile_name
		self.loglist       = [self.logfile_name]
		self.src_exe_path  = src_exe_path

	def init_client(self):
		self.connect()
		self.config_interface()
		self.mac_address = self.get_mac_adress()
		self.clear_arp_table()
		self.copy_exec_script()
		
	def send_packet_to_nps(self, pcap_file):
		logging.log(logging.DEBUG,"Send packet to NPS")
		cmd = "tcpreplay --intf1=" + self.eth + " " + pcap_file
		logging.log(logging.DEBUG,"run command on client:\n" + cmd) #todo
		result, output = self.execute_command(cmd)
		if result == False:
			print "Error while sending a packet to NPS"
			print output
			exit(1)

	def clean_client(self):
		self.remove_exec_script()
		self.logout()
	
	def add_log(self, new_log):
		self.logfile_name = new_log
		self.exec_params = self.exec_params[:self.exec_params.find('-l')] +'-l %s' %self.logfile_name 
		self.loglist.append(new_log)
		
	def remove_last_log(self):
		self.loglist.pop()
		self.logfile_name = self.loglist[-1]
		self.exec_params = self.exec_params[:self.exec_params.find('-l')] +'-l %s' %self.logfile_name 
		
	def get_log(self, dest_dir):
		for log in self.loglist:
			cmd = "sshpass -p %s scp -o StrictHostKeyChecking=no %s@%s:%s %s"%(self.password, self.username, self.hostname, log, dest_dir)
			os.system(cmd)
		
	def copy_exec_script(self):
		# create foledr
		cmd = "mkdir -p %s" %self.exe_path
		self.execute_command(cmd)
		
		# copy exec file
		filename = self.src_exe_path + "/" + self.exe_script
		self.copy_file_to_player(filename, self.exe_path)
		
	def remove_exec_script(self):
		cmd = "rm -rf %s/%s" %(self.exe_path, self.exe_script)
		self.execute_command(cmd)
	

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
					  help="Use ALVS package file (alvs_<version>.deb")
	parser.add_option("-f", "--install_file", dest="install_file",
					  help="installation file name (default=alvs_<version>_amd64.deb)")
	parser.add_option("-d", "--use_director", dest="use_director", choices=bool_choices,
					  help="Use director at host 								(default=True)")
	parser.add_option("--start", "--start_ezbox", dest="start_ezbox", choices=bool_choices,
					  help="Start the alvs service at the beginning of the test (defualt=False)")
	parser.add_option("--stop", "--stop_ezbox", dest="stop_ezbox", choices=bool_choices,
					  help="Stop the alvs service at the end of the test 		(defualt=False)")
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
		config['modify_run_cpus'] = bool_str_to_bool(options.modify_run_cpus)
	if options.install_package:
		config['install_package'] = bool_str_to_bool(options.install_package)
	if options.install_file:
		config['install_file']    = options.install_file
	if options.use_director:
		config['use_director']    = bool_str_to_bool(options.use_director)
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
		server = HttpServer(ip       = setup_list[next_vm_idx]['all_ips'][0],
						    hostname = setup_list[next_vm_idx]['hostname'],
						    all_eths  = setup_list[next_vm_idx]['all_eths'])
		server_list.append(server)
		next_vm_idx+=1
	
	# Create clients list
	client_list=[]
	for i in range(client_count):
		client = HttpClient(ip       = setup_list[next_vm_idx]['all_ips'][0],
						    hostname = setup_list[next_vm_idx]['hostname'],
						    all_eths = setup_list[next_vm_idx]['all_eths'])
		client_list.append(client)
		next_vm_idx+=1

	# get EZbox
	ezbox = alvs_ezbox(setup_num)
	
	
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
def alvs_bringup_env(ezbox, server_list, vip_list, use_director):
	ezbox.config_vips(vip_list)
	ezbox.flush_ipvs()
	
	# init director
	if use_director:
		print 'Start Director'
		services = dict((vip, []) for vip in vip_list )
		for server in server_list:
			services[server.vip].append((server.ip, server.weight))
		ezbox.init_keepalived(services)
		#wait for director
		time.sleep(15)
		#flush director configurations
		ezbox.flush_ipvs()

#------------------------------------------------------------------------------
def fill_default_config(test_config):
	
	# define default configuration 
	default_config = {'setup_num'       : None,  # supply by user
					  'modify_run_cpus' : True,  # in case modify_run_cpus is false, use_4k_cpus ignored
					  'use_4k_cpus'     : False,
					  'install_package' : True,
					  'install_file'    : None,
					  'use_director'    : True,
					  'stats'           : False,
					  'start_ezbox'		: False,
					  'stop_ezbox'		: False,
					  'remote_control'	: False,
					  'num_of_cpus': "512" }# 
	
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
	if test_config['start_ezbox']:
		ezbox_init_proccess = Process(target=test_resources['ezbox'].common_ezbox_bringup,
								  args=(test_config,))
	
		ezbox_init_proccess.start()
		#change_switch_lag_mode(test_config['setup_num'])
		
	# connect Ezbox (proccess work on ezbox copy and not on ezbox object
	test_resources['ezbox'].connect()
	
	for s in test_resources['server_list']:
		init_server(s)
	for c in test_resources['client_list']:
		init_client(c)
	
	# Wait for EZbox proccess to finish
	if test_config['start_ezbox']:
		ezbox_init_proccess.join()
	# Wait for all proccess to finish
		if ezbox_init_proccess.exitcode:
			print "ezbox_init_proccess failed. exit code " + str(ezbox_init_proccess.exitcode)
			exit(ezbox_init_proccess.exitcode)
	time.sleep(6)
	alvs_bringup_env(test_resources['ezbox'], test_resources['server_list'], test_resources['vip_list'], test_config['use_director'])

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
		s.ssh.recreate_ssh_object()
		process_list.append(Process(target=clean_server, args=(s,)))
		
	for c in test_resources['client_list']:
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
				
