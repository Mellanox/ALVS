#!/usr/bin/env python

import sys
sys.path.append("verification/testing")
from test_infra import * 
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
	
	global ezbox 
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	config = fill_default_config(generic_main())
	
	#always restart ezbox in Alla's tests
# 	config['start_ezbox'] = True
	config['use_director'] = False
	
	dict = user_init(config['setup_num'])
	
	init_players(dict, config)
	
	server_list, ezbox, client_list, vip_list = convert_generic_init_to_user_format(dict)
	
	run_user_test(server_list, ezbox, client_list, vip_list)
	
	clean_players(dict, True, config['stop_ezbox'])
	
	print "\nTest Passed\n"
# 
def run_user_test(server_list, ezbox, client_list, vip_list):
	port = '80'
	sched_algorithm = '-b sh-fallback'
	
	ezbox.add_service(vip_list[0], port,'sh', sched_algorithm)
	
	ezbox.add_server(server_list[0].vip, port, server_list[0].ip, port)


# ezbox,args = init_test(test_arguments=sys.argv)

# each setup can use differen VMs
# ip_list = get_setup_list(args['setup_num'])

# each setup can use different the virtual ip 
# virtual_ip_address_1 = get_setup_vip(args['setup_num'], 0)
# virtual_ip_address_2 = get_setup_vip(args['setup_num'], 1)

# create servers
# server1 = real_server(management_ip=ip_list[0]['hostname'], data_ip=ip_list[0]['ip'])
# server2 = real_server(management_ip=ip_list[1]['hostname'], data_ip=ip_list[1]['ip'])
# server3 = real_server(management_ip=ip_list[2]['hostname'], data_ip=ip_list[2]['ip'])
#  
# # create client
# client_object = client(management_ip=ip_list[3]['hostname'], data_ip=ip_list[3]['ip'])

# create services on ezbox
# first_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_1, port='80', schedule_algorithm = 'source_hash')
# first_service.add_server(server1, weight='1')
# 
# second_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_2, port='80', schedule_algorithm = 'source_hash')
# second_service.add_server(server1, weight='1')

	# create packet
	reset_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
							 mac_sa=client_list[0].mac_address.replace(':',' '),
							 ip_dst=ip_to_hex_display(vip_list[0]),
							 ip_src=ip_to_hex_display(client_list[0].ip),
							 tcp_source_port = '00 00',
							 tcp_dst_port = '00 50', # port 80
							 tcp_reset_flag = True,
							 tcp_fin_flag = False,
							 packet_length=64)
	reset_packet.generate_packet()


	fin_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
							 mac_sa=client_list[0].mac_address.replace(':',' '),
							 ip_dst=ip_to_hex_display(vip_list[0]),
							 ip_src=ip_to_hex_display(client_list[0].ip),
							 tcp_source_port = '00 00',
							 tcp_dst_port = '00 50', # port 80
							 tcp_reset_flag = False,
							 tcp_fin_flag = True,
							 packet_length=64)
	fin_packet.generate_packet()
	
	reset_fin_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
							 mac_sa=client_list[0].mac_address.replace(':',' '),
							 ip_dst=ip_to_hex_display(vip_list[0]),
							 ip_src=ip_to_hex_display(client_list[0].ip),
							 tcp_source_port = '00 00',
							 tcp_dst_port = '00 50', # port 80
							 tcp_reset_flag = True,
							 tcp_fin_flag = True,
							 packet_length=64)
	reset_fin_packet.generate_packet()
	
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



	reset_fin_data_packets = create_pcap_file(packets_list=[reset_packet.packet,fin_packet.packet,data_packet.packet], output_pcap_file_name='verification/testing/dp/pcap_files/temp_packet1.pcap')
	reset_data_fin_packets = create_pcap_file(packets_list=[reset_packet.packet,data_packet.packet,fin_packet.packet], output_pcap_file_name='verification/testing/dp/pcap_files/temp_packet2.pcap')
	fin_reset_data_packets = create_pcap_file(packets_list=[fin_packet.packet,reset_packet.packet,data_packet.packet], output_pcap_file_name='verification/testing/dp/pcap_files/temp_packet3.pcap')
	fin_data_reset_packets = create_pcap_file(packets_list=[fin_packet.packet,data_packet.packet,reset_packet.packet], output_pcap_file_name='verification/testing/dp/pcap_files/temp_packet4.pcap')
	data_fin_reset_packets = create_pcap_file(packets_list=[data_packet.packet,fin_packet.packet,reset_packet.packet], output_pcap_file_name='verification/testing/dp/pcap_files/temp_packet5.pcap')
	data_reset_fin_packets = create_pcap_file(packets_list=[data_packet.packet,reset_packet.packet,fin_packet.packet], output_pcap_file_name='verification/testing/dp/pcap_files/temp_packet6.pcap')
	fin_data_packets = create_pcap_file(packets_list=[fin_packet.packet, data_packet.packet], output_pcap_file_name='verification/testing/dp/pcap_files/temp_packet7.pcap')
	data_fin_packets = create_pcap_file(packets_list=[data_packet.packet,fin_packet.packet], output_pcap_file_name='verification/testing/dp/pcap_files/temp_packet8.pcap')
	
	pcaps_with_reset = [reset_fin_packet.pcap_file_name, reset_fin_data_packets, reset_data_fin_packets, fin_reset_data_packets, fin_data_reset_packets, data_fin_reset_packets, data_reset_fin_packets]
	
	pcaps_with_fin_no_reset = [fin_data_packets, data_fin_packets]

	for index,pcap_to_send in enumerate(pcaps_with_reset):
		##########################################################################
		# send packet to slow path (connection is not exist)
		##########################################################################
		print "\nSend packet number %d from %d packets with reset to slow path (connection not exist)\n"%(index+1,len(pcaps_with_reset))
		
		# delete and create service
		print "Delete and Create Service"
		ezbox.delete_service(vip_list[0], port)
		ezbox.add_service(vip_list[0], port,'sh', sched_algorithm)
		ezbox.add_server(server_list[0].vip, port, server_list[0].ip, port)
		
