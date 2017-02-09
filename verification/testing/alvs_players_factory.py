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
from alvs_infra import *

class ALVS_Players_Factory(Players_Factory):
	
	def __init__(self):
		super(ALVS_Player_Factory, self).__init__()
	
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
		
	generic_init = staticmethod(generic_init)

	def convert_generic_init_to_user_format(dict):
		return ( dict['server_list'],
				 dict['ezbox'],
				 dict['client_list'],
				 dict['vip_list'] )
	convert_generic_init_to_user_format = staticmethod(convert_generic_init_to_user_format)
	
	#------------------------------------------------------------------------------ 
	def init_server(server):
		print "init server: " + server.str()
		server.init_server(server.ip)
	
	init_server = staticmethod(init_server)
	
	#------------------------------------------------------------------------------ 
	def init_client(client):
		print "init client: " + client.str()
		client.init_client()
	
	init_client = staticmethod(init_client)
	

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
		
		return Players_Factory.fill_default_config(test_config,default_config)
	
	fill_default_config = staticmethod(fill_default_config)
	
	#------------------------------------------------------------------------------
	def init_players(test_resources, test_config={}):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		process_list=[]
		for s in test_resources['server_list']:
			ALVS_Players_Factory.init_server(s)
		for c in test_resources['client_list']:
			ALVS_Players_Factory.init_client(c)
		
		
		Players_Factory.init_players(test_resources,test_config)
		
		ALVS_Players_Factory.alvs_bringup_env(test_resources['ezbox'], test_resources['server_list'], test_resources['vip_list'], test_config['use_director'])
	
	init_players = staticmethod(init_players)
	
	#===============================================================================
	# clean functions
	#===============================================================================
	#------------------------------------------------------------------------------ 
	def clean_server(server):
		print "clean server: " + server.str()
		
		# reconnect with new object
		server.connect()
		server.clean_server()
	
	clean_server = staticmethod(clean_server)
	
	#------------------------------------------------------------------------------ 
	def clean_client(client):
		print "clean client: " + client.str()
	
		# reconnect with new object
		client.connect()
		client.clean_client()
	
	clean_client = staticmethod(clean_client)
	
	#------------------------------------------------------------------------------ 
	def clean_ezbox(ezbox, use_director, stop_ezbox):
		print "Clean EZbox: " + ezbox.setup['name']
		
		# reconnect with new object
		ezbox.connect()
		ezbox.clean(use_director, stop_ezbox)
	
	clean_ezbox = staticmethod(clean_ezbox)
	#------------------------------------------------------------------------------ 
	def clean_players(test_resources, use_director = False, stop_ezbox = False):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
		# Add servers, client & EZbox to proccess list
		# in adition, recreate ssh object for the new proccess (patch)
		process_list = []
		for s in test_resources['server_list']:
			s.ssh.recreate_ssh_object()
			process_list.append(Process(target=ALVS_Players_Factory.clean_server, args=(s,)))
			
		for c in test_resources['client_list']:
			c.ssh.recreate_ssh_object()
			process_list.append(Process(target=ALVS_Players_Factory.clean_client, args=(c,)))
		
		if test_resources['ezbox']:
			test_resources['ezbox'].ssh_object.recreate_ssh_object()
			test_resources['ezbox'].run_app_ssh.recreate_ssh_object()
			test_resources['ezbox'].syslog_ssh.recreate_ssh_object()
			process_list.append(Process(target=ALVS_Players_Factory.clean_ezbox, args=(test_resources['ezbox'], use_director, stop_ezbox,)))
		
		start_and_wait_for_processes(process_list)
		

		#Players_Factory.clean_players(test_resources,use_director,stop_ezbox,process_list)
	
	clean_players = staticmethod(clean_players)
	
	
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
	
	alvs_bringup_env = staticmethod(alvs_bringup_env)