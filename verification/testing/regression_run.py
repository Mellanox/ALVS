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
from optparse import OptionParser



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
	clean_cmd = currentdir + '/clean_setup_servers.py ' + setup_num + " > /dev/null 2>&1"
	retval = os.system(clean_cmd)
	

def main():
	global gen_retval
	global failed_tests
	signal.signal(signal.SIGINT, exit_signal_handler)
	
	usage = "usage: %prog [-s, -l, -c, -f, -e]"
	parser = OptionParser(usage=usage, version="%prog 1.0")
	
	bool_choices = ['true', 'false', 'True', 'False']
	parser.add_option("-s", "--setup_num", dest="setup_num",
					  help="Setup number. range (1..7)					(Mandatory parameter)")
	parser.add_option("-l", "--list_name", dest="list_name",
					  help="list name to run							(Mandatory parameter)")
	parser.add_option("-c", "--use_4k_cpus", dest="use_4k_cpus", 
					  help="true = use 4k cpu. false = use 512 cpus 	(default=False)")
	parser.add_option("-f", "--install_file", dest="install_file",
					  help="instalation tar.gz file name 				(default=alvs.tar.gz)")
	parser.add_option("-e", "--exit_on_error", dest="exit_on_error",
					  help="Will the program exit on first error		(default=False)")
	(options, args) = parser.parse_args()

	# validate options
	if not options.setup_num:
		print 'ERROR: setup_num was not given'
		exit(1)
	if not options.list_name:
		print 'ERROR: list_name was not given'
		exit(1)

	# Set user params
	setup_num    = options.setup_num
	list_name    = options.list_name
	if options.exit_on_error:
		exit_on_error = bool_str_to_bool(options.exit_on_error)
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
		
	test_group = 'system_level'
	failed_tests = ""
	os.system("cat /dev/null > verification/testing/lists/failed_tests")
	for line in list_file:
		
		if '# SYSTEM_LEVEL_TESTS' in line:
			test_group = 'system_level'
			os.system("echo # SYSTEM_LEVEL_TESTS >> verification/testing/lists/failed_tests")
			print "Run System Level Tests"
			
		if '# CP_UNIT_LEVEL_TESTS' in line:
			test_group = 'cp_unit_level'
			os.system("echo # CP_UNIT_LEVEL_TESTS >> verification/testing/lists/failed_tests")
			print "Run CP Unit Level Tests"

		if '# DP_UNIT_LEVEL_TESTS' in line:
			test_group = 'dp_unit_level'
			os.system("echo # DP_UNIT_LEVEL_TESTS >> verification/testing/lists/failed_tests")
			print "Run DP Unit Level Tests"

			
		if line[0] != '#' and line[0] != '\n':
			start = timer()
			
			test = line.strip('\n')
			
			print 'running test %s ...' % test
			
			# prepare test command
			logfilename = 'logs/%s_log' %test.split()[0]
			
			if test_group == 'system_level':
				cmd = currentdir + '/system_level/' + test + ' -s ' + setup_num
				if not first_test:
					print "**** not first test"
				else:
					clean_setup(setup_num)
					cmd += " --start true"
					if 	options.use_4k_cpus is not None:
						cmd += ' -c ' + options.use_4k_cpus
					if 	options.install_file is not None:
						cmd += ' -f ' + options.install_file
					first_test = False
				cmd = cmd + ' > ' +logfilename

			if test_group == 'cp_unit_level':
				cmd = currentdir + '/cp/' + test + ' -setup ' + setup_num + ' > ' + logfilename
			if test_group == 'dp_unit_level':
				cmd = currentdir + '/dp/' + test + ' -setup ' + setup_num + ' > ' + logfilename
									
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
				os.system("echo %s >> verification/testing/lists/failed_tests"%test)
				clean_setup(setup_num)
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
		print "to run all failed tests use regression run with list failed_tests"
		exit(1)


main()
