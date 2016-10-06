#!/usr/bin/env python

import sys
sys.path.append("verification/testing")
from test_infra import * 
import random

ezbox,args = init_test(test_arguments=sys.argv)
scenarios_to_run = args['scenarios']

# each setup can use differen VMs
ip_list = get_setup_list(args['setup_num'])

# each setup can use different the virtual ip 
virtual_ip_address_1 = get_setup_vip(args['setup_num'], 0)
virtual_ip_address_2 = get_setup_vip(args['setup_num'], 1)
virtual_ip_address_3 = get_setup_vip(args['setup_num'], 2)

# create servers
server1 = real_server(management_ip=ip_list[0]['hostname'], data_ip=ip_list[0]['ip'])
server2 = real_server(management_ip=ip_list[1]['hostname'], data_ip=ip_list[1]['ip'])
server3 = real_server(management_ip=ip_list[2]['hostname'], data_ip=ip_list[2]['ip'])
server4 = real_server(management_ip=ip_list[4]['hostname'], data_ip=ip_list[4]['ip'])

# create services on ezbox
first_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_1, port='80', schedule_algorithm = 'source_hash')
first_service.add_server(server1, weight='1')
first_service.add_server(server2, weight='1')
first_service.add_server(server3, weight='1')

second_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_2, port='80', schedule_algorithm = 'source_hash_with_source_port')
second_service.add_server(server1, weight='1')
second_service.add_server(server2, weight='1')
second_service.add_server(server3, weight='1')
 
third_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_3, port='80', schedule_algorithm = 'source_hash')
third_service.add_server(server1, weight='1')

# create client
client_object = client(management_ip=ip_list[3]['hostname'], data_ip=ip_list[3]['ip'])
	 

if 1 in scenarios_to_run:
	print "\nChecking Scenario 1"
	
	# create packets
	packet_sizes = [60,127,129,511,513,1023,1025,1500]
	packet_list_to_send = []
	
	print "Creating Packets"
	for i in range(50):
		# set the packet size
		if i < len(packet_sizes):
			packet_size = packet_sizes[i]
		else:
			packet_size = random.randint(60,1500)
		
		random_source_port = '%02x %02x'%(random.randint(0,255),random.randint(0,255))

		# create packet
		data_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
								 mac_sa=client_object.mac_address,
								 ip_dst=first_service.virtual_ip_hex_display,
								 ip_src='c0 a8 00 69',
								 tcp_source_port = random_source_port,
								 tcp_dst_port = '00 50', # port 80
								 packet_length=packet_size)
		data_packet.generate_packet()
		
		packet_list_to_send.append(data_packet.packet)
		
	pcap_to_send = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name='verification/testing/dp/pcap_files/temp_packet.pcap')
	
	# try to capture max 10 times (capture is not stable on all VMs)
	for i in range(10):
		time.sleep(5) 
		server1.capture_packets_from_service(service=first_service)
		server2.capture_packets_from_service(service=first_service)
		server3.capture_packets_from_service(service=first_service)
		
		# send packet
		time.sleep(5)
		print "Send packets"
		client_object.send_packet_to_nps(pcap_to_send)
		
		time.sleep(15)
		
		packets_received_1 = server1.stop_capture()
		packets_received_2 = server2.stop_capture()
		packets_received_3 = server3.stop_capture()
			
		if packets_received_1 + packets_received_2 + packets_received_3 == 50:
			print "Server 1 received %d packets"%packets_received_1
			print "Server 2 received %d packets"%packets_received_2
			print "Server 3 received %d packets"%packets_received_3
		
			if packets_received_1 == 50 and packets_received_2 == 0 and packets_received_3 == 0:
				print "Scenario Passed"
				break;
			else:
				ezbox.print_all_stats()
				print "Fail, received packets not as expected\n"
				exit(1)
	
	if i >= 9:
		print "error while capture packets"	
		
	# compare the packets that were received 
#	 server1.compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
		
###########################################################################################################################
# this scenario check the scheduling algorithm of source hash, ip source is changing, (service is on source port disable) #
###########################################################################################################################		
if 2 in scenarios_to_run:
	print "\nChecking Scenario 2"
	
	# create packets
	ip_source_address = '192.168.0.100'
	packet_sizes = [60,127,129,511,513,1023,1025,1500]
	packet_list_to_send = []
	
	print "Creating Packets"
	for i in range(50):
		# set the packet size
		if i < len(packet_sizes):
			packet_size = packet_sizes[i]
		else:
			packet_size = random.randint(60,1500)
		
		ip_source_address = add2ip(ip_source_address,1)
		
		# create packet
		data_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
								 mac_sa=client_object.mac_address,
								 ip_dst=first_service.virtual_ip_hex_display,
								 ip_src=ip_source_address,
								 tcp_source_port = '00 00',
								 tcp_dst_port = '00 50', # port 80
								 packet_length=packet_size)
		data_packet.generate_packet()
		
		packet_list_to_send.append(data_packet.packet)
		
	pcap_to_send = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name='verification/testing/dp/pcap_files/temp_packet.pcap')

	# try to capture max 10 times (capture is not stable on all VMs)
	for i in range(10):
		server1.capture_packets_from_service(service=first_service)
		server2.capture_packets_from_service(service=first_service)
		server3.capture_packets_from_service(service=first_service)
		
		time.sleep(5)
		# send packet
		client_object.send_packet_to_nps(pcap_to_send)
	
		time.sleep(15)
		
		packets_received_1 = server1.stop_capture()
		packets_received_2 = server2.stop_capture()
		packets_received_3 = server3.stop_capture()
		
		if packets_received_1 + packets_received_2 + packets_received_3 == 50:
			print "Server 1 received %d packets"%packets_received_1
			print "Server 2 received %d packets"%packets_received_2
			print "Server 3 received %d packets"%packets_received_3
			
			if packets_received_1 == 14 and packets_received_2 == 18 and packets_received_3 == 18:
				print "Scenario Passed"
				break
			else:
				ezbox.print_all_stats()
				print "Fail, received packets not as expected\n"
				exit(1)
		
	if i >= 9:
		print "error while capture packets"		

