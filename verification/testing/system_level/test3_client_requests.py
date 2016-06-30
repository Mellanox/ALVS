#!/usr/bin/env python


#===============================================================================
# imports
#===============================================================================

# system  
import cmd
from collections import namedtuple
from optparse import OptionParser
import httplib
import socket
import os
import sys
import inspect
import time


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

def myReadHtml(ip,source_ip,source_port):
	log("#connecting from source address = " + source_ip + ":" + str(source_port))
	
	source_ip_and_port = (source_ip,source_port)
	
	#log("#Createing HTTPConnection ...")
	conn = httplib.HTTPConnection(host=ip,source_address=source_ip_and_port)
	
	#log("#Reuesting index.html ...")
	conn.request("GET", "/index.html")

	#log("#Getting response ...")
	response = conn.getresponse()
	
	data = response.read()
	log( ip + " : " + data)
	
	#log("#Closing connection...")
	conn.close()
	
	return data.strip()

################################################################################
# Function: Main
################################################################################
if __name__ == "__main__":
	usage = "usage: %prog [-i, -cip, -l, -r]"
	parser = OptionParser(usage=usage, version="%prog 1.0")
	
	parser.add_option("-i", "--http_ip", dest="http_ip",
					  help="IP of the HTTP server")
	parser.add_option("-c", "--client_ip", dest="client_ip",
					  help="IP of the client")
	parser.add_option("-s", "--setup_num", dest="setup_num",
					  help="Setup Number")
	parser.add_option("-l", "--log_file", dest="log_file_name",
					  help="Log file name", default="log")
	parser.add_option("-r", "--requests", dest="num_of_requests",
					  help="Number of HTTP requests", default=1, type="int")

	(options, args) = parser.parse_args()
	init_log(options.log_file_name)

	if not options.http_ip:
		log('HTTP IP is not given')
		end_log()
		exit(1)
	
	if (options.setup_num != "1") and (options.setup_num != "4"):
		log("Currnetly this test supports only setups 1 & 4 and not setup #" + options.setup_num)
		end_log()
		exit(1)
	
	port_server_dict = []
	if options.setup_num == "1":
		port_server_dict = [(20,"192.168.0.20"),(21,"192.168.0.20"),
					(16,"192.168.0.21"),(17,"192.168.0.21"),
					(11,"192.168.0.22"),(18,"192.168.0.22"),
					(10,"192.168.0.23"),(14,"192.168.0.23"),
					(12,"192.168.0.24"),(13,"192.168.0.24")]
	else:
		port_server_dict = [(30,"192.168.0.50"),(34,"192.168.0.50"),
					(10,"192.168.0.51"),(11,"192.168.0.51"),
					(12,"192.168.0.52"),(14,"192.168.0.52"),
					(13,"192.168.0.53"),(17,"192.168.0.53"),
					(15,"192.168.0.54"),(36,"192.168.0.54")]
		
	exitVal = 1
	response_count = 0
	for port_server in port_server_dict:
		response_server = myReadHtml(options.http_ip, options.client_ip, port_server[0])
		if response_server == port_server[1]:
			response_count += 1
		else:
			log("Error: Response server for service: " + options.http_ip + " and port: " + str(port_server[0]) + " should be : " + port_server[1] + " instead of: " + response_server)

	if response_count == 10:
		log("Test passed !!!")
		exitVal = 0
	else:
		log("Test failed !!!")
	end_log()
	exit(exitVal)

