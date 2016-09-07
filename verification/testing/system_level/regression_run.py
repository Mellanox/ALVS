#!/usr/bin/env python


# system  
import cmd
from collections import namedtuple
import logging
import os
import sys
import inspect
from multiprocessing import Process
import signal
from timeit import default_timer as timer



# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir) 
sys.path.insert(0,currentdir)
from e2e_infra import *

gen_retval = True
failed_tests = ""
def exit_signal_handler(signal = None, frame = None):
	global gen_retval
	global failed_tests
	print("Ctrl-C was pressed, Didn't finished run all tests")
	if gen_retval:
		print 'The tests that ran were passed'
		exit(1)
	else:
		print 'Some tests failed !!!'
		print "Failed Tests:"
		print failed_tests
		exit(1)
		
#===============================================================================
def clean_setup(setup_num):
	print 'Clean setup'
	cmd = currentdir + '/clean_setup_servers.py ' + setup_num + " > /dev/null 2>&1"
	retval = os.system(cmd)
	

def main():
	global gen_retval
	global failed_tests
	signal.signal(signal.SIGINT, exit_signal_handler)
	
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
	
	if not os.path.exists("logs"):
		os.makedirs("logs")
		
	failed_tests = ""
	os.system("cat /dev/null > verification/testing/system_level/lists/failed_tests")
	for line in list_file:
		if line[0] != '#':
			start = timer()
			clean_setup(setup_num)
			
			test = line[:-1]
			print 'running test %s ...' % test
			
			# prepare test command
			logfilename = 'logs/%s_log' %test
			cmd = currentdir + '/' + test + ' -s ' + setup_num + ' -c ' + use_4_k_cpus
			if not first_test:
				print "**** not first test"
				cmd = cmd + " -m False -i False -f False -b False --start false --stop false"
			else:
				cmd = cmd + " --start true"
				first_test = False
			cmd = cmd + ' > ' +logfilename
			
			# execute test:
			print "*** CMD: " + cmd
			retval = os.system(cmd)
			
			if os.WIFSIGNALED(retval):
				exit_signal_handler()
				
			end = timer()
			print "Total test time is: %s" %(end-start)
			# check test retval
			if retval != 0:
				gen_retval = False
				print "Test failed"
				print 'see logfile ' + logfilename
				failed_tests += (test + '\n')
				os.system("echo %s >> verification/testing/system_level/lists/failed_tests"%test)
				if exit_on_error:
					break
			else: 
				print "Test passed"
				cmd = 'rm -f '+logfilename
				retval = os.system(cmd)
			
			# new line for seperating tests results
			print ""
			print ""


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
		print "Failed Tests:"
		print failed_tests
		print "to run all failed tests use: verification/testing/system_level/regression_run.py " + setup_num + " failed_tests false"
		exit(1)


main()
