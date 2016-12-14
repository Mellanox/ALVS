#!/usr/bin/env python

import sys
import os
import inspect
my_currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
my_parentdir = os.path.dirname(my_currentdir)
my_grandparentdir =  os.path.dirname(my_parentdir)
sys.path.append(my_grandparentdir)
sys.path.append(my_parentdir)
import random
from common_infra import *
from alvs_infra import *
from unit_tester import Unit_Tester

server_count   = 2
client_count   = 1
service_count  = 1

class One_service_2Servers_Fallback(Unit_Tester):
	
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = generic_init(setup_num, service_count, server_count, client_count)
		
		w = 3
		for s in self.test_resources['server_list']:
			s.vip = self.test_resources['vip_list'][0]
			s.weight = w
			w += 250
	
	def change_config(self, config):
		config['start_ezbox'] = True
	
	def run_user_test(self):
		port = '80'
		sched_algorithm = '-b sh-fallback'
		ezbox = self.test_resources['ezbox']
		server_list = self.test_resources['server_list']
		client_list = self.test_resources['client_list']
		vip_list = self.test_resources['vip_list']
	
		u_thresh_server2 = 4
		l_thresh_server2 = 2
		
		#changin server threshold
		server_list[1].l_thresh = l_thresh_server2
		server_list[1].u_thresh = u_thresh_server2
	# 	
		ezbox.add_service(vip_list[0], port,'sh', sched_algorithm)
		
		for server in server_list:
			print "adding server %s to service %s" %(server.ip, server.vip)
			ezbox.add_server(server.vip, port, server.ip, port, u_thresh = server.u_thresh ,l_thresh = server.l_thresh)
		
		print "**************************************************************************"
		print "PART 1 - Check that all connections are created - part on first server and part on server_list[1]"
		print "One of the servers will be overloaded - so another server should be found"
		
		    
		num_of_packets_1 = 7
		src_port_list_part_1 = []
		src_port_list_part_2 = []
		packet_list_to_send = []
		for i in range(num_of_packets_1):
		    # set the packet size
		    #packet_size = random.randint(60,1500)
		    packet_size = 150
		    random_source_port = '%02x %02x'%(random.randint(1,255),random.randint(1,255))
		
		    # create packet
		    data_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
		                             mac_sa=client_list[0].get_mac_adress(),
		                             ip_dst=ip_to_hex_display(vip_list[0]), #first_service.virtual_ip_hex_display,
		                             ip_src=ip_to_hex_display(client_list[0].ip),
		                             tcp_source_port = random_source_port,
		                             tcp_dst_port = '00 50', # port 80
		                             packet_length=packet_size)
		    data_packet.generate_packet()
		    
		    packet_list_to_send.append(data_packet.packet)
		    
		    src_port_list_part_1.append(int(random_source_port.replace(' ',''),16))
		     
		pcap_to_send_1 = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name=ALVSdir + 'verification/testing/dp/1serv_2serv_fallback_1.pcap')
		
		time.sleep(5) 
		server_list[0].capture_packets_from_service(vip_list[0])
		server_list[1].capture_packets_from_service(vip_list[0])
		
		    # send packet
		time.sleep(5)
		print "Send packets"
		client_list[0].send_packet_to_nps(pcap_to_send_1)
		time.sleep(120)
		
		    # send packet
		print "capture from server"
		packets_received_1 = server_list[0].stop_capture()
		print "Server 1  - received %d packets"%packets_received_1
		packets_received_2 = server_list[1].stop_capture()
		print "Server 2  - received %d packets"%packets_received_2
		
		sched_connections_on_server_1 = ezbox.get_server_connections_total_stats(0)
		print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server_1
		sched_connections_on_server_2 = ezbox.get_server_connections_total_stats(1)
		print "Server 2  - number of scheduled connections %s on server"%sched_connections_on_server_2
		
		error_stats = ezbox.get_error_stats()     
		print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']
		
		created_connections = 0
		server_is_unavailable_error = 0
		
		for src_port in src_port_list_part_1:   
		    connection=ezbox.get_connection(ip2int(vip_list[0]), 80, ip2int(client_list[0].ip) , src_port, 6)
		    if connection != None:
		        created_connections +=1
		    else:
		        server_is_unavailable_error +=1
		
		error_stats = ezbox.get_error_stats()     
		print "created_connections %d"%created_connections
		print "uncreated_connections %d"%server_is_unavailable_error
		expected_packets_received_2 = u_thresh_server2
		expected_packets_received_1 = num_of_packets_1 - expected_packets_received_2
		expected_packets_receieved = num_of_packets_1
		expected_num_of_errors = 0
		
		if (packets_received_1 == expected_packets_received_1 and
		    packets_received_2 == expected_packets_received_2 and
		    packets_received_1 + packets_received_2 == expected_packets_receieved and 
		    sched_connections_on_server_1 + sched_connections_on_server_2 == expected_packets_receieved and 
		    error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE'] == expected_num_of_errors and
		    (created_connections == expected_packets_receieved) and
		   (server_is_unavailable_error == expected_num_of_errors)):
		
		#     server_list[0].compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
		    print "PART1 Passed"
		else:
		    print "PART1 Faild"
		    exit(1)
		
		print "**************************************************************************"
		print "PART 2 - send more frames to create new connections" 
		print "Server 1 is overloaded, server 2 after modify u_thresh = 4 1 additional connection can be added"
		num_of_packets_2 = 7
		u_thresh = 4
		l_thresh = 2
		ezbox.modify_server(server_list[0].vip, port, server_list[0].ip, port, weight=3, u_thresh=u_thresh, l_thresh=l_thresh)
		ezbox.modify_server(server_list[1].vip, port, server_list[1].ip, port, weight=5, u_thresh=u_thresh, l_thresh=l_thresh)
		
		for i in range(num_of_packets_2):
		    packet_size = 150
		    random_source_port = '%02x %02x'%(random.randint(1,255),random.randint(1,255))
		
		    # create packet
		    data_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
		                             mac_sa=client_list[0].get_mac_adress(),
		                             ip_dst=ip_to_hex_display(vip_list[0]), #first_service.virtual_ip_hex_display,
		                             ip_src=ip_to_hex_display(client_list[0].ip),
		                             tcp_source_port = random_source_port,
		                             tcp_dst_port = '00 50', # port 80
		                             packet_length=packet_size)
		    data_packet.generate_packet()
		    
		    packet_list_to_send.append(data_packet.packet)
		    
		    src_port_list_part_2.append(int(random_source_port.replace(' ',''),16))
		     
		pcap_to_send_2 = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name=ALVSdir + 'verification/testing/dp/1serv_2serv_fallback_2.pcap')
		
		time.sleep(5) 
		server_list[0].capture_packets_from_service(vip_list[0])
		server_list[1].capture_packets_from_service(vip_list[0])
		
		    # send packet
		time.sleep(10)
		print "Send packets"
		client_list[0].send_packet_to_nps(pcap_to_send_2)
		
		time.sleep(120)
		    # send packet
		print "capture from server"
		packets_received_1 = server_list[0].stop_capture()
		print "Server 1  - received %d packets"%packets_received_1
		packets_received_2 = server_list[1].stop_capture()
		print "Server 2  - received %d packets"%packets_received_2
		
		sched_connections_on_server_1 = ezbox.get_server_connections_total_stats(0)
		print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server_1
		sched_connections_on_server_2 = ezbox.get_server_connections_total_stats(1)
		print "Server 2  - number of scheduled connections %s on server"%sched_connections_on_server_2
		
		error_stats = ezbox.get_error_stats()     
		print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']
		
		created_connections = 0
		server_is_unavailable_error = 0
		
		for src_port in src_port_list_part_1:   
		    connection=ezbox.get_connection(ip2int(vip_list[0]), 80, ip2int(client_list[0].ip) , src_port, 6)
		    if connection != None:
		        created_connections +=1
		    else:
		        server_is_unavailable_error +=1
		
		for src_port in src_port_list_part_2:   
		    connection=ezbox.get_connection(ip2int(vip_list[0]), 80, ip2int(client_list[0].ip) , src_port, 6)
		    if connection != None:
		        created_connections +=1
		    else:
		        server_is_unavailable_error +=1
		
		error_stats = ezbox.get_error_stats()     
		print "created_connections %d"%created_connections
		print "uncreated_connections %d"%server_is_unavailable_error
		num_of_packets = num_of_packets_1 + num_of_packets_2
		expected_packets_received_1 = min(u_thresh, num_of_packets)
		expected_packets_received_2 = min(u_thresh, num_of_packets)        #sched_conns from previouse part is 3, one new connection can be created upto u_thresh
		expected_num_of_errors = num_of_packets - expected_packets_received_1 - expected_packets_received_2
		expected_sched_connections = 8 #MMIN (u_thresh on server_list[0] + u_thresh on server_list[1], num_of_packets)
		if (packets_received_1 == expected_packets_received_1 and
		    packets_received_2 == expected_packets_received_2 and
		    sched_connections_on_server_1 + sched_connections_on_server_2 == expected_sched_connections and 
		    error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE'] == expected_num_of_errors and
		    (created_connections == expected_sched_connections) and
		   (server_is_unavailable_error == expected_num_of_errors)):
		
		#     server_list[0].compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
		    print "PART2 Passed"
		else:
		    print "PART2 Failed"
		    exit(1)
		    
	
current_test = One_service_2Servers_Fallback()
current_test.main()
