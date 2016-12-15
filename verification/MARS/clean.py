#!/usr/bin/env python

import urllib2
import sys
import os
from optparse import OptionParser
import socket
from topology.TopologyAPI import TopologyAPI
import inspect
parentdir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
testdir = parentdir + '/testing' 
sys.path.insert(0,testdir) 
from common_infra import *

################################################################################
# Function: Main
################################################################################

def parse_topology(topo_file):
	print " *** Parse topology file"
	t_api = TopologyAPI(topo_file)
	test_runner = t_api.get_topology_obj_by_id("test_runner")
	setup_num = str(test_runner.GetAttribute("setup_num"))
	print "Setup num " + setup_num 
	return setup_num

################################################################################
# Function: Main
################################################################################
def main():
    usage = "usage: %prog [-t]"
    parser = OptionParser(usage=usage, version="%prog 1.0")
    parser.add_option("--topo_file", dest="topo_file", help="test topology script")
    (options, args) = parser.parse_args()
    setup_num = parse_topology(options.topo_file)
    path = parentdir+'/testing/system_level/'
    script_to_run = path + "clean_setup_servers.py"
    cmd = script_to_run + ' ' + setup_num
    print "*** CMD: " + cmd
    retval = os.system(cmd)
    return retval

################################################################################
# Script start point
################################################################################
print '==================='
print ' ALVS cleaning Main'
print '==================='
print 'Running on machine %s %s' %(socket.gethostname(), socket.gethostbyname(socket.gethostname()))
print 'args:'
print sys.argv
print ''
rc = main()
if (rc == 0):
    print '----Cleaning Ended successfully-----'
    exit(0)
else:
    print '----Cleaning Ended with failure-----'
    exit(1)

