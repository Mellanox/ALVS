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
	if len(sys.argv) != 3:
		print "script expects exactly 2 input arguments"
		print "Usage: client_requests.py <setup_num> <True/False (use 4 k CPUs)>"
		exit(1)
	
	cmd = "python2.7 verification/testing/system_level/test8_10_add_servers.py -s %s -t 9 -u %s" %(sys.argv[1], sys.argv[2])
	print "running command " + cmd
	os.system(cmd)

#------------------------------------------------------------------------------ 
main()
