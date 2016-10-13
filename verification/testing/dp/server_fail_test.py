#!/usr/bin/env python

import sys
sys.path.append("verification/testing")
import random
from common_infra import *
from e2e_infra import *

server_count   = 3
client_count   = 1
service_count  = 3

def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
	dict = generic_init(setup_num, service_count, server_count, client_count)
	
	w = 1
	for i,s in enumerate(dict['server_list']):
		s.vip = dict['vip_list'][i]
		s.weight = w
	
	return dict

def main():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	config = fill_default_config(generic_main())
	
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
	for vip in vip_list:
		ezbox.add_service(vip, port, sched_alg=sched_algorithm, sched_alg_opt='')
	
	for server in server_list:
		ezbox.add_server(server.vip, port, server.ip, port)

	# create packet
	data_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
	                         mac_sa=client_list[0].mac_address.replace(':',' '),
	                         ip_dst=ip_to_hex_display(vip_list[0]),
	                         ip_src=ip_to_hex_display(client_list[0].ip),
	                         tcp_source_port = '00 00',
	                         tcp_dst_port = '00 50', # port 80
	                         packet_length=155)
	data_packet.generate_packet()
	
	data_packet_to_the_second_service = tcp_packet(mac_da=ezbox.setup['mac_address'],
	                                               mac_sa=client_list[0].mac_address.replace(':',' '),
	                                               ip_dst=ip_to_hex_display(vip_list[1]),
	                                               ip_src=ip_to_hex_display(client_list[0].ip),
	                                               tcp_source_port = '00 00',
	                                               tcp_dst_port = '00 50', # port 80
	                                               packet_length=113)
	data_packet_to_the_second_service.generate_packet()
	
	data_packet_to_the_third_service = tcp_packet(mac_da=ezbox.setup['mac_address'],
	                                               mac_sa=client_list[0].mac_address.replace(':',' '),
	                                               ip_dst=ip_to_hex_display(vip_list[2]),
	                                               ip_src=ip_to_hex_display(client_list[0].ip),
	                                               tcp_source_port = '00 00',
	                                               tcp_dst_port = '00 50', # port 80
	                                               packet_length=157)
	data_packet_to_the_third_service.generate_packet()

	##########################################################################
	# first check: delete server without existing connection
	##########################################################################
	print "\nfirst check: delete server without existing connection"
	      
	# delete server
	print "Remove a server from service"
	ezbox.delete_server(server_list[0].vip, port, server_list[0].ip, port)
