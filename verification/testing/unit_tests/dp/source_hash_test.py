#!/usr/bin/env python

import sys
sys.path.append("verification/testing")
sys.path.append("verification/testing/unit_tests")
import random
from common_infra import *
from e2e_infra import *
from unit_tester import Unit_Tester

server_count   = 7
client_count   = 1
service_count  = 3

class Source_Hash_Test(Unit_Tester):
	
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = generic_init(setup_num, service_count, server_count, client_count)
		
		w = 1
		for i,s in enumerate(self.test_resources['server_list']):
			if i < 3:
				s.vip = self.test_resources['vip_list'][0]
			elif i < 6:
				s.vip = self.test_resources['vip_list'][1]
			else:
				s.vip = self.test_resources['vip_list'][2]
			s.weight = w
			
	def change_config(self, config):
		config['use_director'] = False
	
	def run_user_test(self):
		port = '80'
		sched_algorithm = 'sh'
		ezbox = self.test_resources['ezbox']
		server_list = self.test_resources['server_list']
		client_list = self.test_resources['client_list']
		vip_list = self.test_resources['vip_list']
		
		for vip in vip_list:
			print "service %s is set with %s scheduling algorithm" %(vip,sched_algorithm)
			ezbox.add_service(vip, port, sched_alg=sched_algorithm, sched_alg_opt='')
		
		for server in server_list:
			ezbox.add_server(server.vip, port, server.ip, port)
	
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
									 mac_sa=client_list[0].mac_address.replace(':',' '),
									 ip_dst=ip_to_hex_display(vip_list[0]),
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
			server_list[0].capture_packets_from_service(vip_list[0])
			server_list[1].capture_packets_from_service(vip_list[0])
			server_list[2].capture_packets_from_service(vip_list[0])
			
			# send packet
			time.sleep(5)
			print "Send packets"
			client_list[0].send_packet_to_nps(pcap_to_send)
			
			time.sleep(15)
			
			packets_received_1 = server_list[0].stop_capture()
			packets_received_2 = server_list[1].stop_capture()
			packets_received_3 = server_list[2].stop_capture()
				
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
			
	###########################################################################################################################
	# this scenario check the scheduling algorithm of source hash, ip source is changing, (service is on source port disable) #
	###########################################################################################################################		
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
									 mac_sa=client_list[0].mac_address.replace(':',' '),
									 ip_dst=ip_to_hex_display(vip_list[0]),
									 ip_src=ip_source_address,
									 tcp_source_port = '00 00',
									 tcp_dst_port = '00 50', # port 80
									 packet_length=packet_size)
			data_packet.generate_packet()
			
			packet_list_to_send.append(data_packet.packet)
			
		pcap_to_send = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name='verification/testing/dp/pcap_files/temp_packet.pcap')
	
		# try to capture max 10 times (capture is not stable on all VMs)
		for i in range(10):
			server_list[0].capture_packets_from_service(vip_list[0])
			server_list[1].capture_packets_from_service(vip_list[0])
			server_list[2].capture_packets_from_service(vip_list[0])
			
			time.sleep(5)
			# send packet
			client_list[0].send_packet_to_nps(pcap_to_send)
		
			time.sleep(15)
			
			packets_received_1 = server_list[0].stop_capture()
			packets_received_2 = server_list[1].stop_capture()
			packets_received_3 = server_list[2].stop_capture()
			
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
		print "\nChecking Scenario 3"
	
		# create packets
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
									 mac_sa=client_list[0].mac_address.replace(':',' '),
									 ip_dst=ip_to_hex_display(vip_list[1]),
									 ip_src='192.168.0.100',
									 tcp_source_port = tcp_source_port,
									 tcp_dst_port = '00 50', # port 80
									 packet_length=packet_size) 
			data_packet.generate_packet()
			
			packet_list_to_send.append(data_packet.packet)
			
		pcap_to_send = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name='verification/testing/dp/pcap_files/temp_packet.pcap')
	
		# try to capture max 10 times (capture is not stable on all VMs)
		for i in range(10):
			server_list[3].capture_packets_from_service(vip_list[1])
			server_list[4].capture_packets_from_service(vip_list[1])
			server_list[5].capture_packets_from_service(vip_list[1])
			time.sleep(5)
			
			# send packet
			client_list[0].send_packet_to_nps(pcap_to_send)
		
			time.sleep(15)
			
			packets_received_1 = server_list[3].stop_capture()
			packets_received_2 = server_list[4].stop_capture()
			packets_received_3 = server_list[5].stop_capture()
			
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
				
	############################################################################
	# this scenario checks behavior when the arp table lookup is failing		
	############################################################################		 
		print "\nChecking Scenario 4"
		
		print "Delete the server arp entry"
		ezbox.execute_command_on_host("arp -d %s"%server_list[6].ip)
		
		
		# create packet
		data_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
								 mac_sa=client_list[0].mac_address.replace(':',' '),
								 ip_dst=ip_to_hex_display(vip_list[2]),
								 ip_src=ip_to_hex_display(client_list[0].ip),
								 tcp_source_port = '00 00',
								 tcp_dst_port = '00 50', # port 80
								 packet_length=128)
		data_packet.generate_packet()
	 	
		# send packet
		client_list[0].send_packet_to_nps(data_packet.pcap_file_name)
		
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
	
current_test = Source_Hash_Test()
current_test.main()
