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

from common_infra import SshConnct
from pexpect import pxssh
import pexpect


# pythons modules 
# local 
#===============================================================================
# Classes
#===============================================================================


class HttpClient(player):

	def readHtml(self, ip):
		print 'Openning HTTP connection with ' + ip
		
		response = urllib2.urlopen(ip)
		html = response.read()
		if isinstance(html, str):
			print 'HTML is a string:' + html
		else:
			print 'HTML is not a string'
			print html
	
			return str(html)

	def init_client(self):
		# TODO: inplement
		pass
	def clean_client(self):
		# TODO: inplement
		pass	
