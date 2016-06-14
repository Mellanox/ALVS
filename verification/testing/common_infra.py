#!/usr/bin/env python

#===============================================================================
# imports
#===============================================================================

# system  
import cmd
import logging
import os
import sys

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
	def __init__(self, ip, hostname, username, password, exe_path, exe_script, exec_params):
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
			sshpass_cmd = "sshpass -p " + self.password+ " ssh root@" + self.ip
			exec_cmd    = "cd " + self.exe_path + "; python " + self.exe_script
			os.system(sshpass_cmd + " \"" + exec_cmd + " " + self.exec_params + "\"")

	def connect(self):
		self.ssh.connect()
		
	def logout(self):
		self.ssh.logout()
		
	def str(self):
		return self.ip
 
