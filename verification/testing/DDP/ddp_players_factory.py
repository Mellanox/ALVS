import sys
import os
import inspect
import time
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir)
from optparse import OptionParser
from multiprocessing import Process
from players_factory import *
from DDP_infra import *
from common_infra import *

class DDP_Players_Factory(Players_Factory):
	
	def __init__(self):
		super(DDP_Player_Factory, self).__init__()
			
	def clean_host(host):
		print "clean host: " ,  host.str()
		# reconnect with new object
		host.connect()
		host.clean_host()
	
	clean_host = staticmethod(clean_host)
	
	def init_host(host):
		print "init host: " , host.str()
		host.init_host()
	
	init_host = staticmethod(init_host)
	
	def clean_remote_host(remote_host):
		print "clean remote host host: " ,  remote_host.str()
		# reconnect with new object
		remote_host.connect()
		remote_host.clean_remote_host()
	
	clean_remote_host = staticmethod(clean_remote_host)
	
	def init_players(test_resources, test_config={}):
		process_list=[]
		
		Players_Factory.init_players(test_resources,test_config)
		for h in test_resources['host_list']:
			DDP_Players_Factory.init_host(h)
		test_resources['remote_host'].connect()
		
	init_players = staticmethod(init_players)
	
	
	def clean_players(test_resources, use_director = False, stop_ezbox = False):
		process_list=[]
		remote_host = test_resources['remote_host']
		if not remote_host.ssh.connection_established:
			remote_host.ssh.connect()
		remote_host.ssh.recreate_ssh_object()
		process_list.append(Process(target=DDP_Players_Factory.clean_remote_host, args=(remote_host,)))
		
		for host in test_resources['host_list']:
			if not host.ssh.connection_established:
				host.ssh.connect()
			host.ssh.recreate_ssh_object()
			process_list.append(Process(target=DDP_Players_Factory.clean_host, args=(host,)))
			
		Players_Factory.clean_players(test_resources,use_director,stop_ezbox,process_list)
	
	clean_players = staticmethod(clean_players)
	
	
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
		
		remote_host.start_sync_agent(ezbox.setup['mng_ip'])

		# conver to dictionary and return it
		dict={}
		dict['next_vm_idx']	= next_vm_idx
		dict['setup_list']	= setup_list
		dict['host_list']	= host_list 
		dict['remote_host']	= remote_host
		dict['ezbox']		= ezbox 
		
		return dict
	
	generic_init_ddp = staticmethod(generic_init_ddp)