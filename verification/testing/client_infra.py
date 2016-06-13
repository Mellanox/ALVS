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
from common_infra import *


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