# 		
# 		first_service.remove_service()
# 		first_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_1, port='80', schedule_algorithm = 'source_hash')
# 		first_service.add_server(server1, weight='1')
		
		# verify that connection not exist
		print "Connection is not exist"
		connection=ezbox.get_connection(ip2int(vip_list[0]), port, ip2int(client_list[0].ip) , 0, 6)
		if connection != None:
			print "ERROR, connection was created, even though server is not exist\n"
			exit(1)
			
		# capture all packets on server
		server_list[0].capture_packets_from_service(server_list[0].vip)
		
		# take statistics before packet was send
		packet_drop_before = ezbox.get_error_stats()['ALVS_ERROR_CONN_MARK_TO_DELETE']
		
		# create a connection and verify that connection was made
		for num_of_sent_packets in range(1,30):
			# send packet
			print "Send the packets with reset to service"
			client_list[0].send_packet_to_nps(pcap_to_send)
			time.sleep(1)
			# verify connection was made
			connection=ezbox.get_connection(ip2int(vip_list[0]), port, ip2int(client_list[0].ip) , 0, 6)
			if connection == None:
				if num_of_sent_packets > 20:
					print "ERROR, connection wasnt created\n"
					exit(1)
				else:
					print "Connection is not exist, maybe it was deleted too quickly, trying again"
					continue
			else:
				break
			
		# check how many packets were captured
		time.sleep(1)
		packets_received = server_list[0].stop_capture()
	
		if pcap_to_send == reset_fin_packet.pcap_file_name:
			if packets_received != 1*num_of_sent_packets:
				ezbox.print_all_stats()
				print "ERROR, wrong number of packets were received on server"
				exit(1)
		else:		
			packet_drop = ezbox.get_error_stats()['ALVS_ERROR_CONN_MARK_TO_DELETE']-packet_drop_before
			if packet_drop+packets_received != 3*num_of_sent_packets:
				ezbox.print_all_stats()
				print "ERROR, wrong number of packets were received on server"
				exit(1)
	
		print "Connection was made and packet was received on server"	
		
		# wait 16 seconds and check that the connection was deleted
		print "Check that connection is deleting after 16 seconds"
		time.sleep(CLOSE_WAIT_DELETE_TIME)
	
		# verify that connection was deleted
		connection=ezbox.get_connection(ip2int(vip_list[0]), port, ip2int(client_list[0].ip) , 0, 6)
		if connection != None:
			print "ERROR, connection wasnt deleted properly\n"
			exit(1)
	
		##########################################################################
		# run the same with an active connection, (fast path) 
		##########################################################################
		print "\nSend packet number %d from %d packets with reset to fast path (connection exist)\n"%(index+1,len(pcaps_with_reset))
		# send data packet
		print "Send data packet to service and create a connection"
		client_list[0].send_packet_to_nps(data_packet.pcap_file_name)
		
		# wait 16 seconds and check that the connection was deleted
		time.sleep(CLOSE_WAIT_DELETE_TIME)
		
		# verify that connection exist
		connection=ezbox.get_connection(ip2int(vip_list[0]), port, ip2int(client_list[0].ip) , 0, 6)
		if connection == None:
			print "ERROR, connection was created, even though server is not exist\n"
			exit(1)
			
		# capture all packets on server
		server_list[0].capture_packets_from_service(server_list[0].vip)
		
		# take statistics before packet was send
		packet_drop_before = ezbox.get_error_stats()['ALVS_ERROR_CONN_MARK_TO_DELETE']
	
		# create a connection and verify that connection was made
		for num_of_sent_packets in range(1,30):
			# send packet
			print "Send the packets with reset to service"
			client_list[0].send_packet_to_nps(pcap_to_send)
			time.sleep(1)
			connection=ezbox.get_connection(ip2int(vip_list[0]), port, ip2int(client_list[0].ip) , 0, 6)
			if connection == None:
				if num_of_sent_packets > 20:
					print "ERROR, connection wasnt created\n"
					exit(1)
				else:
					print "Connection is not exist, maybe it was deleted too quickly, trying again"
					continue
			else:
				break
		
		# check how many packets were captured
		time.sleep(0.5)
		packets_received = server_list[0].stop_capture()
	
		print "pcap_to_send %s"%pcap_to_send
		print "packet_drop_before %d"%int(packet_drop_before)
		print "packets_received %d"%int(packets_received)
		print "num_of_sent_packets %d"%int(num_of_sent_packets)
		if pcap_to_send == reset_fin_packet.pcap_file_name:
			if packets_received != 1*num_of_sent_packets:
				ezbox.print_all_stats()
				print "ERROR, wrong number of packets were received on server"
				exit(1)
		else:
			packet_drop = ezbox.get_error_stats()['ALVS_ERROR_CONN_MARK_TO_DELETE']-packet_drop_before
			print "packet_drop %d"%int(packet_drop)
			if packet_drop+packets_received != 3*num_of_sent_packets:
				ezbox.print_all_stats()
				print "ERROR, wrong number of packets were received on server"
				exit(1)
	
		print "Connection was made and packet was received on server"  
		# wait 16 seconds and check that the connection was deleted
		print "Check that connection is deleting after 16 seconds"
		time.sleep(CLOSE_WAIT_DELETE_TIME)
	
		# verify that connection was deleted
		connection=ezbox.get_connection(ip2int(vip_list[0]), port, ip2int(client_list[0].ip) , 0, 6)
		if connection != None:
			print "ERROR, connection wasnt deleted properly\n"
			exit(1)
		
	for pcap_to_send in pcaps_with_fin_no_reset:
		##########################################################################
		# send packet to slow path (connection is not exist)
		##########################################################################
		print "\nSend %s packets with fin to slow path (connection not exist)\n"%pcap_to_send
		
		# delete service
		print "Delete Service"
		ezbox.delete_service(vip_list[0], port)
		ezbox.add_service(vip_list[0], port,'sh', sched_algorithm)
		ezbox.add_server(server_list[0].vip, port, server_list[0].ip, port)
		

