#!/usr/bin/env python
import time
import urllib2
import sys
import os
from optparse import OptionParser
import socket
import logging
from topology.TopologyAPI import TopologyAPI
import inspect
parentdir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
testdir = parentdir + '/testing' 
sys.path.insert(0,testdir) 
from common_infra import *
from DDP_infra import *

################################################################################
# Function: Main
################################################################################
ezbox = None
def parse_topology(topo_file):
    print " *** Parse topology file"
    t_api = TopologyAPI(topo_file)
    test_runner = t_api.get_topology_obj_by_id("test_runner")
    setup_num = str(test_runner.GetAttribute("setup_num"))
    print "Setup num " + setup_num 
    return setup_num

def init_ezbox(ezbox, cpus_count,file_name):
    print "init EZbox: " + ezbox.setup['name']
    print "CPUs count is " + cpus_count
    
    ezbox.remote_host.start_sync_agent()
    ezbox.connect()

    install_folder = os.getcwd() + '/../../'
    os.chdir(install_folder)
    
    if file_name == "None":
        file_name = None

    ezbox.ddp_service_stop()
    # Install ddp service
    ezbox.copy_and_install_ddp(file_name)
    cpus_range = "16-" + str(int(cpus_count) - 1)
    # Validate chip is up
    ezbox.ddp_service_start()

    #setting CPUs count
    ezbox.modify_run_cpus(cpus_range)
    ezbox.update_debug_params("--run_cpus %s --agt_enabled " % (cpus_range))
    ezbox.update_port_type("--port_type=%s " % (ezbox.setup['nps_port_type']))
    ezbox.ddp_service_stop()
    ezbox.ddp_service_start()
    # Wait for CP & DP
    ezbox.wait_for_cp_app()
    return ezbox.wait_for_dp_app()

    
################################################################################
# Function: Main
################################################################################
def main():
    global ezbox
    usage = "usage: %prog [-f, -c]"
    parser = OptionParser(usage=usage, version="%prog 1.0")
    parser.add_option("-c", "--cpus_count", dest="cpus_count",
                      help="number of cpus")
    parser.add_option("-f", "--file_name", dest="file_name",
                      help="Installation file name (.deb)")
    parser.add_option("--topo_file", dest="topo_file",
                      help="test topology script")
    (options, args) = parser.parse_args()
    setup_num = int(parse_topology(options.topo_file))
    
    remote_host_info = get_remote_host(setup_num)
    remote_host = Remote_Host(ip       = remote_host_info['ip'],
                              hostname = remote_host_info['hostname'])
    
    ezbox = DDP_ezbox(setup_num, remote_host)
    if init_ezbox(ezbox, options.cpus_count, options.file_name):
        return 0
    else:
        return 1



################################################################################
# Script start point
################################################################################
print '======================='
print ' DDP installation Main'
print '======================='
print 'Running on machine %s %s' %(socket.gethostname(), socket.gethostbyname(socket.gethostname()))
print 'args:'
print sys.argv
print ''
try:
    rc = main()
except Exception as error:
    logging.exception(error)
    rc = 1
finally:
    if ezbox:
        print "ezbox logout"
        ezbox.logout()
    if (rc == 0):
        print '----Installation Ended successfully------'
        exit(0)
    else:
        print '----Installation Ended with failure------'
        exit(1)


