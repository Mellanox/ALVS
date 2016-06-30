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

#===============================================================================
if len(sys.argv) != 4:
	print "script expects exactly 3 input arguments"
	print "Usage: regression_run.py <setup_num> <list_name> <True/False (use 4 k CPUs)>"
	exit(1)

setup_num  = sys.argv[1]
list_name = sys.argv[2]
use_4_k_cpus = sys.argv[3].lower()

try:
	filename = currentdir+'/lists/'+list_name
	list_file = open(filename,'r')
except:
	print '****** Error openning file %s !!!' % list_name
	exit(1)

for line in list_file:
	if line[0] != '#':
		test = line[:-1]
		print 'running test %s ...' % test
		logfilename = '%s_log' %test
		cmd = currentdir + '/' + test + ' ' + setup_num + ' ' + use_4_k_cpus + ' > ' +logfilename
		retval = os.system(cmd)
		
		if retval != 0 :
			print "Test failed"
			print 'see logfile ' + logfilename  
		else: 
			print "Test passed"
			cmd = 'rm -f '+logfilename
			retval = os.system(cmd)
	
		



 
