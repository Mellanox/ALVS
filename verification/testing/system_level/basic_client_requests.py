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

def readHtml(ip):
    try:
        response = urllib2.urlopen('http://'+ip, timeout=5)
    except urllib2.URLError,err:
        log('%s : %s' %(ip, '404 ERROR'))
        return str(err)
    try:
    	html_lines = response.readlines()
    except :
    	log('%s : %s' %(ip, 'Connection closed ERROR'))
    	return 'Connection closed ERROR'

    html = html_lines[0]
    if isinstance(html, str):
    	log('%s : %s' %(ip, html))
    else:
    	html = str(html)
    	log('%s : %s' %(ip, html))
	return html

################################################################################
# Function: Main
################################################################################
if __name__ == "__main__":
    usage = "usage: %prog [-i, -l, -r]"
    parser = OptionParser(usage=usage, version="%prog 1.0")
    
    parser.add_option("-i", "--http_ip", dest="http_ip",
                      help="IP of the HTTP server")
    parser.add_option("-l", "--log_file", dest="log_file_name",
					  help="Log file name", default="log")
    parser.add_option("-r", "--requests", dest="num_of_requests",
                      help="Number of HTTP requests", default=1, type="int")

    (options, args) = parser.parse_args()
    init_log(options.log_file_name)

    if not options.http_ip:
        log('HTTP IP is not given')
        exit(1)
    
    # read from HTML server x times (x = options.num_of_requests)
    for i in range(options.num_of_requests):
        readHtml(options.http_ip) 

    end_log()

