#!/usr/bin/env python


# system  
import cmd
from collections import namedtuple
import logging
import os
import sys
import inspect
from multiprocessing import Process



# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir) 
sys.path.insert(0,currentdir)
from e2e_infra import *

 
#===============================================================================
def clean_setup(setup_num):
	cmd = currentdir + '/clean_setup_servers.py ' + setup_num + " > /dev/null 2>&1"
	retval = os.system(cmd)
	if retval != 0 :
		print "WARNNING: CMD: %s - Failed" %cmd

def main():
	if len(sys.argv) < 4:
		print "script expects exactly 3 input arguments"
		print "Usage: regression_run.py <setup_num> <list_name> <True/False (use 4 k CPUs)> <True/False (exit on error)>"
		exit(1)
	
	# Set user params
	setup_num    = sys.argv[1]
	list_name    = sys.argv[2]
	use_4_k_cpus = sys.argv[3].lower()
	if len(sys.argv) >= 5:
		exit_on_error = bool_str_to_bool(sys.argv[4])
	else:
		exit_on_error = False
	
	# Open regression list file 
	try:
		filename = currentdir+'/lists/'+list_name
		list_file = open(filename,'r')
	except:
		print '****** Error openning file %s !!!' % list_name
		exit(1)
	
	# Execute all tests in list
	first_test = True
	gen_retval = True
	for line in list_file:
		if line[0] != '#':
			clean_setup(setup_num)
			
			test = line[:-1]
			print 'running test %s ...' % test
			
			# prepare test command
			logfilename = '%s_log' %test
			cmd = currentdir + '/' + test + ' -s ' + setup_num + ' -c ' + use_4_k_cpus
			if not first_test:
				print "**** not first test"
				cmd = cmd + " -m False -i False -f False -b False"
			else:
				first_test = False
			cmd = cmd + ' > ' +logfilename
			
			# execute test:
			print "*** CMD: " + cmd
			retval = os.system(cmd)
			
			# check test retval
			if retval != 0:
				gen_retval = False
				print "Test failed"
				print 'see logfile ' + logfilename
				if exit_on_error:
					break
			else: 
				print "Test passed"
				cmd = 'rm -f '+logfilename
				retval = os.system(cmd)


	# exit with gen_retval
	if gen_retval:
		print '===================='
		print 'All tests passed !!!'
		print '===================='
		exit(0)
	else:
		print '===================='
		print 'Some test failed !!!'
		print '===================='
		exit(1)


main()
