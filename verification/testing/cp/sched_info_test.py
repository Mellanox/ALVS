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
# from test_infra import *
from common_infra import *
from e2e_infra import *


#===============================================================================
# Test Globals
#===============================================================================
server_count   = 13
client_count   = 0
service_count  = 4

#defines
sh_sched_alg = 5
rr_sched_alg = 0
wrr_sched_alg = 1
bucket_size = 256

#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	dict = generic_init(setup_num, service_count, server_count, client_count)
	
	#servers 0-1 relate to the first service will be defined as sh
	w = 1
	for i in range (0,2):
		dict['server_list'][i].vip = dict['vip_list'][0]
		dict['server_list'][i].weight = w
	#the server 2 relates to the second service will be defined as wsh
	dict['server_list'][2].vip = dict['vip_list'][1]
	dict['server_list'][2].w = 4
	#servers 3-6 relate to the third service will be defined as rr
	w = 1
	for i in range (3,7):
		dict['server_list'][i].vip = dict['vip_list'][2]
		dict['server_list'][i].weight = w
		w +=1
	#servers 7-9 relate to the third service will be defined as wrr
	w = 4
	for i in range (7,10):
		dict['server_list'][i].vip = dict['vip_list'][3]
		dict['server_list'][i].weight = w
		w -=1
	
	#2 more servers that will be added to sh service in test 10
	dict['server_list'][10].vip = dict['vip_list'][0]
	dict['server_list'][10].weight = 4
	dict['server_list'][11].vip = dict['vip_list'][0]
	dict['server_list'][11].weight = 10
	
	#1 more server that will be added to rr service in test 10
	dict['server_list'][12].vip = dict['vip_list'][2]
	dict['server_list'][12].weight = 4
	return dict

