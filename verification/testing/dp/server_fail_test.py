#!/usr/bin/env python

import sys
sys.path.append("verification/testing")
from test_infra import * 
import random

args = read_test_arg(sys.argv)    

log_file = "server_fail_test.log"
if 'log_file' in args:
    log_file = args['log_file']
init_logging(log_file)

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
# server2 = real_server(management_ip=ip_list[1]['hostname'], data_ip=ip_list[1]['ip'])
# server3 = real_server(management_ip=ip_list[2]['hostname'], data_ip=ip_list[2]['ip'])
 
# create services on ezbox
first_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_1, port='80', schedule_algorithm = 'source_hash')
first_service.add_server(server1, weight='1')

second_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_2, port='80', schedule_algorithm = 'source_hash')
second_service.add_server(server1, weight='1')

third_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_3, port='80', schedule_algorithm = 'source_hash')
third_service.add_server(server1, weight='1')

# set the director
# ezbox.init_director(service.services_dictionary)

# create client
client_object = client(management_ip=ip_list[3]['hostname'], data_ip=ip_list[3]['ip'])

# create packet
data_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src=client_object.hex_display_to_ip,
                         tcp_source_port = '00 00',
                         tcp_dst_port = '00 50', # port 80
                         packet_length=155)
data_packet.generate_packet()

data_packet_to_the_second_service = tcp_packet(mac_da=ezbox.setup['mac_address'],
                                               mac_sa=client_object.mac_address,
                                               ip_dst=second_service.virtual_ip_hex_display,
                                               ip_src=client_object.hex_display_to_ip,
                                               tcp_source_port = '00 00',
                                               tcp_dst_port = '00 50', # port 80
                                               packet_length=113)
data_packet_to_the_second_service.generate_packet()

