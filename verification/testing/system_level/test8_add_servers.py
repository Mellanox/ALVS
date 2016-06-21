#!/usr/bin/env python

#===============================================================================
# imports
#===============================================================================
# system  
import cmd
import os
import sys

#===============================================================================
# Function: set_user_params
#
# Brief:
#===============================================================================
def main():
	if len(sys.argv) != 2:
		print "script expects exactly 1 input argument"
		print "Usage: client_requests.py <setup_num>"
		exit(1)
	
	cmd = "python2.7 verification/testing/system_level/test8_10_add_servers.py -s %s -t 8" %(sys.argv[1])
	print "running command " + cmd
	os.system(cmd)

#------------------------------------------------------------------------------ 
main()
