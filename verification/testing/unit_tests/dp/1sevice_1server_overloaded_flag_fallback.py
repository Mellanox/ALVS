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
from e2e_infra import *
from unit_tester import Unit_Tester

server_count   = 1
client_count   = 1
service_count  = 1

class One_Service_1Server_Overloaded_Flag_Fallback(Unit_Tester):

	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = generic_init(setup_num, service_count, server_count, client_count)
		
		w = 1
		for s in self.test_resources['server_list']:
			s.vip = self.test_resources['vip_list'][0]
			s.weight = w
	
	def change_config(self, config):
		config['start_ezbox'] = True
	
	def run_user_test(self):
		port = '80'
		sched_algorithm = 'sh'
		ezbox = self.test_resources['ezbox']
		server_list = self.test_resources['server_list']
		client_list = self.test_resources['client_list']
		vip_list = self.test_resources['vip_list']
	
		u_thresh = 4
		l_thresh = 2
		ezbox.add_service(vip_list[0], port, sched_alg=sched_algorithm, sched_alg_opt='')
		ezbox.add_server(server_list[0].vip, port, server_list[0].ip, port, u_thresh = u_thresh, l_thresh = l_thresh)
		
		print "**************************************************************************"
		print "PART 1 - U_THRESH is upper bound for new created connections"
		    
		 # create packets
		packet_list_to_send = []
		error_stats = []
		long_counters = []
		num_of_packets = 7
		src_port_list = []
		for i in range(num_of_packets):
		    packet_size = 150
		    random_source_port = '%02x %02x'%(random.randint(1,255),random.randint(1,255))
		
		    # create packet
		    data_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
		                             mac_sa=client_list[0].mac_address.replace(':',' '),
		                             ip_dst=ip_to_hex_display(vip_list[0]),
		                             ip_src=ip_to_hex_display(client_list[0].ip),
		                             tcp_source_port = random_source_port,
		                             tcp_dst_port = '00 50', # port 80
		                             packet_length=packet_size)
		    data_packet.generate_packet()
		    
		    packet_list_to_send.append(data_packet.packet)
		    
		    src_port_list.append(int(random_source_port.replace(' ',''),16))
		     
		pcap_to_send = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name=ALVSdir + 'verification/testing/dp/temp_packet.pcap')
		
		time.sleep(2) 
		server_list[0].capture_packets_from_service(vip_list[0])
		   
		    # send packet
		time.sleep(5)
		print "Send packets"
		client_list[0].send_packet_to_nps(pcap_to_send)
		
		time.sleep(60)
		print "capture from server"
		packets_received_1 = server_list[0].stop_capture()
		print "Server 1  - received %d packets"%packets_received_1
		sched_connections_on_server = ezbox.get_server_connections_total_stats(0)
		print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server
		
		error_stats = ezbox.get_error_stats()     
		print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']
		
		
		expected_packets_receieved = 0
		expected_sched_connections_on_server = 0
		expected_num_of_errors = 0
		
		if (u_thresh != 0):
		    expected_packets_receieved = min(u_thresh, num_of_packets)
		else:
		    expected_packets_receieved = num_of_packets
		    
		if (u_thresh != 0):
		    expected_sched_connections_on_server = min(u_thresh, num_of_packets)
		else:
		    expected_sched_connections_on_server = num_of_packets
		    
		if (u_thresh != 0):
		    expected_num_of_errors = abs(num_of_packets - u_thresh)
		
		created_connections = 0
		server_is_unavailable_error = 0
		
		for src_port in src_port_list:   
		    connection=ezbox.get_connection(ip2int(vip_list[0]), 80, ip2int(client_list[0].ip) , src_port, 6)
		    if connection != None:
		        created_connections +=1
		    else:
		        server_is_unavailable_error +=1
		error_stats = ezbox.get_error_stats()     
		
		if (packets_received_1 == expected_packets_receieved and 
		    sched_connections_on_server == expected_sched_connections_on_server and 
		    error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE'] == expected_num_of_errors and
		    (created_connections == expected_sched_connections_on_server) and
		   (expected_num_of_errors == server_is_unavailable_error)):
		
		#     server_list[0].compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
		    print "PART1 Passed"
		else:
		    print "PART1 Faild"
		    exit(1)
	     
		print "**************************************************************************"
		print "PART 2 - U_THRESH  = L_THRESH = 0 - disable feature - allow all connections"
		
		u_thresh = 0
		l_thresh = 0
		ezbox.modify_server(server_list[0].vip, port, server_list[0].ip, port, weight=1, u_thresh=u_thresh, l_thresh=l_thresh)
		
		time.sleep(2) 
		server_list[0].capture_packets_from_service(vip_list[0])
		   
		    # send packet
		time.sleep(5)
		print "Send packets"
		client_list[0].send_packet_to_nps(pcap_to_send)
		  
		time.sleep(60)
		print "capture from server"
		packets_received_1 = server_list[0].stop_capture()
		
		print "Server 1  - received %d packets"%packets_received_1
		
		sched_connections_on_server = ezbox.get_server_connections_total_stats(0)
		print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server
		
		error_stats = ezbox.get_error_stats()     
		print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']
		
		
		expected_packets_receieved = 0
		expected_sched_connections_on_server = 0
		
		if (u_thresh != 0):
		    expected_packets_receieved = min(u_thresh, num_of_packets)
		else:
		    expected_packets_receieved = num_of_packets
		    
		if (u_thresh != 0):
		    expected_sched_connections_on_server = min(u_thresh, num_of_packets)
		else:
		    expected_sched_connections_on_server = num_of_packets
		    
		if (u_thresh != 0):
		    expected_num_of_errors = abs(num_of_packets - u_thresh)
		
		created_connections = 0
		server_is_unavailable_error = 0
		
		for src_port in src_port_list:   
		    connection=ezbox.get_connection(ip2int(vip_list[0]), 80, ip2int(client_list[0].ip) , src_port, 6)
		    if connection != None:
		        created_connections +=1
		    else:
		        server_is_unavailable_error +=1
		error_stats = ezbox.get_error_stats()     
		print "created_connections %d"%created_connections
		print "uncreated_connections %d"%server_is_unavailable_error
		
		if (packets_received_1 == expected_packets_receieved and 
		    sched_connections_on_server == expected_sched_connections_on_server and 
		    error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE'] == expected_num_of_errors and
		    (created_connections == expected_sched_connections_on_server) and
		    (server_is_unavailable_error == 0)):
		
		#     server_list[0].compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
		    print "PART2 Passed"
		else:
		    print "PART2 Failed"
		    exit(1)
	
		print "**************************************************************************"
		print "PART 3 - remove connection below l_thresh and check OVERLOADED is OFF" 
		u_thresh = 6
		l_thresh = 3
		ezbox.modify_server(server_list[0].vip, port, server_list[0].ip, port, weight=1, u_thresh=u_thresh, l_thresh=l_thresh)
		
		print "PART 3 - A - try create new connection u_thresh = %d l_thresh = %d current_connection = %d"%(u_thresh, l_thresh, sched_connections_on_server)
		print "modify server with u_thresh %d instead of 0 and l_thresh %d instead of 0"%(u_thresh, l_thresh)
		print "number of current_connection %d higher than  u_thresh = %d "%(sched_connections_on_server, u_thresh)
		print "New connection shouldn't be created"
		
		num_of_packets = 1
		packet_size = 150
		random_source_port = '%02x %02x'%(random.randint(0,255),random.randint(0,255))
		
		# create packet
		data_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
		                         mac_sa=client_list[0].mac_address.replace(':',' '),
	                             ip_dst=ip_to_hex_display(vip_list[0]),
	                             ip_src=ip_to_hex_display(client_list[0].ip),
		                         tcp_source_port = random_source_port,
		                         tcp_dst_port = '00 50', # port 80
		                         packet_length=packet_size)
		data_packet.generate_packet()
		
		time.sleep(2) 
		server_list[0].capture_packets_from_service(vip_list[0])
		
		pcap_to_send_part3 = create_pcap_file(packets_list=[data_packet.packet], output_pcap_file_name=ALVSdir + 'verification/testing/dp/temp_packet.pcap')
		    
		    # send packet
		time.sleep(5)
		print "Send packets"
		client_list[0].send_packet_to_nps(pcap_to_send_part3)
		  
		time.sleep(60)
		print "capture from server"
		packets_received_1 = server_list[0].stop_capture()
		
		print "Server 1  - received %d packets"%packets_received_1
		
		sched_connections_on_server = ezbox.get_server_connections_total_stats(0)
		print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server
		
		error_stats = ezbox.get_error_stats()     
		print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']
		
		created_connections = 0
		server_is_unavailable_error = 0
		
		for src_port in src_port_list:   
		    connection=ezbox.get_connection(ip2int(vip_list[0]), 80, ip2int(client_list[0].ip) , src_port, 6)
		    if connection != None:
		        created_connections +=1
		    else:
		        server_is_unavailable_error +=1
		
		connection=ezbox.get_connection(ip2int(vip_list[0]), 80, ip2int(client_list[0].ip) , int(random_source_port.replace(' ',''),16), 6)
		if connection != None:
			created_connections +=1
		else:
			server_is_unavailable_error +=1
		        
		
		error_stats = ezbox.get_error_stats()     
		print "created_connections %d"%created_connections
		print "uncreated_connections %d"%server_is_unavailable_error
		
		expected_packets_receieved = 0
		expected_num_of_errors += 1
		if (packets_received_1 == expected_packets_receieved and 
		    sched_connections_on_server == expected_sched_connections_on_server and 
		    error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE'] == expected_num_of_errors and
		    (created_connections == expected_sched_connections_on_server) and
		   (num_of_packets == server_is_unavailable_error)):
		
		#     server_list[0].compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
		    print "PART3 - try create new connection when number of connections = u_thresh Passed"
		else:
		    print "PART3 - try create new connection when number of connections = u_thresh Failed"
		    exit(1)
	
		# create packet
		
		num_of_packets = 2
		print "PART 3 - B - send %d frames for remove connections u_thresh = %d l_thresh = %d current_connection = %d"%(num_of_packets, u_thresh, l_thresh, sched_connections_on_server)
		print "Number of connections should be between u_thresh %d and l_thresh %d"%(u_thresh, l_thresh)
		print "Try  to create new connection - should FAIL"
		
		
		source_port="%04x"%src_port_list[0]
		source_port = source_port[0:2] + ' ' + source_port[2:4]
		reset_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
		                         mac_sa=client_list[0].mac_address.replace(':',' '),
	                             ip_dst=ip_to_hex_display(vip_list[0]),
	                             ip_src=ip_to_hex_display(client_list[0].ip),
		                         tcp_source_port = source_port,
		                         tcp_dst_port = '00 50', # port 80
		                         tcp_reset_flag = True,
		                         packet_length=packet_size)
		reset_packet.generate_packet()
		
		source_port="%04x"%src_port_list[1]
		source_port = source_port[0:2] + ' ' + source_port[2:4]
		fin_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
		                         mac_sa=client_list[0].mac_address.replace(':',' '),
	                             ip_dst=ip_to_hex_display(vip_list[0]),
	                             ip_src=ip_to_hex_display(client_list[0].ip),
		                         tcp_source_port = source_port,
		                         tcp_dst_port = '00 50', # port 80
		                         tcp_fin_flag = True,
		                         packet_length=packet_size)
		fin_packet.generate_packet()   
		    
		pcap_to_send = create_pcap_file(packets_list=[reset_packet.packet,fin_packet.packet], output_pcap_file_name=ALVSdir + 'verification/testing/dp/temp_packet.pcap')
		  
		time.sleep(2) 
		server_list[0].capture_packets_from_service(vip_list[0])
		    
		    # send packet
		time.sleep(5)
		print "Send packets"
		client_list[0].send_packet_to_nps(pcap_to_send)
		  
		time.sleep(60)
		print "capture from server"
		packets_received_1 = server_list[0].stop_capture()
		
		time.sleep(20)
		
		print "Server 1  - received %d packets"%packets_received_1
		
		sched_connections_on_server = ezbox.get_server_connections_total_stats(0)
		print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server
		
		error_stats = ezbox.get_error_stats()     
		print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']
		
		
		expected_packets_receieved = 0
		
		if (u_thresh != 0):
		    expected_packets_receieved = min(u_thresh, num_of_packets)
		else:
		    expected_packets_receieved = num_of_packets
		    
		expected_sched_connections_on_server -= num_of_packets
		
		created_connections = 0
		server_is_unavailable_error = 0
		
		for src_port in src_port_list:   
		    connection=ezbox.get_connection(ip2int(vip_list[0]), 80, ip2int(client_list[0].ip) , src_port, 6)
		    if connection != None:
		        created_connections +=1
		    else:
		        server_is_unavailable_error +=1
		error_stats = ezbox.get_error_stats()     
		print "created_connections %d"%created_connections
		print "uncreated_connections %d"%server_is_unavailable_error
		
		if (packets_received_1 == expected_packets_receieved and 
		    sched_connections_on_server == expected_sched_connections_on_server and 
		    error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE'] == expected_num_of_errors and
		    (created_connections == expected_sched_connections_on_server) and
		   ( server_is_unavailable_error == num_of_packets)):
		
		#     server_list[0].compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
		    print "PART3 - B - Passed"
		else:
		    print "PART3 - B-  Failed"
		    exit(1)
	
		num_of_packets_c = 1
		print "PART 3 - C - send %d frames for create connection "%(num_of_packets_c)
		print "Connection shouldn't be created"
		
		packet_size = 150
		random_source_port = '%02x %02x'%(random.randint(0,255),random.randint(0,255))
		
		# create packet
		data_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
		                         mac_sa=client_list[0].mac_address.replace(':',' '),
	                             ip_dst=ip_to_hex_display(vip_list[0]),
	                             ip_src=ip_to_hex_display(client_list[0].ip),
		                         tcp_source_port = random_source_port,
		                         tcp_dst_port = '00 50', # port 80
		                         packet_length=packet_size)
		data_packet.generate_packet()
		
		time.sleep(2) 
		server_list[0].capture_packets_from_service(vip_list[0])
		
		pcap_to_send_part3 = create_pcap_file(packets_list=[data_packet.packet], output_pcap_file_name=ALVSdir + 'verification/testing/dp/temp_packet.pcap')
		    
		    # send packet
		time.sleep(5)
		print "Send packets"
		client_list[0].send_packet_to_nps(pcap_to_send_part3)
		  
		time.sleep(60)
		print "capture from server"
		packets_received_1 = server_list[0].stop_capture()
		
		print "Server 1  - received %d packets"%packets_received_1
		sched_connections_on_server = ezbox.get_server_connections_total_stats(0)
		print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server
		
		error_stats = ezbox.get_error_stats()     
		print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']
		
		
		created_connections = 0
		server_is_unavailable_error = 0
		for src_port in src_port_list:   
		    connection=ezbox.get_connection(ip2int(vip_list[0]), 80, ip2int(client_list[0].ip) , src_port, 6)
		    if connection != None:
		        created_connections +=1
		    else:
		        server_is_unavailable_error +=1
		connection=ezbox.get_connection(ip2int(vip_list[0]), 80, ip2int(client_list[0].ip) , int(random_source_port.replace(' ',''),16), 6)
		if connection != None:
		    created_connections +=1
		else:
		    server_is_unavailable_error +=1
		
		print "created_connections %d"%created_connections
		print "uncreated_connections %d"%server_is_unavailable_error
		num_of_packets_c += num_of_packets
		expected_packets_receieved = 0
		expected_num_of_errors += 1
		if (packets_received_1 == expected_packets_receieved and 
		    sched_connections_on_server == expected_sched_connections_on_server and 
		    error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE'] == expected_num_of_errors and
		    (created_connections == expected_sched_connections_on_server) and
		   (num_of_packets_c == server_is_unavailable_error)):
		
		#     server_list[0].compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
		    print "PART3 C Passed"
		else:
		    print "PART3 C Failed"
		    exit(1)
	
		num_of_packets_d = 3
		print "PART 3 - D - send %d frames for deletion of  connections "%(num_of_packets_d)
		print "Number pf connections should be reduced below l_thresh"
		
		source_port="%04x"%src_port_list[2]
		source_port = source_port[0:2] + ' ' + source_port[2:4]
		reset_packet_d = tcp_packet(mac_da=ezbox.setup['mac_address'],
		                         mac_sa=client_list[0].mac_address.replace(':',' '),
	                             ip_dst=ip_to_hex_display(vip_list[0]),
	                             ip_src=ip_to_hex_display(client_list[0].ip),
		                         tcp_source_port = source_port,
		                         tcp_dst_port = '00 50', # port 80
		                         tcp_reset_flag = True,
		                         packet_length=packet_size)
		reset_packet_d.generate_packet()
		
		source_port="%04x"%src_port_list[3]
		source_port = source_port[0:2] + ' ' + source_port[2:4]
		fin_packet_d_1 = tcp_packet(mac_da=ezbox.setup['mac_address'],
		                         mac_sa=client_list[0].mac_address.replace(':',' '),
	                             ip_dst=ip_to_hex_display(vip_list[0]),
	                             ip_src=ip_to_hex_display(client_list[0].ip),
		                         tcp_source_port = source_port,
		                         tcp_dst_port = '00 50', # port 80
		                         tcp_fin_flag = True,
		                         packet_length=packet_size)
		fin_packet_d_1.generate_packet()   
		
		source_port="%04x"%src_port_list[4]
		source_port = source_port[0:2] + ' ' + source_port[2:4]
		fin_packet_d_2 = tcp_packet(mac_da=ezbox.setup['mac_address'],
		                         mac_sa=client_list[0].mac_address.replace(':',' '),
	                             ip_dst=ip_to_hex_display(vip_list[0]),
	                             ip_src=ip_to_hex_display(client_list[0].ip),
		                         tcp_source_port = source_port,
		                         tcp_dst_port = '00 50', # port 80
		                         tcp_fin_flag = True,
		                         packet_length=packet_size)
		fin_packet_d_2.generate_packet()   
		    
		pcap_to_send = create_pcap_file(packets_list=[reset_packet_d.packet,fin_packet_d_1.packet,fin_packet_d_2.packet], output_pcap_file_name=ALVSdir + 'verification/testing/dp/temp_packet.pcap')
		  
		time.sleep(2) 
		server_list[0].capture_packets_from_service(vip_list[0])
		    
		    # send packet
		time.sleep(5)
		print "Send packets"
		client_list[0].send_packet_to_nps(pcap_to_send)
		  
		time.sleep(60)
		print "capture from server"
		packets_received_1 = server_list[0].stop_capture()
		
		time.sleep(20)
		
		print "Server 1  - received %d packets"%packets_received_1
		sched_connections_on_server = ezbox.get_server_connections_total_stats(0)
		print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server
		
		error_stats = ezbox.get_error_stats()     
		print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']
		
		created_connections = 0
		server_is_unavailable_error = 0
		for src_port in src_port_list:   
		    connection=ezbox.get_connection(ip2int(vip_list[0]), 80, ip2int(client_list[0].ip) , src_port, 6)
		    if connection != None:
		        created_connections +=1
		    else:
		        server_is_unavailable_error +=1
		
		print "created_connections %d"%created_connections
		print "uncreated_connections %d"%server_is_unavailable_error
		expected_server_is_unavailable_errors = num_of_packets_d + num_of_packets
		expected_packets_receieved = num_of_packets_d
		expected_sched_connections_on_server -= num_of_packets_d
		if (packets_received_1 == expected_packets_receieved and 
		    sched_connections_on_server == expected_sched_connections_on_server and 
		    error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE'] == expected_num_of_errors and
		    (created_connections == expected_sched_connections_on_server) and
		   (expected_server_is_unavailable_errors == server_is_unavailable_error)):
		
		#     server_list[0].compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
		    print "PART3 D Passed"
		else:
		    print "PART3 D Failed"
		    exit(1)
	
		num_of_packets_e = len(packet_list_to_send)
		print "PART 3 - E - send %d frames to create new connections "%(num_of_packets_e)
		print "New connections should be created up to  u_thresh = %d "%u_thresh
		
		pcap_to_send = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name=ALVSdir + 'verification/testing/dp/temp_packet.pcap')
		
		time.sleep(2) 
		server_list[0].capture_packets_from_service(vip_list[0])
		   
		    # send packet
		time.sleep(5)
		print "Send packets"
		client_list[0].send_packet_to_nps(pcap_to_send)
		  
		time.sleep(60)
		print "capture from server"
		packets_received_1 = server_list[0].stop_capture()
		print "Server 1  - received %d packets"%packets_received_1
		
		time.sleep(20)
		sched_connections_on_server = ezbox.get_server_connections_total_stats(0)
		print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server
		error_stats = ezbox.get_error_stats()     
		print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']
		
		created_connections = 0
		server_is_unavailable_error = 0
		for src_port in src_port_list:   
		    connection=ezbox.get_connection(ip2int(vip_list[0]), 80, ip2int(client_list[0].ip) , src_port, 6)
		    if connection != None:
		        created_connections +=1
		    else:
		        server_is_unavailable_error +=1
		
		expected_packets_receieved = u_thresh
		expected_sched_connections_on_server = u_thresh
		expected_num_of_errors +=  num_of_packets_e - u_thresh
		expected_server_is_unavailable_errors = num_of_packets_e - u_thresh
		
		if (packets_received_1 == expected_packets_receieved and 
		    sched_connections_on_server == expected_sched_connections_on_server and 
		    error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE'] == expected_num_of_errors and
		    (created_connections == expected_sched_connections_on_server) and
		   (server_is_unavailable_error == expected_server_is_unavailable_errors)):
		
		#     server_list[0].compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
		    print "PART3 E Passed"
		else:
		    print "PART3 E Failed"
		    exit(1)
		
	
		u_thresh = 7
		l_thresh = 3
		print "**************************************************************************"
		print "PART 4 - modify server with u_thresh = %d higher than it was and check that new connections are created"%u_thresh
		
		num_of_packets_4 = len(packet_list_to_send)
		
		ezbox.modify_server(server_list[0].vip, port, server_list[0].ip, port, weight=1, u_thresh=u_thresh, l_thresh=l_thresh)
		
		pcap_to_send = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name=ALVSdir + 'verification/testing/dp/temp_packet.pcap')
		
		time.sleep(2) 
		server_list[0].capture_packets_from_service(vip_list[0])
		   
		    # send packet
		time.sleep(5)
		print "Send packets"
		client_list[0].send_packet_to_nps(pcap_to_send)
		  
		time.sleep(60)
		print "capture from server"
		packets_received_1 = server_list[0].stop_capture()
		 
		print "Server 1  - received %d packets"%packets_received_1
		
		time.sleep(20)
		sched_connections_on_server = ezbox.get_server_connections_total_stats(0)
		print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server
		error_stats = ezbox.get_error_stats()     
		print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']
		
		created_connections = 0
		server_is_unavailable_error = 0
		for src_port in src_port_list:   
		    connection=ezbox.get_connection(ip2int(vip_list[0]), 80, ip2int(client_list[0].ip) , src_port, 6)
		    if connection != None:
		        created_connections +=1
		    else:
		        server_is_unavailable_error +=1
		
		expected_packets_receieved = u_thresh
		expected_sched_connections_on_server = u_thresh
		expected_num_of_errors +=  num_of_packets_4 - u_thresh
		expected_server_is_unavailable_errors = num_of_packets_4 - u_thresh
		
		if (packets_received_1 == expected_packets_receieved and 
		    sched_connections_on_server == expected_sched_connections_on_server and 
		    error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE'] == expected_num_of_errors and
		    (created_connections == expected_sched_connections_on_server) and
		   (server_is_unavailable_error == expected_server_is_unavailable_errors)):
		
		#     server_list[0].compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
		    print "PART4 Passed"
		else:
		    print "PART4 Failed"
		    exit(1)
	
		u_thresh = 7
		l_thresh = 0
		num_of_packets_5_a = 1
		print "**************************************************************************"
		print "PART 5 - modify server l_thresh = 0 u_thresh as it was previousely and check 3/4 of u_thresh = %s"%u_thresh
		
		print "PART 5 A - remove connection to be at sched_conns*3 > u_thresh*4"
		ezbox.modify_server(server_list[0].vip, port, server_list[0].ip, port, weight=1, u_thresh=u_thresh, l_thresh=l_thresh)
		
		pcap_to_send = create_pcap_file(packets_list=[fin_packet_d_2.packet], output_pcap_file_name=ALVSdir + 'verification/testing/dp/temp_packet.pcap')
		
		time.sleep(2) 
		server_list[0].capture_packets_from_service(vip_list[0])
		   
		    # send packet
		time.sleep(5)
		print "Send packets"
		client_list[0].send_packet_to_nps(pcap_to_send)
		  
		time.sleep(60)
		print "capture from server"
		packets_received_1 = server_list[0].stop_capture()
		 
		print "Server 1  - received %d packets"%packets_received_1
		time.sleep(20)
		
		sched_connections_on_server = ezbox.get_server_connections_total_stats(0)
		print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server
		error_stats = ezbox.get_error_stats()     
		print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']
		
		created_connections = 0
		server_is_unavailable_error = 0
		for src_port in src_port_list:   
		    connection=ezbox.get_connection(ip2int(vip_list[0]), 80, ip2int(client_list[0].ip) , src_port, 6)
		    if connection != None:
		        created_connections +=1
		    else:
		        server_is_unavailable_error +=1
		        
		expected_packets_receieved = num_of_packets_5_a
		expected_sched_connections_on_server -= num_of_packets_5_a
		expected_server_is_unavailable_errors = num_of_packets_5_a
		
		if (packets_received_1 == expected_packets_receieved and 
		    sched_connections_on_server == expected_sched_connections_on_server and 
		    error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE'] == expected_num_of_errors and
		    (created_connections == expected_sched_connections_on_server) and
		   (server_is_unavailable_error == expected_server_is_unavailable_errors)):
		
		#     server_list[0].compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
		    print "PART5 A Passed"
		else:
		    print "PART5 A Failed"
		    exit(1)
	
		print "PART 5 B - try to create new connections - should fail - as OVERLOADED flag is still set"
		num_of_packets_5_b = 1
		pcap_to_send = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name=ALVSdir + 'verification/testing/dp/temp_packet.pcap')
		
		time.sleep(2) 
		server_list[0].capture_packets_from_service(vip_list[0])
		   
		    # send packet
		time.sleep(5)
		print "Send packets"
		client_list[0].send_packet_to_nps(pcap_to_send)
		  
		time.sleep(60)
		print "capture from server"
		packets_received_1 = server_list[0].stop_capture()
		 
		print "Server 1  - received %d packets"%packets_received_1
		
		created_connections = 0
		server_is_unavailable_error = 0
		for src_port in src_port_list:   
		    connection=ezbox.get_connection(ip2int(vip_list[0]), 80, ip2int(client_list[0].ip) , src_port, 6)
		    if connection != None:
		        created_connections +=1
		    else:
		        server_is_unavailable_error +=1
		
		time.sleep(20)
		sched_connections_on_server = ezbox.get_server_connections_total_stats(0)
		print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server
		error_stats = ezbox.get_error_stats()     
		print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']
		
		expected_packets_receieved = len(packet_list_to_send) - num_of_packets_5_b
		expected_sched_connections_on_server = len(packet_list_to_send) - num_of_packets_5_b
		expected_num_of_errors += num_of_packets_5_b
		expected_server_is_unavailable_errors = num_of_packets_5_b
		
		if (packets_received_1 == expected_packets_receieved and 
		    sched_connections_on_server == expected_sched_connections_on_server and 
		    error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE'] == expected_num_of_errors and
		    (created_connections == expected_sched_connections_on_server) and
		   (server_is_unavailable_error == expected_server_is_unavailable_errors)):
		
		#     server_list[0].compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
		    print "PART5 B Passed"
		else:
		    print "PART5 B Failed"
		    exit(1)
		
	
		print "PART 5 C - remove connection to be below sched_conns*3 > u_thresh*4"
		num_of_packets_5_c = 1
		
		pcap_to_send = create_pcap_file(packets_list=[fin_packet_d_1.packet], output_pcap_file_name=ALVSdir + 'verification/testing/dp/temp_packet.pcap')
		
		time.sleep(2) 
		server_list[0].capture_packets_from_service(vip_list[0])
		   
		    # send packet
		time.sleep(5)
		print "Send packets"
		client_list[0].send_packet_to_nps(pcap_to_send)
		  
		time.sleep(60)
		print "capture from server"
		packets_received_1 = server_list[0].stop_capture()
		 
		print "Server 1  - received %d packets"%packets_received_1
		
		sched_connections_on_server = ezbox.get_server_connections_total_stats(0)
		print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server
		error_stats = ezbox.get_error_stats()     
		print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']
		
		created_connections = 0
		server_is_unavailable_error = 0
		for src_port in src_port_list:   
		    connection=ezbox.get_connection(ip2int(vip_list[0]), 80, ip2int(client_list[0].ip) , src_port, 6)
		    if connection != None:
		        created_connections +=1
		    else:
		        server_is_unavailable_error +=1
		        
		expected_packets_receieved = num_of_packets_5_c
		expected_sched_connections_on_server -= num_of_packets_5_c
		expected_server_is_unavailable_errors = num_of_packets_5_a + num_of_packets_5_c
		
		if (packets_received_1 == expected_packets_receieved and 
		    sched_connections_on_server == expected_sched_connections_on_server and 
		    error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE'] == expected_num_of_errors and
		    (created_connections == expected_sched_connections_on_server) and
		   (server_is_unavailable_error == expected_server_is_unavailable_errors)):
		
		#     server_list[0].compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
		    print "PART5 C Passed"
		else:
		    print "PART5 C Failed"
		    exit(1)
	
		print "PART 5 D - try to create new connections - should PASS - as OVERLOADED flag is off"
		pcap_to_send = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name=ALVSdir + 'verification/testing/dp/temp_packet.pcap')
		
		time.sleep(2) 
		server_list[0].capture_packets_from_service(vip_list[0])
		   
		    # send packet
		time.sleep(5)
		print "Send packets"
		client_list[0].send_packet_to_nps(pcap_to_send)
		  
		time.sleep(5)
		print "capture from server"
		packets_received_1 = server_list[0].stop_capture()
		 
		print "Server 1  - received %d packets"%packets_received_1
		
		created_connections = 0
		server_is_unavailable_error = 0
		for src_port in src_port_list:   
		    connection=ezbox.get_connection(ip2int(vip_list[0]), 80, ip2int(client_list[0].ip) , src_port, 6)
		    if connection != None:
		        created_connections +=1
		    else:
		        server_is_unavailable_error +=1
		
		time.sleep(20)
		sched_connections_on_server = ezbox.get_server_connections_total_stats(0)
		print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server
		error_stats = ezbox.get_error_stats()     
		print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']
		
		expected_packets_receieved = min(u_thresh, len(packet_list_to_send))
		expected_sched_connections_on_server = min(u_thresh, len(packet_list_to_send))
		expected_num_of_errors += len(packet_list_to_send) - u_thresh
		expected_sched_connections_on_server = min(u_thresh, len(packet_list_to_send))
		expected_server_is_unavailable_errors =  len(packet_list_to_send) - expected_packets_receieved
		
		if (packets_received_1 == expected_packets_receieved and 
		    sched_connections_on_server == expected_sched_connections_on_server and 
		    error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE'] == expected_num_of_errors and
		    (created_connections == expected_sched_connections_on_server) and
		   (server_is_unavailable_error == expected_server_is_unavailable_errors)):
		
		#     server_list[0].compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
		    print "PART5 D Passed"
		else:
		    print "PART5 D Failed"
		    exit(1)
	
		print "**************************************************************************"
		print "PART 6 - wait till all connections are aged - due to timeout"
		time.sleep(1000)
		server_list[0].capture_packets_from_service(vip_list[0])
		     
		time.sleep(5)
		print "capture from server"
		packets_received_1 = server_list[0].stop_capture()
		 
		print "Server 1  - received %d packets"%packets_received_1
		created_connections = 0
		server_is_unavailable_error = 0
		for src_port in src_port_list:   
		    connection=ezbox.get_connection(ip2int(vip_list[0]), 80, ip2int(client_list[0].ip) , src_port, 6)
		    if connection != None:
		        created_connections +=1
		    else:
		        server_is_unavailable_error +=1
		        
		time.sleep(20)
		sched_connections_on_server = ezbox.get_server_connections_total_stats(0)
		print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server
		error_stats = ezbox.get_error_stats()     
		print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']
		
		expected_packets_receieved = 0
		expected_sched_connections_on_server = 0
		expected_server_is_unavailable_errors =  len(packet_list_to_send)
		
		if (packets_received_1 == expected_packets_receieved and 
		    sched_connections_on_server == expected_sched_connections_on_server and 
		    error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE'] == expected_num_of_errors and
		    (created_connections == expected_sched_connections_on_server) and
		   (server_is_unavailable_error == expected_server_is_unavailable_errors)):
		    print "PART5 D Passed"
		else:
		    print "PART5 D Failed"
		    exit(1)
		    
current_test = One_Service_1Server_Overloaded_Flag_Fallback()
current_test.main()
