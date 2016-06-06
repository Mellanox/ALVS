#!/usr/bin/env python

#===============================================================================
# imports
#===============================================================================

# system  
import os
import sys

# pythons modules 
import pexpect
from pexpect import pxssh
import cmd
import logging


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

