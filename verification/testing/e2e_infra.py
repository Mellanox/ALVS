#!/usr/bin/env python


#===============================================================================
# imports
#===============================================================================

# system  
import os
import sys

# pythons modules 
import cmd
import logging
from optparse import OptionParser
from collections import namedtuple

# local 
from common_infra import *
from server_infra import *


#===============================================================================
# Classes
#===============================================================================

class player:
	def __init__(self, hostname, username, password, name):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		self.ip_address = hostname
		self.username   = username
		self.password   = password
		self.name       = name

	def connect():
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		# TODO: implement
		print "TODO: implement " + sys._getframe().f_code.co_name

	def collect_reports():
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		# TODO: implement
		print "TODO: implement " + sys._getframe().f_code.co_name

	def disconnect():
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		# TODO: implement
		print "TODO: implement " + sys._getframe().f_code.co_name

#===============================================================================
# init functions
#===============================================================================

def init_HTTP_server(ip, virtual_ip, index_str, host_name):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	# TODO: get the following parameters
	user_name = "root" 
	password  = "3tango"
	
	server = HttpServer(ip_server=ip,
					virtual_ip_server=virtual_ip,
					hostname=host_name,
					username=user_name,
					password=password)
	server.connect()
	server.start_http_daemon()
	server.configure_loopback()
	server.disable_arp()
	
	print index_str
	server.set_index_html(index_str)


#------------------------------------------------------------------------------ 
def init_ALVS_daemon(host_ip):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	# TODO: implement
	print "TODO: implement " + sys._getframe().f_code.co_name
	return


	# TODO: add app_bin to class variables
	app_bin = "/tmp/alvs_daemon"
	
	ezbox = ezbox_host(hostname=host_ip, username='root', password='ezchip')
	ezbox.connect()
	ezbox.run_cp_app()
	ezbox.run_cp_app(app_bin)


#------------------------------------------------------------------------------ 	
def init_ALVS_DP(ip):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	# TODO: implement
	print "TODO: implement " + sys._getframe().f_code.co_name
	

#------------------------------------------------------------------------------ 
def init_clients(ip):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	# TODO: implement
	print "TODO: implement " + sys._getframe().f_code.co_name


#------------------------------------------------------------------------------ 
def init_players(server_list, ezbox, client_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	# init HTTP servers 
	for s in server_list:
		print "init server: " + str(s)
		init_HTTP_server(s.ip, s.virtual_ip, s.index_str, s.host_name)

	# init ALVS daemon
	init_ALVS_daemon(ezbox.host_ip)
	
	# init ALVS DP
	init_ALVS_DP(ezbox.dp_ip)
	
	# init client
	for c in client_list:
		print "init client: " + str(c)
		init_clients(c.ip)
	
	

#===============================================================================
# clean functions
#===============================================================================

def clean_HTTP_server(ip, virtual_ip, index_str, host_name):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	# TODO: get the following parameters
	user_name = "root" 
	password  = "3tango"
	
	server = HttpServer(ip_server=ip,
					virtual_ip_server=virtual_ip,
					hostname=host_name,
					username=user_name,
					password=password)
	
	server.connect()
	server.stop_http_daemon()
	server.take_down_loopback()
	server.enable_arp()
	server.delete_index_html()
	server.logout()


#------------------------------------------------------------------------------ 
def clean_ALVS_daemon(host_ip):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	# TODO: implement
	print "TODO: implement " + sys._getframe().f_code.co_name

#------------------------------------------------------------------------------ 
def clean_ALVS_DP(ip):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	# TODO: implement
	print "TODO: implement " + sys._getframe().f_code.co_name


#------------------------------------------------------------------------------ 
def clean_clients(ip):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	# TODO: implement
	print "TODO: implement " + sys._getframe().f_code.co_name


#------------------------------------------------------------------------------ 
def clean_players(server_list, ezbox, client_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	# init HTTP servers 
	for s in server_list:
		print "init server: " + str(s)
		clean_HTTP_server(s.ip, s.virtual_ip, s.index_str, s.host_name)

	# init ALVS daemon
	clean_ALVS_daemon(ezbox.host_ip)
	
	# init ALVS DP
	clean_ALVS_DP(ezbox.dp_ip)
	
	# init client
	for c in client_list:
		print "init client: " + str(c)
		clean_clients(c.ip)


#===============================================================================
# Run functions
#===============================================================================
	
def exe_pyton_on_vm(password, ip, exe_path, exe_script, exec_params):
	
	sshpass_cmd = "sshpass -p " + password+ " ssh root@" + ip
	exec_cmd    = "cd " + exe_path + "; python " + exe_script
	os.system(sshpass_cmd + " \"" + exec_cmd + " " + exec_params + "\"")

def run_test(server_list, client_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	# Execute HTTP servers scripts 
	for s in server_list:
		if s.exe_script:
			print "running server: " + str(s)
			exe_pyton_on_vm(s.password, s.ip, s.exe_path, s.exe_script, s.exec_params)

	# Excecure client
	for c in client_list:
		if c.exe_script:
			print "running client: " + str(c)
			exe_pyton_on_vm(c.password, c.ip, c.exe_path, c.exe_script, c.exec_params)
	
	

def collect_logs(server_list, ezbox, client_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	print "TODO: implement " + sys._getframe().f_code.co_name
	

#===============================================================================
# User Area
#===============================================================================

def user_init_sample():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	# Simple usage:
	# player: 10.157.7.195 (l-nps-001) - HTTP server 
	# player: 10.157.7.196 (l-nps-002) - Client
	# player: 10.157.7.197 (l-nps-003) - LVS
	# player: 10.157.7.198 (l-nps-004) - HTTP server 
	
	# create HTTP server list
	vip = "10.157.7.244"
	ServerStruct = namedtuple("ServerStruct", "ip password virtual_ip index_str host_name exe_path exe_script exec_params")
	server_1     = ServerStruct(ip = "10.157.7.195",
							password = "3tango",
							virtual_ip = vip,
							index_str = "10.157.7.195",
							host_name = "l-nps-001")
	server_2     = ServerStruct(ip = "10.157.7.196",
							password = "3tango",
							virtual_ip = vip,
							index_str = "10.157.7.196",
							host_name = "l-nps-002")
	server_list  = {server_1, server_2}
	
	# create Client list
	ClientStruct = namedtuple("ClientStruct", "ip", "password", "exe_path", "exe_script", "exec_params")
	client_1     = ClientStruct(ip      = "10.157.7.196",
							password    = "3tango",
							exe_path    = "/.autodirect/swgwork/kobis/workspace/GIT2/ALVS/verification/MARS/LoadBalancer/",
							exe_script  = "ClientWrapper.py",
							exec_params = "-i 10.157.7.244 -r 10 --s1 5 --s2 5 --s3 0 --s4 0 --s5 0 --s6 0")
	client_list  = {client_1}

	# EZbox list
	EZboxStruct = namedtuple("EZbox", "host_ip dp_ip")  # TODO: add more fields
	ezbox       = EZboxStruct(host_ip = "192.168.0.1", dp_ip = "192.168.0.1")  # TODO: handle

	return (server_list, ezbox, client_list)


#===============================================================================
# main function
#===============================================================================
# this function should be implemented by user

def main_sample():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	server_list, ezbox, client_list = user_init_sample()

	init_players(server_list, ezbox, client_list)
	
	run_test(server_list, client_list)
	
	collect_logs(server_list, ezbox, client_list)
	
	clean_players(server_list, ezbox, client_list)
	


#main_sample()
