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
ddpdir = testdir + '/DDP'
sys.path.append(testdir)
sys.path.append(ddpdir)
from common_infra import *
from alvs_infra import *
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

def get_test_config(cpus_count, file_name):
	install_folder = os.getcwd() + '/../../'
	os.chdir(install_folder)

	if file_name == "None":
		file_name = None
	
	test_config = {}
	test_config['start_ezbox'] = True
	test_config['install_package'] = True
	test_config['install_file'] = file_name
	test_config['stats'] = False
	test_config['modify_run_cpus'] = True
	test_config['num_of_cpus'] = cpus_count
	return test_config


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
	parser.add_option("-p", "--project_name", dest="project_name",
					help="Installation project name (alvs/ddp)")
	(options, args) = parser.parse_args()
	
	setup_num = int(parse_topology(options.topo_file))
	
	if options.project_name == 'alvs':
		print '========  ALVS  =========='
		ezbox = alvs_ezbox(setup_num)
	
	elif options.project_name == 'ddp':
		print '========  DDP  =========='
		remote_host_info = get_remote_host(setup_num)
		remote_host = Remote_Host(ip   = remote_host_info['all_ips'],
							  hostname = remote_host_info['hostname'],
							  all_eths = remote_host_info['all_eths'])
		ezbox = DDP_ezbox(setup_num, remote_host)
	
	else:
		err_msg = 'ERROR: Wrong Project Name: %s'%options.project_name
		raise RuntimeError(err_msg)

	test_config = get_test_config( options.cpus_count, options.file_name)
	ezbox.common_ezbox_bringup(test_config)

	
################################################################################
# Script start point
################################################################################
print '======================='
print '   Installation Main'
print '======================='
print 'Running on machine %s %s' %(socket.gethostname(), socket.gethostbyname(socket.gethostname()))
print 'args:'
print sys.argv
print ''
try:
	rc = 0
	main()
	print '----Installation Ended successfully------'
except Exception as error:
	logging.exception(error)
	print '----Installation Ended with failure------'
	rc = 1
finally:
	if ezbox:
		print "ezbox logout"
		ezbox.logout()
	exit(rc)

