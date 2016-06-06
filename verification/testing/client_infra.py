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
import urllib2

# local 
from user import SshConnct


#===============================================================================
# Classes
#===============================================================================


class HttpClient:
	def __init__(self, server_ip, hostname, username, password):

		# Init class variables
		self.server_ip	= server_ip
		self.ssh		= SshConnct(hostname, username, password)


	def connect(self):
		self.ssh.connect()

		
	def logout(self):
		self.ssh.logout()


	def readHtml(ip):
		print 'Openning HTTP connection with ' + ip
		
		response = urllib2.urlopen(ip)
		html = response.read()
		if isinstance(html, str):
			print 'HTML is a string:' + html
		else:
			print 'HTML is not a string'
			print html
	
			return str(html)
