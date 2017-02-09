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
from atc_infra import atc_ezbox
from common_infra import *

class ATC_Players_Factory(Players_Factory):
	
	def __init__(self):
		super(ATC_Player_Factory, self).__init__()
			
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
	
	def init_players(test_resources, test_config={}):	
		Players_Factory.init_players(test_resources,test_config)
		for h in test_resources['host_list']:
			ATC_Players_Factory.init_host(h)
	
	init_players = staticmethod(init_players)
	
	
	def clean_players(test_resources, use_director = False, stop_ezbox = False):
		process_list=[]
		for host in test_resources['host_list']:
			if not host.ssh.connection_established:
				host.ssh.connect()
			host.ssh.recreate_ssh_object()
			process_list.append(Process(target=ATC_Players_Factory.clean_host, args=(host,)))
		Players_Factory.clean_players(test_resources,use_director,stop_ezbox,process_list)
	
	clean_players = staticmethod(clean_players)
	
	
	def generic_init_atc(setup_num, host_count):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		next_vm_idx = 0
		setup_list  = get_setup_list(setup_num)
	
		# Create host list
		host_list=[]
		for i in range(host_count):
			host = Host(ip 			= setup_list[next_vm_idx]['all_ips'],
						hostname 	= setup_list[next_vm_idx]['hostname'],
						all_eths 	= setup_list[next_vm_idx]['all_eths'])
			host_list.append(host)
			next_vm_idx+=1
		
		ezbox = atc_ezbox(setup_num)
	
		# conver to dictionary and return it
		dict={}
		dict['next_vm_idx']	= next_vm_idx
		dict['setup_list']	= setup_list
		dict['host_list']	= host_list 
		dict['ezbox']		= ezbox 
	
		return dict
	
	generic_init_atc = staticmethod(generic_init_atc)