def init_services(ezbox,vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	port = '80'
	ezbox.add_service(vip_list[0], port, sched_alg='sh', sched_alg_opt='')
	ezbox.add_service(vip_list[1], port, sched_alg='sh', sched_alg_opt='')
	ezbox.add_service(vip_list[2], port, sched_alg='rr', sched_alg_opt='')
	ezbox.add_service(vip_list[3], port, sched_alg='wrr', sched_alg_opt='')

def check_service_info(expected_service_info, service_info):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	error = 0
	if service_info['sched_alg'] != expected_service_info['sched_alg'] :
		print "ERROR, sched_alg  = %d expected = %d\n" % (service_info['sched_alg'], expected_service_info['sched_alg'])
		error = 1

	if service_info['sched_info_entries'] != expected_service_info['sched_info_entries'] :
		print "ERROR, sched_info_entries = %d, expected = %d\n" % (service_info['sched_info_entries'], expected_service_info['sched_info_entries'])
		error = 1

	if service_info['flags'] != expected_service_info['flags'] :
		print "ERROR, flags  = %d expected = %d\n" % (service_info['flags'], expected_service_info['flags'])
		error = 1

	if len(service_info['sched_info']) != len(expected_service_info['sched_info']) :
		print "ERROR, len(sched_info)  = %d expected = %d\n" % (len(service_info['sched_info']), len(expected_service_info['sched_info']))
		error = 1

	if cmp(service_info['sched_info'], expected_service_info['sched_info']) != 0 :
		print "ERROR, sched_info is not as expected\n"
		error = 1
	
	return error


def add_service_test_sh(server_list, ezbox, vip):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	port = '80'
	print "Test 1 - create first service with scheduling algorithm: source_hash"
	ezbox.add_server(server_list[0].vip, port, server_list[0].ip, port, weight=1)
	ezbox.add_server(server_list[1].vip, port, server_list[1].ip, port, weight=1)
	
	service_info = ezbox.get_service(ip2int(vip), port=80, protocol = 6)
	if service_info == None:
		print "ERROR, sh service wasn't created as expected"
		return 1
	print "service info:"
	print service_info
	
	expected_sched_info = []
	while len(expected_sched_info) < bucket_size :
		expected_sched_info += [0]
		expected_sched_info += [1]
		
	expected_service_info = {'sched_alg' : sh_sched_alg,
							 'sched_info_entries' : bucket_size,
							 'flags' : 0,
							 'sched_info' : expected_sched_info,
							 'stats_base' : 0}
	
	print "expected service info:"
	print expected_service_info
	
	rc = check_service_info(expected_service_info, service_info)	
	if rc:
		print "Error in comparing service info"
		return 1
	
	return 0


def add_service_test_weighted_sh(server_list, ezbox, vip):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	port = '80'
	print "Test 2 - create second service with scheduling algorithm: source_hash with weights"
	ezbox.add_server(server_list[2].vip, port, server_list[2].ip, port, weight=4)
	
	service_info = ezbox.get_service(ip2int(vip), port=80, protocol = 6)
	if service_info == None:
		print "ERROR, wsh service wasn't created as expected"
		return 1
	print "service info:"
	print service_info
	
	expected_sched_info = []	
	while len(expected_sched_info) < bucket_size :
		expected_sched_info += [2]
	
	expected_service_info = {'sched_alg' : sh_sched_alg,
							 'sched_info_entries' : bucket_size,
							 'flags' : 0,
							 'sched_info' : expected_sched_info,
							 'stats_base' : 0}
	
	
	print "expected service info:"
	print expected_service_info
	
	rc = check_service_info(expected_service_info, service_info)	
	if rc:
		print "Error in comparing service info"
		return 1
	
	return 0


def add_service_test_rr(server_list, ezbox, vip):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	port = '80'
	print "Test 3 - create third service with scheduling algorithm: rr"
	#adding servers 3-6 to rr service
	for i in range(3, 7):
		ezbox.add_server(server_list[i].vip, port, server_list[i].ip, port, weight=server_list[i].weight)
	
	service_info = ezbox.get_service(ip2int(vip), port=80, protocol = 6)
	if service_info == None:
		print "ERROR, rr service wasn't created as expected"
		return 1
	print "service info:"
	print service_info
	
	expected_sched_info = []	
	expected_sched_info += [3]
	expected_sched_info += [4]
	expected_sched_info += [5]
	expected_sched_info += [6]
	
	expected_service_info = {'sched_alg' : rr_sched_alg,
							 'sched_info_entries' : 4,
							 'flags' : 0,
							 'sched_info' : expected_sched_info,
							 'stats_base' : 0}
	
	print "expected service info:"
	print expected_service_info
	
	rc = check_service_info(expected_service_info, service_info)	
	if rc:
		print "Error in comparing service info"
		return 1
	
	return 0


def add_service_test_wrr(server_list, ezbox, vip):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	port = '80'
	print "Test 4 - create fourth service with scheduling algorithm: wrr"
	#adding servers 7,8,9 to the forth server which is wrr
	for i in range(7, 10):
		ezbox.add_server(server_list[i].vip, port, server_list[i].ip, port, weight=server_list[i].weight)
	
	service_info = ezbox.get_service(ip2int(vip), port=80, protocol = 6)
	if service_info == None:
		print "ERROR, wrr service wasn't created as expected"
		return 1
	
	print "service info:"
	print service_info
	
	expected_sched_info = []	
	expected_sched_info += [7]
	expected_sched_info += [7]
	expected_sched_info += [8]
	expected_sched_info += [7]
	expected_sched_info += [8]
	expected_sched_info += [9]
	expected_sched_info += [7]
	expected_sched_info += [8]
	expected_sched_info += [9]
	
	expected_service_info = {'sched_alg' : wrr_sched_alg,
							 'sched_info_entries' : 9,
							 'flags' : 0,
							 'sched_info' : expected_sched_info,
							 'stats_base' : 0}
	
	print "expected service info:"
	print expected_service_info
	
	rc = check_service_info(expected_service_info, service_info)	
	if rc:
		print "Error in comparing service info"
		return 1
	
	return 0


def modify_rr_service_test(server_list, ezbox, vip):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	port = '80'
	print "Test 5 - modify third service with scheduling algorithm: rr to wrr"
	
	ezbox.modify_service(vip, port=80, sched_alg='wrr', sched_alg_opt='')
	
	service_info = ezbox.get_service(ip2int(vip), port=80, protocol = 6)
	if service_info == None:
		print "ERROR, wrr service wasn't created as expected"
		return 1
	print "service info:"
	print service_info
	
	expected_sched_info = []	
	expected_sched_info += [6]
	expected_sched_info += [5]
	expected_sched_info += [6]
	expected_sched_info += [4]
	expected_sched_info += [5]
	expected_sched_info += [6]
	expected_sched_info += [3]
	expected_sched_info += [4]
	expected_sched_info += [5]
	expected_sched_info += [6]
	
	expected_service_info = {'sched_alg' : wrr_sched_alg,
							 'sched_info_entries' : 10,
							 'flags' : 0,
							 'sched_info' : expected_sched_info,
							 'stats_base' : 0}
	
	print "expected service info:"
	print expected_service_info
	
	rc = check_service_info(expected_service_info, service_info)	
	if rc:
		print "Error in comparing service info"
		return 1
	
	return 0


def remove_wrr_sh_service_test(ezbox, sh_vip, wrr_vip):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	print "Test 6 - remove source hash & RR services"
	
# 	sh_service.remove_service();
	ezbox.delete_service(sh_vip, '80')
	
	if ezbox.get_num_of_services() != 3 :
		print "ERROR, number of services should be 3 after removing sh service. Current number of services = %d" %(ezbox.get_num_of_services())
		return 1
	
	service_info = ezbox.get_service(ip2int(sh_vip), port=80, protocol = 6)
	if service_info != None:
		print "ERROR, sh service wasn't removed"
		return 1
	
# 	wrr_service.remove_service();
	ezbox.delete_service(wrr_vip, '80')
	
	if ezbox.get_num_of_services() != 2 :
		print "ERROR, number of services should be 2 after removing wrr service. Current number of services = %d" %(ezbox.get_num_of_services())
		return 1
	
	service_info = ezbox.get_service(ip2int(wrr_vip), port=80, protocol = 6)
	if service_info != None:
		print "ERROR, wrr service wasn't removed"
		return 1
	
	return 0


def add_removed_services_test(ezbox, server_list, sh_vip, wrr_vip):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	port = '80'
	print "Test 7 - add removed services from previous test"
	
	ezbox.add_service(sh_vip, '80', sched_alg='sh', sched_alg_opt='')
	
	server_list[0].weight = 5
	server_list[1].weight = 2
	ezbox.add_server(server_list[0].vip, port, server_list[0].ip, port, weight=5)
	ezbox.add_server(server_list[1].vip, port, server_list[1].ip, port, weight=2)
	
	service_info = ezbox.get_service(ip2int(sh_vip), port=80, protocol = 6)
	if service_info == None:
		print "ERROR, sh service wasn't created as expected\n"
		return 1
	
	print "service info:"
	print service_info
	
	expected_sched_info = []	
	while len(expected_sched_info) < bucket_size :
		expected_sched_info += [10]*5
		expected_sched_info += [11]*2
	expected_sched_info = expected_sched_info[:bucket_size]
	
	expected_service_info = {'sched_alg' : sh_sched_alg,
							 'sched_info_entries' : bucket_size,
							 'flags' : 0,
							 'sched_info' : expected_sched_info,
							 'stats_base' : 0}
	
	print "expected service info:"
	print expected_service_info
	
	rc = check_service_info(expected_service_info, service_info)	
	if rc:
		print "Error in comparing service info"
		return 1
	
	if ezbox.get_num_of_services() != 3 :
		print "ERROR, number of services should be 3 after adding sh service. Current number of services = %d" %(ezbox.get_num_of_services())
		return 1
	
# 	wrr_service = service(ezbox=ezbox, virtual_ip=wrr_vip, port='80', schedule_algorithm = 'wrr')
	ezbox.add_service(wrr_vip, '80', sched_alg='wrr', sched_alg_opt='')
	for i in range(7,10):
		server_list[i].weight = 100
		ezbox.add_server(server_list[i].vip, port, server_list[i].ip, port, weight=100)
	
	service_info = ezbox.get_service(ip2int(wrr_vip), port=80, protocol = 6)
	if service_info == None:
		print "ERROR, wrr service wasn't created as expected\n"
		return 1
	
	print "service info:"
	print service_info
	
	expected_sched_info = []	
	expected_sched_info += [12]
	expected_sched_info += [13]
	expected_sched_info += [14]
	
	expected_service_info = {'sched_alg' : wrr_sched_alg,
							 'sched_info_entries' : 3,
							 'flags' : 0,
							 'sched_info' : expected_sched_info,
							 'stats_base' : 0}
	
	print "expected service info:"
	print expected_service_info
	
	rc = check_service_info(expected_service_info, service_info)	
	if rc:
		print "Error in comparing service info"
		return 1
	
	if ezbox.get_num_of_services() != 4 :
		print "ERROR, number of services should be 4 after adding wrr service. Current number of services = %d" %(ezbox.get_num_of_services())
		return 1
	return 0


def modify_servers_test(ezbox, server_list, w_sh_vip, wrr_vip):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	port = '80'
	print "Test 8 - modify servers of weighted sh & rr services"
	ezbox.modify_server(server_list[2].vip, port, server_list[2].ip, port, weight=0)
	
	service_info = ezbox.get_service(ip2int(w_sh_vip), port=80, protocol = 6)
	if service_info == None:
		print "ERROR, weighted sh service wasn't created as expected\n"
		return 1
	
	print "service info:"
	print service_info
	
	expected_sched_info = []

	expected_service_info = {'sched_alg' : sh_sched_alg,
							 'sched_info_entries' : 0,
							 'flags' : 0,
							 'sched_info' : expected_sched_info,
							 'stats_base' : 0}
	
	print "expected service info:"
	print expected_service_info
	
	rc = check_service_info(expected_service_info, service_info)	
	if rc:
		print "Error in comparing service info"
		return 1
# 	
	ezbox.modify_server(server_list[7].vip, port, server_list[7].ip, port, weight=0)
	ezbox.modify_server(server_list[9].vip, port, server_list[9].ip, port, weight=50)
	
	service_info = ezbox.get_service(ip2int(wrr_vip), port=80, protocol = 6)
	if service_info == None:
		print "ERROR, wrr service wasn't created as expected\n"
		return 1
	
	print "service info:"
	print service_info
	
	expected_sched_info = []
	expected_sched_info += [13]
	expected_sched_info += [13]
	expected_sched_info += [14]

	expected_service_info = {'sched_alg' : wrr_sched_alg,
							 'sched_info_entries' : 3,
							 'flags' : 0,
							 'sched_info' : expected_sched_info,
							 'stats_base' : 0}
	
	print "expected service info:"
	print expected_service_info
	
	rc = check_service_info(expected_service_info, service_info)	
	if rc:
		print "Error in comparing service info"
		return 1
	
	return 0


def wrr_wo_gcd_test(ezbox, server_list, wrr_vip):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	port = '80'
	print "Test 9 - add servers for wrr service with large weights w/o gcd"
	ezbox.delete_service(wrr_vip, '80')
	
	ezbox.add_service(wrr_vip, port, sched_alg='wrr', sched_alg_opt='')

	server_list[7].weight = 200
	server_list[8].weight = 199
	server_list[9].weight = 198
	ezbox.add_server(server_list[7].vip, port, server_list[7].ip, port, weight=server_list[7].weight)
	ezbox.add_server(server_list[8].vip, port, server_list[8].ip, port, weight=server_list[8].weight)
	ezbox.add_server(server_list[9].vip, port, server_list[9].ip, port, weight=server_list[9].weight)
	
	service_info = ezbox.get_service(ip2int(wrr_vip), port=80, protocol = 6)
	if service_info == None:
		print "ERROR, wrr service wasn't created as expected\n"
		return 1
	
	print "service info:"
	print service_info
	
	expected_sched_info = []
	expected_sched_info += [15]
	expected_sched_info += [15]
	expected_sched_info += [16]
	while len(expected_sched_info) < bucket_size :
		expected_sched_info += [15]
		expected_sched_info += [16]
		expected_sched_info += [17]
	expected_sched_info = expected_sched_info[:bucket_size]

	expected_service_info = {'sched_alg' : wrr_sched_alg,
							 'sched_info_entries' : bucket_size,
							 'flags' : 0,
							 'sched_info' : expected_sched_info,
							 'stats_base' : 0}
	
	print "expected service info:"
	print expected_service_info
	
	rc = check_service_info(expected_service_info, service_info)	
	if rc:
		print "Error in comparing service info"
		return 1
	
	return 0


def add_servers_test(ezbox, server_list, sh_vip, rr_vip):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	port = '80'
	print "Test 10 - add more servers to sh & rr services"
	ezbox.add_server(server_list[10].vip, port, server_list[10].ip, port, weight= 4)
	ezbox.add_server(server_list[11].vip, port, server_list[11].ip, port, weight=10)
	
	service_info = ezbox.get_service(ip2int(sh_vip), port=80, protocol = 6)
	if service_info == None:
		print "ERROR, sh service wasn't created as expected\n"
		return 1
	
	print "service info:"
	print service_info
	
	expected_sched_info = []	
	while len(expected_sched_info) < bucket_size :
		expected_sched_info += [10]*5
		expected_sched_info += [11]*2
		expected_sched_info += [18]*4
		expected_sched_info += [19]*10
	expected_sched_info = expected_sched_info[:bucket_size]
	
	expected_service_info = {'sched_alg' : sh_sched_alg,
							 'sched_info_entries' : bucket_size,
							 'flags' : 0,
							 'sched_info' : expected_sched_info,
							 'stats_base' : 0}
	
	print "expected service info:"
	print expected_service_info
	
	rc = check_service_info(expected_service_info, service_info)	
	if rc:
		print "Error in comparing service info"
		return 1
	
	# reset to rr after test 5
	ezbox.modify_service(rr_vip, port=80, sched_alg='rr', sched_alg_opt='')
	ezbox.add_server(server_list[12].vip, port, server_list[12].ip, port, weight=4)
	
	service_info = ezbox.get_service(ip2int(rr_vip), port=80, protocol = 6)
	if service_info == None:
		print "ERROR, wrr service wasn't created as expected"
		return 1
	print "service info:"
	print service_info
	
	expected_sched_info = []	
	expected_sched_info += [3]
	expected_sched_info += [4]
	expected_sched_info += [5]
	expected_sched_info += [6]
	expected_sched_info += [20]
	
	expected_service_info = {'sched_alg' : rr_sched_alg,
							 'sched_info_entries' : 5,
							 'flags' : 0,
							 'sched_info' : expected_sched_info,
							 'stats_base' : 0}
	
	print "expected service info:"
	print expected_service_info
	
	rc = check_service_info(expected_service_info, service_info)	
	if rc:
		print "Error in comparing service info"
		return 1
	
	return 0


def modify_rr_service_to_sh_test(ezbox, server_list, rr_vip):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	print "Test 11 - modify rr service to sh"
	
	ezbox.modify_service(rr_vip, port=80, sched_alg='sh', sched_alg_opt='')
	
	service_info = ezbox.get_service(ip2int(rr_vip), port=80, protocol = 6)
	if service_info == None:
		print "ERROR, sh service wasn't created as expected"
		return 1
	
	print "service info:"
	print service_info
	
	expected_sched_info = []
	while len(expected_sched_info) < bucket_size :
		expected_sched_info += [3]*1
		expected_sched_info += [4]*2
		expected_sched_info += [5]*3
		expected_sched_info += [6]*4
		expected_sched_info += [20]*4
	expected_sched_info = expected_sched_info[:bucket_size]
	
	expected_service_info = {'sched_alg' : sh_sched_alg,
							 'sched_info_entries' : bucket_size,
							 'flags' : 0,
							 'sched_info' : expected_sched_info,
							 'stats_base' : 0}
	
	print "expected service info:"
	print expected_service_info
	
	rc = check_service_info(expected_service_info, service_info)	
	if rc:
		print "Error in comparing service info"
		return 1
	
	return 0

def clear_test(ezbox):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	print "Test 12 - clear ezbox test"
	ezbox.flush_ipvs()
	if ezbox.get_num_of_services()!= 0:
		print "after FLUSH all should be 0\n"
		return 1
	return 0
	

#===============================================================================
# main function
#===============================================================================

def main():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	config = fill_default_config(generic_main())
	
	config['start_ezbox'] = True
	
	dict = user_init(config['setup_num'])
	
	init_players(dict, config)
	
	server_list, ezbox, client_list, vip_list = convert_generic_init_to_user_format(dict)
	
	run_user_test(server_list, ezbox, client_list, vip_list)
	
	clean_players(dict, True, config['stop_ezbox'])
	
	print "Test Passed"

def run_user_test(server_list, ezbox, client_list, vip_list):
	init_services(ezbox,vip_list)
	sh_vip = vip_list[0]
	w_sh_vip = vip_list[1]
	rr_vip = vip_list[2]
	wrr_vip = vip_list[3]
	
	failed_tests = 0
	rc = 0
	
	rc = add_service_test_sh(server_list, ezbox, sh_vip)
	if rc:
		print 'Test1 failed !!!\n'
		failed_tests += 1
	else:
		print 'Test1 passed !!!\n'
	# after test1, sh_service has 2 servers 0 & 1 (servers with index 0 & 1)with weight = 1
	
	rc = add_service_test_weighted_sh(server_list, ezbox, w_sh_vip)
	if rc:
		print 'Test2 failed !!!\n'
		failed_tests += 1
	else:
		print 'Test2 passed !!!\n'
	# after test2, wsh_service has server 2 (servers with index 2) with weight = 4
	
	rc = add_service_test_rr(server_list, ezbox, rr_vip)
	if rc:
		print 'Test3 failed !!!\n'
		failed_tests += 1
	else:
		print 'Test3 passed !!!\n'
	# after test3, rr_service has 4 servers 3,4,5,6 (servers with index 3,4,5,6) with weights 1,2,3,4
	
	rc = add_service_test_wrr(server_list, ezbox, wrr_vip)
	if rc:
		print 'Test4 failed !!!\n'
		failed_tests += 1
	else:
		print 'Test4 passed !!!\n'
	# after test4, wrr_service has 3 servers 0,1 & 2 (getting index 7,8,9) with weights 4,3,2
	
	rc = modify_rr_service_test(server_list, ezbox, rr_vip)
	if rc:
		print 'Test5 failed !!!\n'
		failed_tests += 1
	else:
		print 'Test5 passed !!!\n'
	# after test5, rr_service has modified to wrr and therefore sched info bucket will has same servers (index 3,4,5,6 with weights 1,2,3,4)
	
	rc = remove_wrr_sh_service_test(ezbox, sh_vip, wrr_vip)
	if rc:
		print 'Test6 failed !!!\n'
		failed_tests += 1
	else:
		print 'Test6 passed !!!\n'
	# after test6, sh_service & wrr_service removed.
	
	rc = add_removed_services_test(ezbox, server_list, sh_vip, wrr_vip)
	if rc:
		print 'Test7 failed !!!\n'
		failed_tests += 1
	else:
		print 'Test7 passed !!!\n'
	# after test7, sh_service has 2 servers 0,1 (getting index 10 & 11) with weights 2,5
	#			   wrr_service has three servers 7,8,9 (getting index 14,13,12) with weights 100,100,100
	
	rc = modify_servers_test(ezbox, server_list, w_sh_vip, wrr_vip)
	if rc:
		print 'Test8 failed !!!\n'
		failed_tests += 1
	else:
		print 'Test8 passed !!!\n'
	# after test8, server 2 (index 2) modified and given weight = 0 in wsh_service which become without servers
	#				in wrr_service: server 7 (index 12) get weight = 0 & server 9 get weight = 50, service now has 2 servers 2 & 3 (index 13,12) with weights 50,100
	
	rc = wrr_wo_gcd_test(ezbox, server_list, wrr_vip)
	if rc:
		print 'Test9 failed !!!\n'
		failed_tests += 1
	else:
		print 'Test9 passed !!!\n'
	# after test9, wrr_service was cleared and initialize with 3 servers 7,8,9 (getting index 15,16,17) with weights 200,199,198
	
	rc = add_servers_test(ezbox, server_list, sh_vip, rr_vip)
	if rc:
		print 'Test10 failed !!!\n'
		failed_tests += 1
	else:
		print 'Test10 passed !!!\n'
	# after test10, sh_service has 2 additional servers 10 & 11 (getting index 18,19) with weights 4 & 10.
	# regarding to rr_service service, after test5 it has modified to be wrr and in this test it's modified to be rr (including servers 3,4,5,6 with index 3,4,5,6 with weights 1,2,3,4) 
	# and getting a new server 12 with weight 4 (getting index 20)
	
	rc = modify_rr_service_to_sh_test(ezbox, server_list, rr_vip)
	if rc:
		print 'Test11 failed !!!\n'
		failed_tests += 1
	else:
		print 'Test11 passed !!!\n'
	# after test 11, rr_service modified to sh therefore it will include servers 3,4,5,6,12 with weights 1,2,3,4,4 and index 3,4,5,6,20
	
	rc = clear_test(ezbox)
	if rc:
		print 'Test12 failed !!!\n'
		failed_tests += 1
	else:
		print 'Test12 passed !!!\n'
	# after test12 all services were cleared
	
	if failed_tests == 0:
		print 'ALL Tests were passed !!!'
		exit(0)
	else:
		print 'Number of failed tests: %d' %failed_tests
		exit(1)

main()

