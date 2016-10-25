#!/usr/bin/env python


#===============================================================================
# imports
#===============================================================================

# system  
import cmd
from collections import namedtuple
from optparse import OptionParser
import urllib2
import os
import sys
import inspect
from urllib2 import HTTPError, URLError
import socket
from socket import *

log_file = None

def init_log(log_file_name):
	global log_file
	log_file = open(log_file_name, 'w')
	log_file.write("#start HTTP client \n")
	
def log(str):
	log_file.write("%s\n" % str)

def end_log():
	log_file.write("#end HTTP client\n")
	log_file.close()

def readHtml(ip,connTimeout,port):
	try:
		response = urllib2.urlopen('http://'+ip+port, timeout=connTimeout)
	except HTTPError as e:
		log('%s : %s' %(ip, '404 ERROR'))
		log("#The server couldn\'t fulfill the request. Error code: %s " %str(e.code))
		log("# " + rc)
		return -1
	except URLError as e:
		log('%s : %s' %(ip, '404 ERROR'))
		log("# Failed to reach a server. Reason: %s" %str(e.reason))
		return -1
	except:
		log('%s : %s' %(ip, '404 ERROR'))
		log("# Unexpected error: %s" %sys.exc_info()[0])
		return -1
		
	   
	try:
		html_lines = response.readlines()
	except :
		log('%s : %s' %(ip, 'Connection closed ERROR'))
		log('#Connection closed ERROR')
		return 0

	html = html_lines[0]
	if isinstance(html, str):
		log('%s : %s' %(ip, html))
	else:
		html = str(html)
		log('%s : %s' %(ip, html))
		
	# end sucessfuly without errors
	return 0

################################################################################
# Function: Main
################################################################################
if __name__ == "__main__":
	usage = "usage: %prog [-i, -l, -r -t -e -p]"
	parser = OptionParser(usage=usage, version="%prog 1.0")
	
	parser.add_option("-i", "--http_ip", dest="http_ip",
					  help="IP of the HTTP server")
	parser.add_option("-p", "--port", dest="port",
					  help="PORT of the HTTP server", default= '')
	parser.add_option("-l", "--log_file", dest="log_file_name",
					  help="Log file name", default="log")
	parser.add_option("-r", "--requests", dest="num_of_requests",
					  help="Number of HTTP requests", default=1, type="int")
	parser.add_option("-t", "--timeout", dest="timeout",
					  help="Http Connection timeout", default=8, type="int")
	parser.add_option("-e", "--expects404", dest="expects404",
					  help="Test expects error 404", default= 'False')

	(options, args) = parser.parse_args()
	init_log(options.log_file_name)
	#convert to string
	options.expects404 = options.expects404.lower() == 'true'

	if not options.http_ip:
		log('#HTTP IP is not given')
		exit(1)
	
	
	if options.port!='':
		options.port=":"+options.port
	
	
	# read from HTML server x times (x = options.num_of_requests)
	for i in range(options.num_of_requests):
		rc = readHtml(options.http_ip,options.timeout,options.port)
		if rc == -1 and options.expects404 == False:
			break
		
	end_log()

