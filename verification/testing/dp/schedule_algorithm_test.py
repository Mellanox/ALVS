#!/usr/bin/env python

import sys
sys.path.append("verification/testing")
from test_infra import * 
import random

args = read_test_arg(sys.argv)	

log_file = "scheduling_algorithm_test.log"
if 'log_file' in args:
	log_file = args['log_file']
init_logging(log_file)

# scenarios_to_run = args['scenarios']
scenarios_to_run = [1,2,3,4]

ezbox = ezbox_host(args['setup_num'])

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
								 ip_src=client_object.hex_display_to_ip,
								 tcp_source_port = random_source_port,
								 tcp_dst_port = '00 50', # port 80
								 packet_length=packet_size)
		data_packet.generate_packet()
		
		packet_list_to_send.append(data_packet.packet)
		
	pcap_to_send = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name='verification/testing/dp/pcap_files/temp_packet.pcap')
	
	time.sleep(2) 
	server1.capture_packets_from_service(service=first_service)
	server2.capture_packets_from_service(service=first_service)
	server3.capture_packets_from_service(service=first_service)

	# send packet
	time.sleep(5)
	print "Send packets"
	client_object.send_packet_to_nps(pcap_to_send)

	time.sleep(5)
	
	packets_received_1 = server1.stop_capture()
	packets_received_2 = server2.stop_capture()
	packets_received_3 = server3.stop_capture()
		
	print "Server 1 received %d packets"%packets_received_1
	print "Server 2 received %d packets"%packets_received_2
	print "Server 3 received %d packets"%packets_received_3
	
	if packets_received_1 == 0 and packets_received_2 == 50 and packets_received_3 == 0:

		print "Scenario Passed"

	else:
		ezbox.print_all_stats()
		print "Fail, received packets not as expected\n"
		exit(1)
		
	# compare the packets that were received 
#	 server1.compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
		
###########################################################################################################################
# this scenario check the scheduling algorithm of source hash, ip source is changing, (service is on source port disable) #
###########################################################################################################################		
if 2 in scenarios_to_run:
	print "\nChecking Scenario 2"
	
	# create packets
	ip_source_address = get_setup_vip(args['setup_num'], 3)
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

	server1.capture_packets_from_service(service=first_service)
	server2.capture_packets_from_service(service=first_service)
	server3.capture_packets_from_service(service=first_service)

	# send packet
	client_object.send_packet_to_nps(pcap_to_send)

	time.sleep(10)
	
	packets_received_1 = server1.stop_capture()
	packets_received_2 = server2.stop_capture()
	packets_received_3 = server3.stop_capture()
	
	print "Server 1 received %d packets"%packets_received_1
	print "Server 2 received %d packets"%packets_received_2
	print "Server 3 received %d packets"%packets_received_3
	

	if packets_received_1 == 10 and packets_received_2 == 22 and packets_received_3 == 18:
	#	 server1.compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
		print "Scenario Passed"

	else:
		ezbox.print_all_stats()
		print "Fail, received packets not as expected\n"
		exit(1)
		
		

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
		
		tcp_source_port = "%02x %02x"%(i,i)
		
		# create packet
		data_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
								 mac_sa=client_object.mac_address,
								 ip_dst=second_service.virtual_ip_hex_display,
								 ip_src=client_object.hex_display_to_ip,
								 tcp_source_port = tcp_source_port,
								 tcp_dst_port = '00 50', # port 80
								 packet_length=packet_size) 
		data_packet.generate_packet()
		
		packet_list_to_send.append(data_packet.packet)
		
	pcap_to_send = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name='verification/testing/dp/pcap_files/temp_packet.pcap')

	server1.capture_packets_from_service(service=second_service)
	server2.capture_packets_from_service(service=second_service)
	server3.capture_packets_from_service(service=second_service)
	time.sleep(1)
	
	# send packet
	client_object.send_packet_to_nps(pcap_to_send)

	time.sleep(2)
	
	packets_received_1 = server1.stop_capture()
	packets_received_2 = server2.stop_capture()
	packets_received_3 = server3.stop_capture()
	
	print "Server 1 received %d packets"%packets_received_1
	print "Server 2 received %d packets"%packets_received_2
	print "Server 3 received %d packets"%packets_received_3
	
	if packets_received_1 == 17 and packets_received_2 == 17 and packets_received_3 == 16:
	#	 server1.compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')

		print "Scenario Passed"

	else:
		ezbox.print_all_stats()
		print "Fail, received packets not as expected\n"
		exit(1)	   
		
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
	ezbox.capture_packets('dst ' + third_service.virtual_ip)
	
	# send packet
	client_object.send_packet_to_nps(data_packet.pcap_file_name)
	
	time.sleep(2)
	# todo - check statistics of unavailable arp entry 
	
	print "Verify that the packet was send to host"
	packets_received_on_host = ezbox.stop_capture()
	if packets_received_on_host != 1:
		ezbox.print_all_stats()
		print "ERROR, packet wasnt forward to host\n"
		exit(1)
	

	print "Scenario Passed"
	

ezbox.print_all_stats()
	
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