# 	first_service.remove_server(server1)
	        
	# capture all packets on server
	server_list[0].capture_packets_from_service(server_list[0].vip)
	        
	# send packet to the service
	print "Send a packet to service"
	client_list[0].send_packet_to_nps(data_packet.pcap_file_name)
	          
	# check if packets were received
	print "Check if packet was received on server"
	packets_received = server_list[0].stop_capture()
	if packets_received != 0:
	    print "ERROR, alvs didnt forward the packets to server"
	    exit(1)
	        
	print "Packet wasn't received on server"
	 
	# verify that connection wasn't made
	connection=ezbox.get_connection(ip2int(vip_list[0]), port, ip2int(client_list[0].ip) , 0, 6)
	if connection != None:
	    print "ERROR, connection was created, even though server is not exist\n"
	    exit(1)
		     
		# TODO add check statistics
	
	##########################################################################
	# second check: delete service without existing connection
	##########################################################################
	print "\nsecond check: delete service without existing connection"
	      
	# delete server
	print "Delete a service"
	ezbox.delete_service(vip_list[0], port)
	        
	# capture all packets on server
	server_list[0].capture_packets_from_service(server_list[0].vip)
	        
	# send packet
	print "Send a packet to service"
	client_list[0].send_packet_to_nps(data_packet.pcap_file_name)
	    
	time.sleep(1)
	    
	# check if packets were received on server
	print "Check if packet was received on server"
	packets_received = server_list[0].stop_capture()
	if packets_received != 0:
	    print "ERROR, alvs didnt forward the packets to server"
	    exit(1)
	            
	print "Packet wasn't received on server"
	       
	##########################################################################
	# third check: create a connection and then delete server
	##########################################################################
	print "\nthird check: create a connection and then delete server"
	# capture all packets on the server
	server_list[1].capture_packets_from_service(server_list[1].vip)
	        
	# send a packet and create a connection
	print "Create a connection"
	client_list[0].send_packet_to_nps(data_packet_to_the_second_service.pcap_file_name)
	  
	# verify that connection was made
	time.sleep(1)
	connection=ezbox.get_connection(ip2int(vip_list[1]), port, ip2int(client_list[0].ip) , 0, 6)
	if connection == None:
	    print "ERROR, connection wasn't created properly\n"
	    exit(1)
	    
	# check if packet was received on server
	packets_received = server_list[1].stop_capture()
	if packets_received == 0:
	    print "ERROR, alvs didnt forward the packets to server\n"
	    exit(1)
	      
	      
	# delete server
	print "Delete the connection server from the service"
	ezbox.delete_server(server_list[1].vip, port, server_list[1].ip, port)
	        
	# capture all packets on the server
	server_list[1].capture_packets_from_service(server_list[1].vip)
	        
	# send another packet and create a connection
	print "Send a packet to service"
	client_list[0].send_packet_to_nps(data_packet_to_the_second_service.pcap_file_name)
	      
	time.sleep(0.5)
	# check if packets were received on server  
	print "Check if packet was received on server"
	packets_received = server_list[1].stop_capture()
	if packets_received != 0:
	    print "ERROR, alvs didnt forward the packets to server"
	    exit(1)
	      
	print "Packet wasn't received on server"
	       
	# verify that connection was made
	connection=ezbox.get_connection(ip2int(vip_list[1]), port, ip2int(client_list[0].ip) , 0, 6)
	if connection == None:
	    print "ERROR, connection wasn't created properly\n"
	    exit(1)
	      
	print "Verify that the connection was deleted after 16 seconds"    
	time.sleep(CLOSE_WAIT_DELETE_TIME)
	# verify that connection was deleted
	connection=ezbox.get_connection(ip2int(vip_list[1]), port, ip2int(client_list[0].ip) , 0, 6)
	if connection != None:
	    print "ERROR, connection wasn't deleted properly\n"
	    exit(1)
	      
	##########################################################################
	# fourth check: create a connection and then delete service
	##########################################################################
	print "\nfourth check - create a connection and then delete service"
	       
	# add the deleted server from last check
	ezbox.add_server(server_list[1].vip, port, server_list[1].ip, port)
	      
	# send a packet and create a connection
	print "Create a new connection"
	client_list[0].send_packet_to_nps(data_packet_to_the_second_service.pcap_file_name)
	       
	time.sleep(CLOSE_WAIT_DELETE_TIME) 
	         
	# verify that connection was made
	connection=ezbox.get_connection(ip2int(vip_list[1]), port, ip2int(client_list[0].ip) , 0, 6)
	if connection == None:
	    print "ERROR, connection wasn't created properly\n"
	    exit(1) # todo - failing due to bug 812957
	      
	# delete service
	print "Delete Service"
	ezbox.delete_service(vip_list[1], port)
	        
	# capture all packets on the server
	server_list[1].capture_packets_from_service(server_list[1].vip)
	        
	# send another packet and create a connection
	print "Send packet to service"
	client_list[0].send_packet_to_nps(data_packet_to_the_second_service.pcap_file_name)
	      
	time.sleep(1)
	# check if packets were received on server  
	print "Check if packet was received on server"
	packets_received = server_list[1].stop_capture()
	if packets_received != 0:
	    print "ERROR, alvs forward the packets to server"
	    exit(1)
	        
	##########################################################################
	# fifth check: create a connection, delete a server, 
	# send a packet and add this server again, send again packet 
	# and check connection after 16 seconds
	##########################################################################
	print "\nfifth check - check connectivity after delete and create server "
	   
	print "Send a packet - create a connection"
	client_list[0].send_packet_to_nps(data_packet_to_the_third_service.pcap_file_name)
	   
	# verify that connection was made
	connection=ezbox.get_connection(ip2int(vip_list[2]), port, ip2int(client_list[0].ip) , 0, 6)
	if connection == None:
	    print "ERROR, connection wasnt created"
	    exit(1)
	   
	# save the statistics before the packet transmision
	stats_before = ezbox.get_error_stats()
	 
	print "Delete server from service"    
	ezbox.delete_server(server_list[2].vip, port, server_list[2].ip, port)
	   
	# send a packet
	client_list[0].send_packet_to_nps(data_packet_to_the_third_service.pcap_file_name)
	 
	# check that packet was dropped 
	print "Check drop statistics"
	time.sleep(0.5)
	if ezbox.get_error_stats()['ALVS_ERROR_DEST_SERVER_IS_NOT_AVAIL'] != stats_before['ALVS_ERROR_DEST_SERVER_IS_NOT_AVAIL']+1:
	    print "ERROR, ALVS_ERROR_DEST_SERVER_IS_NOT_AVAIL stats is incorrect"
	    print "stats before = %d"%stats_before
	    print "stats after packet transmit = %d"%ezbox.get_error_stats()['ALVS_ERROR_DEST_SERVER_IS_NOT_AVAIL']
	    exit(1)
	     
	# add server back
	print "Add the server back again"
	ezbox.add_server(server_list[2].vip, port, server_list[2].ip, port)
	 
	# capture packets from third service
	server_list[2].capture_packets_from_service(server_list[2].vip)
	    
	# send a packet
	client_list[0].send_packet_to_nps(data_packet_to_the_third_service.pcap_file_name)
	 
	# check if packets were received on server  
	time.sleep(0.5)
	print "Check if packet was received on server"
	packets_received = server_list[2].stop_capture()
	if packets_received == 0:
	    print "ERROR, alvs forward the packets to server"
	    exit(1)
	     
	time.sleep(CLOSE_WAIT_DELETE_TIME)
	        
	connection = ezbox.get_connection(ip2int(vip_list[2]), port, ip2int(client_list[0].ip) , 0, 6)
	if connection == None:
	    print "ERROR, connection wasnt supposed to be deleted"
	    exit(1)
	   
	##########################################################################
	# sixth check: create a connection, delete a service, 
	# send a packet and add this service again, send again packet 
	# and check connection after 16 seconds
	##########################################################################
	print "\nsixth check - check connectivity after delete and create service "
	  
	print "Send a packet - create a connection"
	client_list[0].send_packet_to_nps(data_packet_to_the_third_service.pcap_file_name)
	  
	# verify that connection was made
	connection=ezbox.get_connection(ip2int(vip_list[2]), port, ip2int(client_list[0].ip) , 0, 6)
	if connection == None:
	    print "ERROR, connection wasnt created"
	    exit*(1)
	  
	print "Delete the service"    
	ezbox.delete_service(vip_list[2], port)
	  
	# send a packet
	client_list[0].send_packet_to_nps(data_packet_to_the_third_service.pcap_file_name)
	  
	# create the service again
	ezbox.add_service(vip_list[2], port, sched_alg=sched_algorithm, sched_alg_opt='')
	ezbox.add_server(server_list[2].vip, port, server_list[2].ip, port)