data_packet_to_the_third_service = tcp_packet(mac_da=ezbox.setup['mac_address'],
                                               mac_sa=client_object.mac_address,
                                               ip_dst=third_service.virtual_ip_hex_display,
                                               ip_src=client_object.hex_display_to_ip,
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
first_service.remove_server(server1)
        
# capture all packets on server
server1.capture_packets_from_service(service=first_service)
        
# send packet to the service
print "Send a packet to service"
client_object.send_packet_to_nps(data_packet.pcap_file_name)
          
# check if packets were received
print "Check if packet was received on server"
packets_received = server1.stop_capture()
if packets_received != 0:
    print "ERROR, alvs didnt forward the packets to server"
    exit(1)
        
print "Packet wasn't received on server"
 
# verify that connection wasn't made
connection=ezbox.get_connection(ip2int(first_service.virtual_ip), first_service.port, ip2int(client_object.data_ip) , 0, 6)
if connection != None:
    print "ERROR, connection was created, even though server is not exist\n"
    exit(1)
     
# check statistics
 
            
##########################################################################
# second check: delete service without existing connection
##########################################################################
print "\nsecond check: delete service without existing connection"
      
# delete server
print "Delete a service"
first_service.remove_service()
        
# capture all packets on server
server1.capture_packets_from_service(service=first_service)
        
# send packet
print "Send a packet to service"
client_object.send_packet_to_nps(data_packet.pcap_file_name)
    
time.sleep(1)
    
# check if packets were received on server
print "Check if packet was received on server"
packets_received = server1.stop_capture()
if packets_received != 0:
    print "ERROR, alvs didnt forward the packets to server"
    exit(1)
            
print "Packet wasn't received on server"
       
##########################################################################
# third check: create a connection and then delete server
##########################################################################
print "\nthird check: create a connection and then delete server"
# capture all packets on the server
server1.capture_packets_from_service(service=second_service)
        
# send a packet and create a connection
print "Create a connection"
client_object.send_packet_to_nps(data_packet_to_the_second_service.pcap_file_name)
  
# verify that connection was made
time.sleep(1)
connection=ezbox.get_connection(ip2int(second_service.virtual_ip), second_service.port, ip2int(client_object.data_ip) , 0, 6)
if connection == None:
    print "ERROR, connection wasn't created properly\n"
    exit(1)
    
# check if packet was received on server
packets_received = server1.stop_capture()
if packets_received == 0:
    print "ERROR, alvs didnt forward the packets to server\n"
    exit(1)
      
      
# delete server
print "Delete the connection server from the service"
second_service.remove_server(server1)
        
# capture all packets on the server
server1.capture_packets_from_service(service=second_service)
        
# send another packet and create a connection
print "Send a packet to service"
client_object.send_packet_to_nps(data_packet_to_the_second_service.pcap_file_name)
      
time.sleep(0.5)
# check if packets were received on server  
print "Check if packet was received on server"
packets_received = server1.stop_capture()
if packets_received != 0:
    print "ERROR, alvs didnt forward the packets to server"
    exit(1)
      
print "Packet wasn't received on server"
       
# verify that connection was made
connection=ezbox.get_connection(ip2int(second_service.virtual_ip), second_service.port, ip2int(client_object.data_ip) , 0, 6)
if connection == None:
    print "ERROR, connection wasn't created properly\n"
    exit(1)
      
print "Verify that the connection was deleted after 16 seconds"    
time.sleep(CLOSE_WAIT_DELETE_TIME)
# verify that connection was deleted
connection=ezbox.get_connection(ip2int(second_service.virtual_ip), second_service.port, ip2int(client_object.data_ip) , 0, 6)
if connection != None:
    print "ERROR, connection wasn't deleted properly\n"
    exit(1)
      
##########################################################################
# fourth check: create a connection and then delete service
##########################################################################
print "\nfourth check - create a connection and then delete service"
       
# add the deleted server from last check
second_service.add_server(server1)
      
# send a packet and create a connection
print "Create a new connection"
client_object.send_packet_to_nps(data_packet_to_the_second_service.pcap_file_name)
       
time.sleep(CLOSE_WAIT_DELETE_TIME) 
         
# verify that connection was made
connection=ezbox.get_connection(ip2int(second_service.virtual_ip), second_service.port, ip2int(client_object.data_ip) , 0, 6)
if connection == None:
    print "ERROR, connection wasn't created properly\n"
    exit(1) # todo - failing due to bug 812957
      
# delete service
print "Delete Service"
second_service.remove_service()
        
# capture all packets on the server
server1.capture_packets_from_service(service=second_service)
        
# send another packet and create a connection
print "Send packet to service"
client_object.send_packet_to_nps(data_packet_to_the_second_service.pcap_file_name)
      
time.sleep(1)
# check if packets were received on server  
print "Check if packet was received on server"
packets_received = server1.stop_capture()
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
client_object.send_packet_to_nps(data_packet_to_the_third_service.pcap_file_name)
   
# verify that connection was made
connection=ezbox.get_connection(ip2int(third_service.virtual_ip), third_service.port, ip2int(client_object.data_ip) , 0, 6)
if connection == None:
    print "ERROR, connection wasnt created"
    exit(1)
   
# save the statistics before the packet transmision
stats_before = ezbox.get_error_stats()
 
print "Delete server from service"    
third_service.remove_server(server1)
   
# send a packet
client_object.send_packet_to_nps(data_packet_to_the_third_service.pcap_file_name)
 
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
third_service.add_server(server1)
 
# capture packets from third service
server1.capture_packets_from_service(service=third_service)
    
# send a packet
client_object.send_packet_to_nps(data_packet_to_the_third_service.pcap_file_name)
 
# check if packets were received on server  
time.sleep(0.5)
print "Check if packet was received on server"
packets_received = server1.stop_capture()
if packets_received == 0:
    print "ERROR, alvs forward the packets to server"
    exit(1)
     
time.sleep(CLOSE_WAIT_DELETE_TIME)
        
connection = ezbox.get_connection(ip2int(third_service.virtual_ip), third_service.port, ip2int(client_object.data_ip) , 0, 6)
if connection == None:
    print "ERROR, connection wasnt supposed to be deleted"
    exit(1)
   
# ##########################################################################
# # sixth check: create a connection, delete a service, 
# # send a packet and add this service again, send again packet 
# # and check connection after 16 seconds
# ##########################################################################
# print "\nsixth check - check connectivity after delete and create service "
#   
# print "Send a packet - create a connection"
# client_object.send_packet_to_nps(data_packet_to_the_third_service.pcap_file_name)
#   
# # verify that connection was made
# connection=ezbox.get_connection(ip2int(third_service.virtual_ip), third_service.port, ip2int(client_object.data_ip) , 0, 6)
# if connection == None:
#     print "ERROR, connection wasnt created"
#     exit*(1)
#   
# print "Delete the service"    
# third_service.remove_service()
#   
# # send a packet
# client_object.send_packet_to_nps(data_packet_to_the_third_service.pcap_file_name)
#   
# # create the service again
# third_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_3, port='80', schedule_algorithm = 'source_hash')
# third_service.add_server(server1, weight='1')
#   
# # save stats before packet
# stats_before = ezbox.get_services_stats(service_id=2)
# server_stats_before = ezbox.get_servers_stats(server_id=2)
# 
# client_object.send_packet_to_nps(data_packet_to_the_third_service.pcap_file_name)
# 
# time.sleep(0.5)
# connection = ezbox.get_connection(ip2int(third_service.virtual_ip), third_service.port, ip2int(client_object.data_ip) , 0, 6)
# if connection == None:
#     print "ERROR, connection wasnt supposed to be deleted"
#     exit(1)
#   
  
# # check service statitics  
# print "Check service statistics"
# time.sleep(1)
# stats_after = ezbox.get_services_stats(service_id=2)
# if stats_after['SERVICE_STATS_IN_PKT'] != stats_before['SERVICE_STATS_IN_PKT']+1:
#     print "ERROR, service stats is wrong"
#     exit(1) 
#      
# # check server stats
# print "Check server statistics"
# server_stats_after = ezbox.get_servers_stats(server_id=2)
# if stats_after['SERVER_STATS_IN_PKT'] != stats_before['SERVER_STATS_IN_PKT']+1:
#     print "ERROR, service stats is wrong"
#     exit(1)
     
#     
# 
# time.sleep(CLOSE_WAIT_DELETE_TIME)
#       
# connection = ezbox.get_connection(ip2int(third_service.virtual_ip), third_service.port, ip2int(client_object.data_ip) , 0, 6)
# if connection == None:
#     print "ERROR, connection wasnt supposed to be deleted"
#     exit(1)

# # check if packet was received on server
# packets_received = server1.stop_capture()
# if packets_received == 0:
#     print "ERROR, alvs didnt forward the packets to server\n"
#     exit(1)
 
print 
print "Test Passed"

# checkers
# server1.compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')

# tear down
# server1.close()
# server2.close()
# client.close()
# ezbox.close()
