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
	def __init__(self, ip, hostname, username, password, exe_path=None, exe_script=None, exec_params=None):
		# init parent class
		super(HttpClient, self).__init__(ip, hostname, username, password, exe_path, exe_script, exec_params)
		# Init class variables
		self.logfile = open('client_%s.log'%ip, 'w')
	def readHtml(self, ip):
		self.log('Openning HTTP connection with ' + ip)
		response = urllib2.urlopen('http://'+ip)
		html = response.read()
		if isinstance(html, str):
			self.log('HTML is a string:' + html)
			return html
		else:
			self.log('HTML is not a string')
			self.log(str(html))
			return str(html)

	def init_client(self):
		self.init_log()

	def clean_client(self):
		self.clear_log()

	def init_log(self):
		self.logfile.write("start HTTP client %s\n"%self.ip)
	
	def clear_log(self):
		self.logfile.write("end HTTP client %s\n"%self.ip)
		self.logfile.close()

	def log(self, str):
		self.logfile.write("%s\n"%str)
	
	def get_log(self, dest_dir):
		cmd = "sshpass -p %s scp %s@%s:%s %s"%(self.password, self.username, self.hostname, self.logfile.name, dest_dir)
		os.system(cmd)
		
		