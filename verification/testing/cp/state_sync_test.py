#!/usr/bin/env python


#===============================================================================
# imports
#===============================================================================

# system  
import sys
import random


# pythons modules 
# local
sys.path.append("verification/testing")
from test_infra import *


#===============================================================================
# Test Globals
#===============================================================================
server_count = 4
service_count = 3

#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

def init_log(args):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	log_file = "scheduling_algorithm_test.log"
	if 'log_file' in args:
		log_file = args['log_file']
	init_logging(log_file)


def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	vip_list = [get_setup_vip(setup_num,i) for i in range(service_count)]
	
	setup_list = get_setup_list(setup_num)
	
	index = 0
	server_list=[]
	for i in range(server_count):
		server_list.append(real_server(management_ip=setup_list[index]['hostname'], data_ip=setup_list[index]['ip']))
		index+=1
	
	# EZbox
	ezbox = ezbox_host(setup_num)
	
	return (server_list, ezbox, vip_list)


def init_ezbox(args,ezbox):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	if args['hard_reset']:
		ezbox.reset_ezbox()
		# init ALVS daemon
	ezbox.connect()
	ezbox.flush_ipvs()
	ezbox.alvs_service_stop()
	ezbox.copy_cp_bin(debug_mode=args['debug'])
	ezbox.copy_dp_bin(debug_mode=args['debug'])
	ezbox.alvs_service_start()
	ezbox.wait_for_cp_app()
	ezbox.wait_for_dp_app()
	ezbox.clean_director()


def server_class_hash_add_test():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	service0.add_server(server_list[0])
	service0.add_server(server_list[1])
	service0.add_server(server_list[2])
	service1.add_server(server_list[1])
	service1.add_server(server_list[3])

	servers = ezbox.get_all_servers()

	#Verify server0 in service0
	found = False
	for server in servers:
	    if server['index'] == 0:
	        if server['key']['server_ip'] == ip2int(server_list[0].data_ip) and server['key']['virt_ip'] == ip2int(vip_list[0]) and server['key']['server_port'] == 80 and server['key']['virt_port'] == 80 and server['key']['protocol'] == 6:
	            found=True 
	if found == False:
	    print 'Server0 not found in service0'
	    return 1

	#Verify server1 in service0
	found = False
	for server in servers:
	    if server['index'] == 1:
	        if server['key']['server_ip'] == ip2int(server_list[1].data_ip) and server['key']['virt_ip'] == ip2int(vip_list[0]) and server['key']['server_port'] == 80 and server['key']['virt_port'] == 80 and server['key']['protocol'] == 6:
	            found=True 
	if found == False:
	    print 'Server1 not found in service0'
	    return 1

	#Verify server2 in service0
	found = False
	for server in servers:
	    if server['index'] == 2:
	        if server['key']['server_ip'] == ip2int(server_list[2].data_ip) and server['key']['virt_ip'] == ip2int(vip_list[0]) and server['key']['server_port'] == 80 and server['key']['virt_port'] == 80 and server['key']['protocol'] == 6:
	            found=True 
	if found == False:
	    print 'Server2 not found in service0'
	    return 1

	#Verify server1 in service1
	found = False
	for server in servers:
	    if server['index'] == 3:
	        if server['key']['server_ip'] == ip2int(server_list[1].data_ip) and server['key']['virt_ip'] == ip2int(vip_list[1]) and server['key']['server_port'] == 80 and server['key']['virt_port'] == 80 and server['key']['protocol'] == 6:
	            found=True 
	if found == False:
	    print 'Server1 not found in service1'
	    return 1

	#Verify server3 in service1
	found = False
	for server in servers:
	    if server['index'] == 4:
	        if server['key']['server_ip'] == ip2int(server_list[3].data_ip) and server['key']['virt_ip'] == ip2int(vip_list[1]) and server['key']['server_port'] == 80 and server['key']['virt_port'] == 80 and server['key']['protocol'] == 6:
	            found=True 
	if found == False:
	    print 'Server3 not found in service1'
	    return 1

	return 0


def server_class_hash_delete_test():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	service0.remove_server(server_list[1])
	service1.remove_server(server_list[1])

	servers = ezbox.get_all_servers()

	#Verify server0 in service0
	found = False
	for server in servers:
	    if server['index'] == 0:
	        if server['key']['server_ip'] == ip2int(server_list[0].data_ip) and server['key']['virt_ip'] == ip2int(vip_list[0]) and server['key']['server_port'] == 80 and server['key']['virt_port'] == 80 and server['key']['protocol'] == 6:
	            found=True 
	if found == False:
	    print 'Server0 not found in service0'
	    return 1

	#Verify server2 in service0
	found = False
	for server in servers:
	    if server['index'] == 2:
	        if server['key']['server_ip'] == ip2int(server_list[2].data_ip) and server['key']['virt_ip'] == ip2int(vip_list[0]) and server['key']['server_port'] == 80 and server['key']['virt_port'] == 80 and server['key']['protocol'] == 6:
	            found=True 
	if found == False:
	    print 'Server2 not found in service0'
	    return 1

	#Verify server3 in service1
	found = False
	for server in servers:
	    if server['index'] == 4:
	        if server['key']['server_ip'] == ip2int(server_list[3].data_ip) and server['key']['virt_ip'] == ip2int(vip_list[1]) and server['key']['server_port'] == 80 and server['key']['virt_port'] == 80 and server['key']['protocol'] == 6:
	            found=True 
	if found == False:
	    print 'Server3 not found in service1'
	    return 1

	return 0


def clear_test(ezbox):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	print "Test clear ezbox test"
	ezbox.flush_ipvs()
	if ezbox.get_num_of_services()!= 0:
		print "after FLUSH all should be 0\n"
		return 1
	return 0
	

#===============================================================================
# main function
#===============================================================================
args = read_test_arg(sys.argv)	
init_log(args)
server_list, ezbox, vip_list = user_init(args['setup_num'])
init_ezbox(args,ezbox)
service0 = service(ezbox=ezbox, virtual_ip=vip_list[0], port='80', schedule_algorithm = 'source_hash')
service1 = service(ezbox=ezbox, virtual_ip=vip_list[1], port='80', schedule_algorithm = 'source_hash')


failed_tests = 0
rc = 0

print "Test 1 - Add servers to services"
rc = server_class_hash_add_test()
if rc:
	print 'Test1 failed !!!\n'
	failed_tests += 1
else:
	print 'Test1 passed !!!\n'
	
print "Test 2 - Delete servers from services"
rc = server_class_hash_delete_test()
if rc:
	print 'Test2 failed !!!\n'
	failed_tests += 1
else:
	print 'Test2 passed !!!\n'

rc = clear_test(ezbox)
if rc:
	print 'Clear test failed !!!\n'
	failed_tests += 1
else:
	print 'Clear test passed !!!\n'
# after test12 all services were cleared

if failed_tests == 0:
	print 'ALL Tests were passed !!!'
else:
	print 'Number of failed tests: %d' %failed_tests
	exit(1)
