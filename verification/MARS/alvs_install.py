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

def init_ezbox(ezbox, use_4k_cpus,file_name):
	print "init EZbox: " + ezbox.setup['name']
	ezbox.connect()

	install_folder = os.getcwd() + '/../../'
	os.chdir(install_folder)
	
	if file_name == "None":
		file_name = None
	# Install alvs service
	ezbox.copy_and_install_alvs(file_name)
	# Validate chip is up
	ezbox.alvs_service_start()
	# Set CP, DP params
	ezbox.update_dp_cpus(use_4k_cpus)
	ezbox.update_cp_params("--agt_enabled --port_type=%s"%ezbox.setup['nps_port_type'])
	ezbox.alvs_service_stop()
	# Start the service
	ezbox.alvs_service_start()
	# Wait for CP & DP
	ezbox.wait_for_cp_app()
	ezbox.wait_for_dp_app()
	
################################################################################
# Function: Main
################################################################################
def main():
    usage = "usage: %prog [-f, -c]"
    parser = OptionParser(usage=usage, version="%prog 1.0")
    parser.add_option("-c", "--use_4k_cpus", dest="use_4k_cpus",
                      help="use 4k cpus true/false")
    parser.add_option("-f", "--file_name", dest="file_name",
                      help="Installation file name (.deb)")
    parser.add_option("--topo_file", dest="topo_file",
                      help="test topology script")
    (options, args) = parser.parse_args()
    setup_num = int(parse_topology(options.topo_file))

    if options.use_4k_cpus.lower() not in ['true', 'false']:
        print "Bad param: use_4k_cpus = %s !! Should be true/false" %options.use_4k_cpus
        return 1


    ezbox = ezbox_host(setup_num)
    init_ezbox(ezbox, bool_str_to_bool(options.use_4k_cpus), options.file_name)

    #, options.cpu_count
    retval=0
    return retval

################################################################################
# Script start point
################################################################################
print '======================='
print ' ALVS installation Main'
print '======================='
print 'Running on machine %s %s' %(socket.gethostname(), socket.gethostbyname(socket.gethostname()))
print 'args:'
print sys.argv
print ''
rc = main()
if (rc == 0):
    print '----Installation Ended successfully------'
    exit(0)
else:
    print '----Installation Ended with failure------'
    exit(1)


