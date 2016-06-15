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

from pexpect import pxssh
import pexpect


# pythons modules 
#===============================================================================
# Classes
#===============================================================================
class SshConnct:
	def __init__(self, hostname, username, password):

		# Init class variables
		self.ip_address	= hostname
		self.username   = username
		self.password   = password
		self.ssh_object = pxssh.pxssh()


	def connect(self):
		print "Connecting to : " + self.ip_address + ", username: " + self.username + " password: " + self.password
		self.ssh_object.login(self.ip_address, self.username, self.password)
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
	def execute_command(self, cmd):
		self.ssh_object.sendline(cmd)
		self.ssh_object.prompt()
		output = self.ssh_object.before
		output = output.split('\n')
		output = output[1:]
		output = str(output)
		
		self.ssh_object.sendline("echo $?")
		self.ssh_object.prompt()			   
		exit_code = self.ssh_object.before 
		exit_code = exit_code.split('\n')
		exit_code = exit_code[1]
 
		if int(exit_code) != 0:
			print "Error in excecuting command: " + cmd
			print "Exit Code " + exit_code
			print "Error message: " + output
			return [False,output]
		
		return [True,output]


class player(object):
	def __init__(self, ip, hostname, username, password, exe_path=None, exe_script=None, exec_params=None):
		self.ip = ip
		self.hostname = hostname
		self.username = username
		self.password = password
		self.exe_path = exe_path
		self.exe_script = exe_script
		self.exec_params = exec_params
		self.ssh		= SshConnct(hostname, username, password)

	def execute(self):
		if self.exe_script:
			sshpass_cmd = "sshpass -p " + self.password+ " ssh " + self.username + "@" + self.hostname
			exec_cmd    = "cd " + self.exe_path + "; python " + self.exe_script
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
# Functions
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

def get_ezbox_names(setup_num):
	setup_dict = {1:['ezbox29-host','ezbox29-chip'],
				  2:['ezbox24-host','ezbox24-chip'],
				  3:['ezbox35-host','ezbox35-chip'],
				  4:['ezbox55-host','ezbox55-chip']}
	
	return setup_dict[setup_num]

def get_setup_vip_list(setup_num):
	setup_dict = {1:['192.168.1.x','192.168.11.x'],
				  2:['192.168.2.x','192.168.12.x'],
				  3:['192.168.3.x','192.168.13.x'],
				  4:['192.168.4.x','192.168.14.x']}
	
	return setup_dict[setup_num]

def get_vip(vip_list,index):
	return vip_list[index/256].replace('x',str(index%256))

def get_setup_vip(setup_num,index):
	return get_vip(get_setup_vip_list(setup_num),index)
