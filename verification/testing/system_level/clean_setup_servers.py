#!/usr/bin/env python
#===============================================================================
# imports
#===============================================================================
# system  
import cmd
from collections import namedtuple
import logging
import os
import sys
import inspect
from multiprocessing import Process
import time
from timeit import default_timer as timer
# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir) 
from e2e_infra import *

'''
 Function: clean_server
 Brief:
	connect to VM. 
	if connection time takes more than 6 seconds - reset VM
	else - clean server configurations
 Return: 
 	0 = Connection successful, VM cleaned
 	1 = Connection failed, reset VM 
'''
def clean_server(server, vm):
	s = HttpServer(ip = vm['ip'],
				  hostname = vm['hostname'], 
				  username = "root", 
				  password = "3tango", 
				  vip = "",
				  eth='ens6')
	start = timer()
	s.connect()
	end = timer()
	total = (end-start)
	if total < 6:
		s.clean_server(False)
		exit(0)
	else:
		print "Reset VM %s, Connect time is: %s" % (vm['hostname'], total)
		server_ssh = SshConnect(server, username = "root", password = "3tango")
		server_ssh.connect()
		cmd = "virsh reset %s-RH-7.2" %vm['hostname']
		server_ssh.execute_command(cmd)
		server_ssh.logout()
		exit(1)

def main():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	if len(sys.argv) != 2:
		print "script expects exactly 1 input argument"
		print "Usage: clean_setup_servers.py <setup_num>"
		exit(1)
	
	rc = 0
	setup_num = int(sys.argv[1]) 	
	setup_list = get_setup_list(setup_num)
	process_list = []
	server_name = setup_list[0]['hostname'][:setup_list[0]['hostname'].rfind('-')]
	for vm in setup_list:
		process_list.append(Process(target=clean_server, args=(server_name, vm)))
	for p in process_list:
		p.start()
	for p in process_list:
		p.join()
		rc += p.exitcode
	if rc > 0:
		print "Wait for %d VMs to reset ... (you can CTRL+C here)" %rc
		time.sleep(30)
	print "Done!"
	
main()