# 	third_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_3, port='80', schedule_algorithm = 'source_hash')
# 	third_service.add_server(server1, weight='1')
	  
	# save stats before packet
	stats_before = ezbox.get_services_stats(service_id=2)
	server_stats_before = ezbox.get_servers_stats(server_id=2)
	
	client_list[0].send_packet_to_nps(data_packet_to_the_third_service.pcap_file_name)
	
	time.sleep(0.5)
	connection = ezbox.get_connection(ip2int(vip_list[2]), port, ip2int(client_list[0].ip) , 0, 6)
	if connection == None:
	    print "ERROR, connection wasnt supposed to be deleted"
	    exit(1)
	  
	  
	# check service statitics  
	print "Check service statistics"
	time.sleep(1)
	stats_after = ezbox.get_services_stats(service_id=2)
	if stats_after['SERVICE_STATS_IN_PKT'] != stats_before['SERVICE_STATS_IN_PKT']+1:
	    print "ERROR, service stats is wrong"
	    exit(1) 
	     
	# check server stats
	print "Check server statistics"
	server_stats_after = ezbox.get_servers_stats(server_id=2)
	if server_stats_after['SERVER_STATS_IN_PKT'] != server_stats_before['SERVER_STATS_IN_PKT']+1:
	    print "ERROR, service stats is wrong"
	    exit(1)
	     
	    
	
	time.sleep(CLOSE_WAIT_DELETE_TIME)
	      
	connection = ezbox.get_connection(ip2int(vip_list[2]), port, ip2int(client_list[0].ip) , 0, 6)
	if connection == None:
	    print "ERROR, connection wasnt supposed to be deleted"
	    exit(1)
	
	# check if packet was received on server
	packets_received = server_list[2].stop_capture()
	if packets_received == 0:
	    print "ERROR, alvs didnt forward the packets to server\n"
	    exit(1) 
	
main()
