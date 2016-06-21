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

from pexpect import pxssh
import pexpect
from ftplib import FTP

# pythons modules 
from test_infra import *


#===============================================================================
# Struct IDs
#===============================================================================
STRUCT_ID_NW_INTERFACES                = 0
STRUCT_ID_NW_LAG                       = 1
STRUCT_ID_ALVS_CONN_CLASSIFICATION     = 2
STRUCT_ID_ALVS_CONN_INFO               = 3
STRUCT_ID_ALVS_SERVICE_CLASSIFICATION  = 4
STRUCT_ID_ALVS_SERVICE_INFO            = 5
STRUCT_ID_ALVS_SCHED_INFO              = 6
STRUCT_ID_ALVS_SERVER_INFO             = 7
STRUCT_ID_NW_FIB                       = 8
STRUCT_ID_NW_ARP                       = 9


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
    		for server in services[vip]:
    			outfile.write('	real=%s:80 gate\n'%server)
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
            self.execute_command_on_host("ifconfig %s:%d %s netmask 255.255.0.0"%(self.setup['interface'], index+1, vip))

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
        if dp_bin != None:
            ftp=FTP(self.setup['chip'])
            ftp.login()
            ftp.storbinary("STOR /tmp/dp_bin", open(dp_bin, 'rb'))
            ftp.quit()
            os.system("{ echo \"chmod +x tmp/dp_bin\"; sleep 1; } | telnet " + self.setup['chip'])
        
    def copy_file_to_host(self, filename, dest):
        os.system("sshpass -p " + self.setup['password'] + " scp " + filename + " " + self.setup['username'] + "@" + self.setup['host'] + ":" + dest)

    def reset_chip(self):
        self.run_app_ssh.execute_command("bsp_nps_power -r por")

    def run_dp(self, args=''):
        os.system("{ echo \"./tmp/dp_bin %s &\"; sleep 3; } | telnet %s" %(args, self.setup['chip']))

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
        pid = self.get_cp_app_pid()
        while pid != None:
        	retcodde, output = self.run_app_ssh.execute_command("kill " + pid)
        	time.sleep(1)
        	pid = self.get_cp_app_pid()

         
    def record_host_traffic(self, tcpdump_params = ''):
        self.ssh_object.execute_command('tcpdump -G 10 -w /tmp/dump.pcap -n -i ' + self.setup['interface'] + ' ' + tcpdump_params + ' dst ' + self.setup['host'] + ' & sleep 3s && pkill -HUP -f tcpdump')

    def get_num_of_services(self):
        return self.cpe.cp.struct.get_num_entries(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION, channel_id = 0).result['num_entries']['number_of_entries']

    def get_num_of_connections(self):
        return self.cpe.cp.struct.get_num_entries(STRUCT_ID_ALVS_CONN_CLASSIFICATION, channel_id = 0).result['num_entries']['number_of_entries']
    
    def get_service(self, vip, port, protocol):
        class_res = str(self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION, 0, {'key' : "%08x%04x%04x" % (vip, port, protocol)}).result["params"]["entry"]["result"]).split(' ')
        if (int(class_res[0], 16) >> 4) != 3:
            return None
        info_res = str(self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_SERVICE_INFO, 0, {'key' : class_res[2]}).result["params"]["entry"]["result"]).split(' ')
        if (int(info_res[0], 16) >> 4) != 3:
            print 'This should not happen!'
            return None
        
        sched_info = []
        for ind in range(256):
            sched_index = int(class_res[2], 16) * 256 + ind
            sched_res = str(self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_SCHED_INFO, 0, {'key' : "%04x" % sched_index}).result["params"]["entry"]["result"]).split(' ')
            if (int(sched_res[0], 16) >> 4) == 3:
                sched_info[ind] = int(''.join(sched_res[4:8]), 16)
        
        service = {'sched_alg' : int(info_res[0], 16) & 0xf,
                   'server_count' : int(''.join(res[2:4]), 16),
                   'stats_base' : int(''.join(res[8:12]), 16),
                   'flags' : int(''.join(res[12:16]), 16),
                   'sched_info' : sched_info
                   }
        
        return service
    
    def get_connection(self, vip, vport, cip, cport, protocol):
        class_res = str(self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_CONN_CLASSIFICATION, 0, {'key' : "%08x%08x%04x%04x%04x" % (cip, vip, cport, vport, protocol)}).result["params"]["entry"]["result"]).split(' ')
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
        interface = {'oper_status' : (int(info_res[0], 16) >> 3) & 0x1,
                     'path_type' : (int(info_res[0], 16) >> 1) & 0x3,
                     'is_vlan' : int(info_res[0], 16) & 0x1,
                     'lag_id' : int(info_res[1], 16),
                     'default_vlan' : int(''.join(info_res[2:4]), 16),
                     'mac_address' : int(''.join(info_res[4:10]), 16),
                     'output_channel' : int(info_res[10], 16),
                     'stats_base' : int(''.join(info_res[12:16]), 16),
                     }
        
        return interface

    def get_all_arp_entries(self):
        iterator_params_dict = (self.cpe.cp.struct.iterator_create(STRUCT_ID_NW_ARP, { 'channel_id': 0 })).result['iterator_params']
        num_entries = (self.cpe.cp.struct.get_num_entries(STRUCT_ID_NW_ARP, channel_id = 0)).result['num_entries']['number_of_entries']

        entries = []
        for i in range(0, num_entries):
            iterator_params_dict = self.cpe.cp.struct.iterator_get_next(STRUCT_ID_NW_ARP, iterator_params_dict).result['iterator_params']
            
            key = str(iterator_params_dict['entry']['key']).split(' ')
            ip = int(''.join(key[0:4]), 16)
            
            result = str(iterator_params_dict['entry']['result']).split(' ')
            entry = {'mac' : int(result[4:], 16),
                     'key' : {'ip' : ip}
                     }
            
            entries.append(entry)

        self.cpe.cp.struct.iterator_delete(STRUCT_ID_NW_ARP, iterator_params_dict)             
        return entries

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
                       'server_count' : int(''.join(res[2:4]), 16),
                       'stats_base' : int(''.join(res[8:12]), 16),
                       'flags' : int(''.join(res[12:16]), 16),
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
	
    def modify_service(self, vip, port, sched_alg='sh', sched_alg_opt='-b sh-port'):
		self.execute_command_on_host("ipvsadm -E -t %s:%s -s %s %s"%(vip,port, sched_alg, sched_alg_opt))
	
    def delete_service(self, vip, port):
		self.execute_command_on_host("ipvsadm -D -t %s:%s"%(vip,port))
		
    def add_server(self, vip, service_port, server_ip, server_port, weight=1, routing_alg_opt=' '):
		self.execute_command_on_host("ipvsadm -a -t %s:%s -r %s:%s -w %d %s"%(vip, service_port, server_ip, server_port, weight, routing_alg_opt))
	
    def modify_server(self, vip, service_port, server_ip, server_port, weight=1, routing_alg_opt=' '):
		self.execute_command_on_host("ipvsadm -e -t %s:%s -r %s:%s -w %d %s"%(vip, service_port, server_ip, server_port, weight, routing_alg_opt))
	
    def delete_server(self, vip, service_port, server_ip, server_port, weight=1, routing_alg_opt=' '):
		self.execute_command_on_host("ipvsadm -d -t %s:%s -r %s:%s"%(vip, service_port, server_ip, server_port))
	
    def flush_ipvs(self):
		self.execute_command_on_host("ipvsadm -C")


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
	# brief:     exucute commnad on SSH object
	#
	# in params:     cmd - coommnad string for execute
	#            
	# return params: success - True for success, False for Error
	#                output -  Command output results     
	def execute_command(self, cmd, wait_prompt=True):
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
			exec_cmd    = "cd " + self.exe_path + "; " + exe_prog + " " + self.exe_script
			cmd = sshpass_cmd + " \"" + exec_cmd + " " + self.exec_params + "\""
			print cmd
			os.system(cmd)

	def connect(self):
		self.ssh.connect()
		
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
	setup_dict = {1:{'name':'ezbox29', 'host':'ezbox29-host', 'chip':'ezbox29-chip', 'interface':'eth0.6', 'username':'root', 'password':'ezchip'},
				  2:{'name':'ezbox24', 'host':'ezbox24-host', 'chip':'ezbox24-chip', 'interface':'eth0.5', 'username':'root', 'password':'ezchip'},
				  3:{'name':'ezbox35', 'host':'ezbox35-host', 'chip':'ezbox35-chip', 'interface':'eth0.7', 'username':'root', 'password':'ezchip'},
				  4:{'name':'ezbox55', 'host':'ezbox55-host', 'chip':'ezbox55-chip', 'interface':'eth0.8', 'username':'root', 'password':'ezchip'}}
	
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
