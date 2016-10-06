#!/usr/bin/env python

#===============================================================================
# imports
#===============================================================================

# system  
import cmd
import logging
import os
import sys
import urllib2

# local 
from common_infra import *


#===============================================================================
# Classes
#===============================================================================
# 

class HttpClient(player):
	def __init__(self, ip, hostname,
				username    = "root",
				password    = "3tango",
				exe_path    = "/root/tmp/",
				src_exe_path    = os.path.dirname(os.path.realpath(__file__))+"/system_level",
				exe_script  = "basic_client_requests.py",
				exec_params = ""):
		
		# init parent class
		super(HttpClient, self).__init__(ip, hostname, username, password, exe_path, exe_script, exec_params)
		
		# Init class variables
		self.logfile_name  = '/root/client_%s.log'%ip
		self.exec_params  += ' -l %s' %self.logfile_name
		self.loglist       = [self.logfile_name]
		self.src_exe_path  = src_exe_path

	def init_client(self):
		self.connect()
		self.clear_arp_table()
		self.copy_exec_script()

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
		rc = self.copy_file_to_player(filename, self.exe_path)
		if rc:
			print "ERROR: falied to copy exe script. client IP: " + self.hostname
		
	def remove_exec_script(self):
		cmd = "rm -rf %s/%s" %(self.exe_path, self.exe_script)
		self.execute_command(cmd)