# 		first_service.remove_service()
# 		first_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_1, port='80', schedule_algorithm = 'source_hash')
# 		first_service.add_server(server1, weight='1')
		
		# verify that connection not exist
		print "Connection is not exist"
		connection=ezbox.get_connection(ip2int(vip_list[0]), port, ip2int(client_list[0].ip) , 0, 6)
		if connection != None:
			print "ERROR, connection was created, even though server is not exist\n"
			exit(1)
			
		# capture all packets on server
		server_list[0].capture_packets_from_service(server_list[0].vip)
		
		# send the packet and create a connection and verify that connection was made
		for num_of_sent_packets in range(1,30):
			# send packet
			print "Send the packets with reset to service"
			client_list[0].send_packet_to_nps(pcap_to_send)
			time.sleep(1)
			connection=ezbox.get_connection(ip2int(vip_list[0]), port, ip2int(client_list[0].ip) , 0, 6)
			if connection == None:
				if num_of_sent_packets > 20:
					print "ERROR, connection wasnt created\n"
					exit(1)
				else:
					print "Connection is not exist, maybe it was deleted too quickly, trying again"
					continue
			else:
				break
			
		# check how many packets were captured
		time.sleep(0.5)
		packets_received = server_list[0].stop_capture()
		if packets_received == 0:
			print "ERROR, alvs didnt forward the packets to server"
			exit(1)
		
		print "Connection was made and packet was received on server"	
		# wait 16 seconds and check that the connection was not deleted
		print "Check that connection is not deleting after 16 seconds"
		time.sleep(CLOSE_WAIT_DELETE_TIME-2)
		
		# verify that connection was not deleted
		connection=ezbox.get_connection(ip2int(vip_list[0]), port, ip2int(client_list[0].ip) , 0, 6)
		if connection == None:
			print "ERROR, connection was deleted\n"
			exit(1)
			
		time.sleep(60-(CLOSE_WAIT_DELETE_TIME-2))
		
		# verify that connection was deleted
		connection=ezbox.get_connection(ip2int(vip_list[0]), port, ip2int(client_list[0].ip) , 0, 6)
		if connection != None:
			print "ERROR, connection wasnt deleted properly\n"
			exit(1)
		
		print "Connection is deleted after 60 seconds"
		
		##########################################################################
		# run the same with an active connection, (fast path) 
		##########################################################################
		print "\nNow Send the same packet on fast path (connection exist)\n"
		# send data packet
		print "Send data packet to service and create a connection"
		client_list[0].send_packet_to_nps(data_packet.pcap_file_name)
		
		# wait 60 seconds and check that the connection was deleted
		time.sleep(60)
		
		# verify that connection exist
		connection=ezbox.get_connection(ip2int(vip_list[0]), port, ip2int(client_list[0].ip) , 0, 6)
		if connection == None:
			print "ERROR, connection was created, even though server is not exist\n"
			exit(1)
			
		# capture all packets on server
		server_list[0].capture_packets_from_service(vip_list[0])
		
		# send the packet and create a connection and verify that connection was made
		for num_of_sent_packets in range(1,30):
			# send packet
			print "Send the packets with reset to service"
			client_list[0].send_packet_to_nps(pcap_to_send)
			time.sleep(1)
			connection=ezbox.get_connection(ip2int(vip_list[0]), first_service.port, ip2int(client_list[0].ip) , 0, 6)
			if connection == None:
				if num_of_sent_packets > 20:
					print "ERROR, connection wasnt created\n"
					exit(1)
				else:
					print "Connection is not exist, maybe it was deleted too quickly, trying again"
					continue
			else:
				break
		
		# check how many packets were captured
		time.sleep(0.5)
		packets_received = server_list[0].stop_capture()
		if packets_received == 0:
			print "ERROR, alvs didnt forward the packets to server"
			exit(1)
		
		print "Connection was made and packet was received on server"  
		# wait 16 seconds and check that the connection was not deleted
		print "Check that connection is not deleting after 16 seconds"
		time.sleep(CLOSE_WAIT_DELETE_TIME-5)
		
		# verify that connection was deleted
		connection=ezbox.get_connection(ip2int(vip_list[0]), port, ip2int(client_list[0].ip) , 0, 6)
		if connection == None:
			print "ERROR, connection was deleted\n"
			exit(1)
	
		print "Check that connection was deleted after 60 seconds (max time to delete connection after fin packet)"
		time.sleep(60-(CLOSE_WAIT_DELETE_TIME-5))
		
		# verify that connection was deleted
		connection=ezbox.get_connection(ip2int(vip_list[0]), port, ip2int(client_list[0].ip) , 0, 6)
		if connection != None:
			print "ERROR, connection wasnt deleted properly\n"
			exit(1)
			
main()
