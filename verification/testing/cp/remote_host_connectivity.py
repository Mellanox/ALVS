#!/usr/bin/env python


#===============================================================================
# imports
#===============================================================================

# system  
import sys
import random
import time

# pythons modules 
# local
sys.path.append("verification/testing")
from common_infra import *
from e2e_infra import *

server_count   = 0
client_count   = 0
service_count  = 0

def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	dict = generic_init(setup_num, service_count, server_count, client_count)
	
	return dict

def check_interface(expected_interface, interface):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	error = 0
	if interface['is_direct_output_lag'] != expected_interface['is_direct_output_lag'] :
		print "ERROR, is_direct_output_lag  = %d expected = %d\n" % (interface['is_direct_output_lag'], expected_interface['is_direct_output_lag'])
		error = 1

	if interface['direct_output_if'] != expected_interface['direct_output_if'] :
		print "ERROR, direct_output_if = %d, expected = %d\n" % (interface['direct_output_if'], expected_interface['direct_output_if'])
		error = 1

	if interface['path_type'] != expected_interface['path_type'] :
		print "ERROR, path_type  = %d expected = %d\n" % (interface['path_type'], expected_interface['path_type'])
		error = 1
	
	return error

def run_user_test_remote_mode(server_list, ezbox, client_list, vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	HOST_LOGICAL_ID = 4
	REMOTE_BASE_LOGICAL_ID = 5
	final_rc = True
	interfaces = ezbox.get_all_interfaces()
	print 'Start verifying interface table entries...'
	for interface in interfaces:
		print 'checking interface #%d...' %interface['key']
		#interfaces 0-3 are for NW
		if interface['key'] < HOST_LOGICAL_ID:
			expected_interface = {	'is_direct_output_lag' : 0,
									'direct_output_if' : interface['key'] + REMOTE_BASE_LOGICAL_ID,
									'path_type' : 0}
		elif interface['key'] == HOST_LOGICAL_ID:
			expected_interface = {	'is_direct_output_lag' : 0,
									'direct_output_if' : 0,
									'path_type' : 1}
		elif interface['key'] > HOST_LOGICAL_ID:
			expected_interface = {	'is_direct_output_lag' : 0,
									'direct_output_if' : interface['key'] - REMOTE_BASE_LOGICAL_ID,
									'path_type' : 2}
		
		#print 'interface = %s' %interface
		#print 'expected_interface = %s' %expected_interface
		rc = check_interface(expected_interface, interface)
		if rc:
			print "Error in comparing interface table entry"
			final_rc = False
	return final_rc

def run_user_test_non_remote_mode(server_list, ezbox, client_list, vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	HOST_LOGICAL_ID = 4
	final_rc = True
	interfaces = ezbox.get_all_interfaces()
	print 'Start verifying interface table entries...'
	for interface in interfaces:
		print 'checking interface #%d...' %interface['key']
		#interfaces 0-3 are for NW
		if interface['key'] < HOST_LOGICAL_ID:
			expected_interface = {	'is_direct_output_lag' : 0,
									'direct_output_if' : 4,
									'path_type' : 0}
		elif interface['key'] == HOST_LOGICAL_ID:
			expected_interface = {	'is_direct_output_lag' : 0,
									'direct_output_if' : 0,
									'path_type' : 1}
		
		rc = check_interface(expected_interface, interface)
		if rc:
			print "Error in comparing interface table entry"
			final_rc = False
	return final_rc
#===============================================================================
# main function
#===============================================================================

def main():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	config = fill_default_config(generic_main())
	
	print 'Start Remote control test...\n'
	print 'Setting start_ezbox to True...'
	config['start_ezbox'] = True
	print 'Setting stop_ezbox to True...'
	config['stop_ezbox'] = True
	print 'Setting remote_control to True...'
	config['remote_control'] = True
	
	final_rc = True
	
	dict = user_init(config['setup_num'])
	
	init_players(dict, config)
	
	server_list, ezbox, client_list, vip_list = convert_generic_init_to_user_format(dict)
	
	rc = run_user_test_remote_mode(server_list, ezbox, client_list, vip_list)
	
	clean_players(dict, True, config['stop_ezbox'])
	
	if rc == True:
		print 'Remote control test passed !!!'
	else:
		print 'Remote control test failed !!!'
		final_rc = False
	
	
	print 'Start Backward compatibility test...\n'
	
	print 'Setting remote_control to False...'
	config['remote_control'] = False
	print 'Setting stop_ezbox to False...'
	config['stop_ezbox'] = False
	
	init_players(dict, config)
	
	server_list, ezbox, client_list, vip_list = convert_generic_init_to_user_format(dict)
	
	rc = run_user_test_non_remote_mode(server_list, ezbox, client_list, vip_list)
	
	clean_players(dict, True, config['stop_ezbox'])
	
	if rc == True:
		print 'Backward compatibility test passed !!!'
	else:
		print 'Backward compatibility test failed !!!'
		final_rc = False
	
	if final_rc == True:
		print 'Test passed !!!'
		exit(0)
	else:
		print 'Test failed !!!'
		exit(1)
	
main()

