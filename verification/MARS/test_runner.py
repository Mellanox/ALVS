#!/usr/bin/env python

import urllib2
import sys
import os
from optparse import OptionParser
import socket
from topology.TopologyAPI import TopologyAPI
import inspect
parentdir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
################################################################################
# Function: Main
################################################################################

def parse_topology(topo_file):
	print " *** Parse topology file"
	t_api = TopologyAPI(topo_file)
	hosts = t_api.get_all_hosts()
	ezbox = t_api.get_topology_obj_by_id("main_ezbox")
	test_runner = t_api.get_topology_obj_by_id("test_runner")
#	print vars(test_runner) 
	setup_num = str(test_runner.GetAttribute("setup_num"))
	print "setup num " + setup_num 
#	print vars(ezbox) 
	host_name = ezbox.GetAttribute("host_name")
	return setup_num
	
################################################################################
# Function: Main
################################################################################
def main():
    usage = "usage: %prog [-t]"
    parser = OptionParser(usage=usage, version="%prog 1.0")
    parser.add_option("-e", "--env", dest="test_environment", help="test environment (path)")
    parser.add_option("-t", "--test", dest="test_script", help="test script to run")
    parser.add_option("--topo_file", dest="topo_file", help="test topology script")
    (options, args) = parser.parse_args()
    if (not options.test_script or not options.test_environment):
        print 'test script to run or test environment is not given'
        return 1
    
    test_to_run = parentdir +options.test_environment + options.test_script
    print "Test to run: %s" %test_to_run
    setup_num = parse_topology(options.topo_file)
    cmd = test_to_run + ' -s ' + setup_num
    
    if 'unit_tests' in cmd:
    	cmd = cmd + ' --install_package False --modify_run_cpus False'
    
    print "*** CMD: " + cmd
    retval = os.system(cmd)
    return retval

################################################################################
# Script start point
################################################################################
print '===================='
print ' Test Runner Main'
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


