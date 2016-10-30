#!/usr/bin/env python

import sys
sys.path.append("verification/testing")
#from test_infra import * 
import random
from common_infra import *
from e2e_infra import *

server_count   = 1
client_count   = 1
service_count  = 1

def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
	dict = generic_init(setup_num, service_count, server_count, client_count)
	
	w = 1
	for s in dict['server_list']:
		s.vip = dict['vip_list'][0]
		s.weight = w
	
	return dict

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
	port = '80'
	sched_algorithm = 'sh'

	print "service %s is set with %s scheduling algorithm" %(vip_list[0],sched_algorithm)
	ezbox.add_service(vip_list[0], port, sched_alg=sched_algorithm, sched_alg_opt='')
	ezbox.add_server(server_list[0].vip, port, server_list[0].ip, port)

	# create packet
	data_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
							 mac_sa=client_list[0].mac_address.replace(':',' '),
							 ip_dst=ip_to_hex_display(vip_list[0]),
	                         ip_src=ip_to_hex_display(client_list[0].ip),
							 tcp_source_port = '00 00',
							 tcp_dst_port = '00 50', # port 80
							 tcp_reset_flag = False,
							 tcp_fin_flag = False,
							 packet_length=64)
	data_packet.generate_packet()

	##########################################################################
	# send packet to slow path (connection is not exist)
	##########################################################################
	
	# verify that connection is not exist
	connection=ezbox.get_connection(ip2int(vip_list[0]), port, ip2int(client_list[0].ip) , 0, 6)
	if connection != None:
		print "ERROR, exist , fail\n"
		exit(1)
	

	# capture all packets on server
	server_list[0].capture_packets_from_service(vip_list[0])

	# send packet
	print "Send Data Packet to Service"
	client_list[0].send_packet_to_nps(data_packet.pcap_file_name)

	# verify that connection exist
	time.sleep(0.5)
	connection=ezbox.get_connection(ip2int(vip_list[0]), port, ip2int(client_list[0].ip) , 0, 6)
	if connection == None:
		print "ERROR, connection was created, even though server is not exist\n"
		exit(1)
	print "Connection exist"

	# check how many packets were captured
	packets_received = server_list[0].stop_capture()
 
	if packets_received !=1:
		print "ERROR, wrong number of packets were received on server"
		exit(1)

	# wait 5 minutes
	print "Wait 5 minutes and send data packet to this connection again"
	time.sleep(60*5)

	# verify that connection exist
	connection=ezbox.get_connection(ip2int(vip_list[0]), port, ip2int(client_list[0].ip) , 0, 6)
	if connection == None:
		print "ERROR, connection was created, even though server is not exist\n"
		exit(1)
			
	# send data packet again
	print "Send Data Packet to Service again"
	client_list[0].send_packet_to_nps(data_packet.pcap_file_name)

	
	# wait 5 minutes
	print "Wait 16 minutes and check that connection still exist"
	
	for i in range(16):
		# verify that connection exist
		connection=ezbox.get_connection(ip2int(vip_list[0]), port, ip2int(client_list[0].ip) , 0, 6)
		if connection == None:
			print "ERROR, connection was deleted..\n"
			exit(1)
		print "Connection Exist"
		
		time.sleep(60)
		
	
	time.sleep(20)
	
	# check that connection was deleted
	connection=ezbox.get_connection(ip2int(vip_list[0]), port, ip2int(client_list[0].ip) , 0, 6)
	if connection != None:
		print "ERROR, still exist, connection should be deleted..\n"
		exit(1)
	
	
	print "Connection was deleted after established time"
	print
	
main()
