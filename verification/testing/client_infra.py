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


# pythons modules 
# local 
#===============================================================================
# Classes
#===============================================================================


class HttpClient(player):
	def __init__(self, ip, hostname, username, password, exe_path, exe_script, exec_params):
		# init parent class
		super(HttpClient, self).__init__(ip, hostname, username, password, exe_path, exe_script, exec_params)
		# Init class variables
		self.logfile_name = '/root/client_%s.log'%ip
		self.exec_params += ' -l %s' %self.logfile_name
		self.loglist = [self.logfile_name]
	def init_client(self):
		pass

	def clean_client(self):
		pass
	def add_log(self, new_log):
		self.logfile_name = new_log
		self.exec_params = self.exec_params[:self.exec_params.find('-l')] +'-l %s' %self.logfile_name 
		self.loglist.append(new_log)
	def get_log(self, dest_dir):
		for log in self.loglist:
			cmd = "sshpass -p %s scp -o StrictHostKeyChecking=no %s@%s:%s %s"%(self.password, self.username, self.hostname, log, dest_dir)
			os.system(cmd)
		
		