###################################################################################################################
########					   this scenario check the scheduling algorithm of source hash,				 ##########
########				  ip source and source port is changing, (service is on source port enable)		 ##########
###################################################################################################################		

if 3 in scenarios_to_run:
	print "\nChecking Scenario 3"

	# create packets
	ip_source_address = get_setup_vip(args['setup_num'], 4)
	packet_sizes = [60,127,129,511,513,1023,1025,1500]
	packet_list_to_send = []
	
	print "Creating Packets"

	for i in range(500):

		# set the packet size
		if i < len(packet_sizes):
			packet_size = packet_sizes[i]
		else:
			packet_size = random.randint(60,1500)
		
		tcp_source_port = "%2s %2s"%(str(i+1000)[0:2],(str(i+1000)[2:4]))
		
		# create packet
		data_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
								 mac_sa=client_object.mac_address,
								 ip_dst=second_service.virtual_ip_hex_display,
								 ip_src='192.168.0.100',
								 tcp_source_port = tcp_source_port,
								 tcp_dst_port = '00 50', # port 80
								 packet_length=packet_size) 
		data_packet.generate_packet()
		
		packet_list_to_send.append(data_packet.packet)
		
	pcap_to_send = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name='verification/testing/dp/pcap_files/temp_packet.pcap')

	# try to capture max 10 times (capture is not stable on all VMs)
	for i in range(10):
		server1.capture_packets_from_service(service=second_service)
		server2.capture_packets_from_service(service=second_service)
		server3.capture_packets_from_service(service=second_service)
		time.sleep(5)
		
		# send packet
		client_object.send_packet_to_nps(pcap_to_send)
	
		time.sleep(15)
		
		packets_received_1 = server1.stop_capture()
		packets_received_2 = server2.stop_capture()
		packets_received_3 = server3.stop_capture()
		
		if packets_received_1 + packets_received_2 + packets_received_3 == 500:
			print "Server 1 received %d packets"%packets_received_1
			print "Server 2 received %d packets"%packets_received_2
			print "Server 3 received %d packets"%packets_received_3
			
			if packets_received_1 == 171 and packets_received_2 == 163 and packets_received_3 == 166:
				print "Scenario Passed"
				break
			else:
				ezbox.print_all_stats()
				print "Fail, received packets not as expected\n"
				exit(1)
	
	if i >= 9:
		print "error while capture packets"		
			
if 4 in scenarios_to_run:
############################################################################
# this scenario checks behavior when the arp table lookup is failing		
############################################################################		 
	print "\nChecking Scenario 4"
	
	print "Delete the server arp entry"
	ezbox.execute_command_on_host("arp -d %s"%server1.data_ip)
	
	
	# create packet
	data_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
							 mac_sa=client_object.mac_address,
							 ip_dst=third_service.virtual_ip_hex_display,
							 ip_src=client_object.hex_display_to_ip,
							 tcp_source_port = '00 00',
							 tcp_dst_port = '00 50', # port 80
							 packet_length=128)
	data_packet.generate_packet()
	
	# capture packets on host
# 	ezbox.capture_packets('dst ' + third_service.virtual_ip)
 	
	# send packet
	client_object.send_packet_to_nps(data_packet.pcap_file_name)
	
	time.sleep(15)
	# todo - check statistics of unavailable arp entry 
	
	print "Verify that the packet was discarded"

	arp_fail_discard_stat = 0
	for i in range(4):
		arp_fail_discard_stat += ezbox.get_interface_stats(i)['NW_IF_STATS_FAIL_ARP_LOOKUP']
		
	if arp_fail_discard_stat != 1:
		ezbox.print_all_stats()
		print "ERROR, packet wasnt forward to host\n"
		exit(1)
	

	print "Scenario Passed"
	

# checkers
	# create packet
#	 data_packet_to_compare = tcp_packet(mac_da=server1.mac_address.replace(':',' '),
#							  mac_sa=client_object.mac_address,
#							  ip_dst=third_service.virtual_ip_hex_display,
#							  ip_src=client_object.hex_display_to_ip,
#							  tcp_source_port = '00 00',
#							  tcp_dst_port = '00 50', # port 80
#							  packet_length=packet_size)
#	 data_packet_to_compare.generate_packet()
#	 
#	 server1.compare_received_packets_to_pcap_file(pcap_file=data_packet_to_compare.pcap_file_name)


# tear down
# server1.close()
# server2.close()
# client.close()
# ezbox.close()

print
print "Test Passed"
