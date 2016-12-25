#!/usr/bin/env python

import urllib2
import sys
import os
from optparse import OptionParser
import socket
import inspect
parentdir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
grandparentdir =  os.path.dirname(parentdir)
################################################################################
# Function: Main
################################################################################
def main():
    usage = "usage: %prog [-t]"
    parser = OptionParser(usage=usage, version="%prog 1.0")
    parser.add_option("-a", "--app", dest="app", help="Name of application to run. Valid values: ALVS", default='', type="str")
    parser.add_option("-u", "--unit_test", dest="unit_test", help="Unit test name")
    parser.add_option("-t", "--test_case", dest="test_case", help="Name of a specific test case to run")
    parser.add_option("--topo_file", dest="topo_file", help="test topology script")
    (options, args) = parser.parse_args()
    
    tests_path = grandparentdir + '/venus'
    os.chdir(tests_path)
    print "Test Case :   " + options.test_case
    print "venus_runner working dir is:   "+ os.getcwd()
    cmd = './run_unit.py -a ' + options.app + ' -u unit_tests/' + options.unit_test + ' -t ' + options.test_case + ' -n'
    print "*** CMD: " + cmd
    retval = os.system(cmd)
    return retval

################################################################################
# Script start point
################################################################################
print '===================='
print ' Venus Runner Main'
print '===================='
print 'Running on machine %s %s' %(socket.gethostname(), socket.gethostbyname(socket.gethostname()))
print 'args:'
print sys.argv
print ''
print '---M a i n   S t a r t e d---'
rc = main()
print "ret code = %d" %rc
if (rc == 0):
    print '----M a i n   E n d e d successfully------'
    exit(0)
else:
    print '----M a i n   E n d e d with failure------'
    exit(1)


