#!/usr/bin/env python

import sys
sys.path.append("verification/testing")
from test_infra import * 
import random

args = read_test_arg(sys.argv)    

log_file = "tcp_flags_test.log"
if 'log_file' in args:
    log_file = args['log_file']
init_logging(log_file)

# scenarios_to_run = args['scenarios']
scenarios_to_run = [1,2,3]

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

# create servers
server1 = real_server(management_ip=ip_list[0]['hostname'], data_ip=ip_list[0]['ip'])
server2 = real_server(management_ip=ip_list[1]['hostname'], data_ip=ip_list[1]['ip'])
server3 = real_server(management_ip=ip_list[2]['hostname'], data_ip=ip_list[2]['ip'])
 
# create client
client_object = client(management_ip=ip_list[3]['hostname'], data_ip=ip_list[3]['ip'])

# create services on ezbox
first_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_1, port='80', schedule_algorithm = 'source_hash')
first_service.add_server(server1, weight='1')

second_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_2, port='80', schedule_algorithm = 'source_hash')
second_service.add_server(server1, weight='1')

# create packet
reset_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src=client_object.hex_display_to_ip,
                         tcp_source_port = '00 00',
                         tcp_dst_port = '00 50', # port 80
                         tcp_reset_flag = True,
                         tcp_fin_flag = False,
                         packet_length=64)
reset_packet.generate_packet()


fin_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src=client_object.hex_display_to_ip,
                         tcp_source_port = '00 00',
                         tcp_dst_port = '00 50', # port 80
                         tcp_reset_flag = False,
                         tcp_fin_flag = True,
                         packet_length=64)
fin_packet.generate_packet()

reset_fin_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src=client_object.hex_display_to_ip,
                         tcp_source_port = '00 00',
                         tcp_dst_port = '00 50', # port 80
                         tcp_reset_flag = True,
                         tcp_fin_flag = True,
                         packet_length=64)
reset_fin_packet.generate_packet()

data_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src=client_object.hex_display_to_ip,
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

# pcaps_with_reset = [reset_fin_packet.pcap_file_name, reset_fin_data_packets, reset_data_fin_packets, fin_reset_data_packets, fin_data_reset_packets, data_fin_reset_packets, data_reset_fin_packets]

pcaps_with_reset = [reset_packet.pcap_file_name, reset_data_fin_packets, fin_reset_data_packets, fin_data_reset_packets, data_fin_reset_packets, data_reset_fin_packets]

pcaps_with_fin_no_reset = [fin_data_packets, data_fin_packets]

for pcap_to_send in pcaps_with_reset:
    ##########################################################################
    # send packet to slow path (connection is not exist)
    ##########################################################################
    print "\nSend %s packets with reset to slow path (connection not exist)\n"%pcap_to_send
    
    # delete and create service
#     print "Delete and Create Service"
#     first_service.remove_service()
#     first_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_1, port='80', schedule_algorithm = 'source_hash')
#     first_service.add_server(server1, weight='1')
#     
#     # verify that connection not exist
#     print "Connection is not exist"
#     connection=ezbox.get_connection(ip2int(first_service.virtual_ip), first_service.port, ip2int(client_object.data_ip) , 0, 6)
#     if connection != None:
#         print "ERROR, connection was created, even though server is not exist\n"
#         exit(1)
#         
#     # capture all packets on server
#     server1.capture_packets_from_service(service=first_service)
#     
#     # send packet
#     print "Send Packet with reset to Service"
#     client_object.send_packet_to_nps(pcap_to_send)
#     
#     # verify that connection was made
#     connection=ezbox.get_connection(ip2int(first_service.virtual_ip), first_service.port, ip2int(client_object.data_ip) , 0, 6)
#     if connection == None:
#         print "ERROR, connection is not exist\n"
#         exit(1)
# 
#     # check how many packets were captured
#     packets_received = server1.stop_capture()
#     
#     if packets_received < 1 or packets_received > 3:
#         print "ERROR, wrong number of packets were received on server"
#         exit(1)
#     
#     print "Connection was made and packet was received on server"    
#     # wait 16 seconds and check that the connection was deleted
#     print "Check that connection is deleting after 16 seconds"
#     time.sleep(CLOSE_WAIT_DELETE_TIME)
#     
#     # verify that connection was deleted
#     connection=ezbox.get_connection(ip2int(first_service.virtual_ip), first_service.port, ip2int(client_object.data_ip) , 0, 6)
#     if connection != None:
#         print "ERROR, connection wasnt deleted properly\n"
#         exit(1)
 
    ##########################################################################
    # run the same with an active connection, (fast path) 
    ##########################################################################
    print "\nNow Send the same packets on fast path (connection exist)\n"
    # send data packet
    print "Send data packet to service and create a connection"
    client_object.send_packet_to_nps(data_packet.pcap_file_name)
    
    # wait 16 seconds and check that the connection was deleted
    time.sleep(CLOSE_WAIT_DELETE_TIME)
    
    # verify that connection exist
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), first_service.port, ip2int(client_object.data_ip) , 0, 6)
    if connection == None:
        print "ERROR, connection was created, even though server is not exist\n"
        exit(1)
        
    # capture all packets on server
    server1.capture_packets_from_service(service=first_service)
    
    exit(1)
    # send packet
#     print "Send the packets with reset to service"
#     client_object.send_packet_to_nps(reset_packet.pcap_file_name) #should be pcap_to_send
#  
#     # verify that connection was made
#     time.sleep(1)
#     connection=ezbox.get_connection(ip2int(first_service.virtual_ip), first_service.port, ip2int(client_object.data_ip) , 0, 6)
#     if connection == None:
#         print "ERROR, connection was created, even though server is not exist\n"
#         exit(1)
#     
#     # check how many packets were captured
#     packets_received = server1.stop_capture()
#     
#     if packets_received < 1 or packets_received > 3: # after reset all packets should be dropped
#         print "ERROR, wrong number of packets were received on server"
#         print "Received %d packets"%packets_received
#         exit(1)
#     
#     print "Connection was made and packet was received on server"  
#     # wait 16 seconds and check that the connection was deleted
#     print "Check that connection is deleting after 16 seconds"
#     time.sleep(CLOSE_WAIT_DELETE_TIME)
#     
#     # verify that connection was deleted
#     connection=ezbox.get_connection(ip2int(first_service.virtual_ip), first_service.port, ip2int(client_object.data_ip) , 0, 6)
#     if connection != None:
#         print "ERROR, connection wasnt deleted properly\n"
#         exit(1)       
        
for pcap_to_send in pcaps_with_fin_no_reset:
    ##########################################################################
    # send packet to slow path (connection is not exist)
    ##########################################################################
    print "\nSend %s packets with fin to slow path (connection not exist)\n"%pcap_to_send
    
    # delete service
    print "Delete Service"
    first_service.remove_service()
    first_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_1, port='80', schedule_algorithm = 'source_hash')
    first_service.add_server(server1, weight='1')
    
    # verify that connection not exist
    print "Connection is not exist"
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), first_service.port, ip2int(client_object.data_ip) , 0, 6)
    if connection != None:
        print "ERROR, connection was created, even though server is not exist\n"
        exit(1)
        
    # capture all packets on server
    server1.capture_packets_from_service(service=first_service)
    
    # send packet
    print "Send Packet to Service"
    client_object.send_packet_to_nps(pcap_to_send)
    
    # verify that connection was made
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), first_service.port, ip2int(client_object.data_ip) , 0, 6)
    if connection == None:
        print "ERROR, connection was created, even though server is not exist\n"
        exit(1)
    
    # check how many packets were captured
    packets_received = server1.stop_capture()
    if packets_received == 0:
        print "ERROR, alvs didnt forward the packets to server"
        exit(1)
    
    print "Connection was made and packet was received on server"    
    # wait 16 seconds and check that the connection was not deleted
    print "Check that connection is not deleting after 16 seconds"
    time.sleep(CLOSE_WAIT_DELETE_TIME)
    
    # verify that connection was not deleted
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), first_service.port, ip2int(client_object.data_ip) , 0, 6)
    if connection == None:
        print "ERROR, connection was deleted\n"
        exit(1)
        
    time.sleep(60-CLOSE_WAIT_DELETE_TIME)
    
    # verify that connection was deleted
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), first_service.port, ip2int(client_object.data_ip) , 0, 6)
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
    client_object.send_packet_to_nps(data_packet.pcap_file_name)
    
    # wait 60 seconds and check that the connection was deleted
    time.sleep(60)
    
    # verify that connection exist
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), first_service.port, ip2int(client_object.data_ip) , 0, 6)
    if connection == None:
        print "ERROR, connection was created, even though server is not exist\n"
        exit(1)
        
    # capture all packets on server
    server1.capture_packets_from_service(service=first_service)
    
    # send packet
    print "Send the packet to service"
    client_object.send_packet_to_nps(pcap_to_send)
    
    # verify that connection was made
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), first_service.port, ip2int(client_object.data_ip) , 0, 6)
    if connection == None:
        print "ERROR, connection was created, even though server is not exist\n"
        exit(1)
    
    # check how many packets were captured
    packets_received = server1.stop_capture()
    if packets_received == 0:
        print "ERROR, alvs didnt forward the packets to server"
        exit(1)
    
    print "Connection was made and packet was received on server"  
    # wait 16 seconds and check that the connection was not deleted
    print "Check that connection is not deleting after 16 seconds"
    time.sleep(CLOSE_WAIT_DELETE_TIME)
    
    # verify that connection was deleted
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), first_service.port, ip2int(client_object.data_ip) , 0, 6)
    if connection == None:
        print "ERROR, connection was deleted\n"
        exit(1)

    print "Check that connection was deleted after 60 seconds"
    time.sleep(60-CLOSE_WAIT_DELETE_TIME)
    
    # verify that connection was deleted
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), first_service.port, ip2int(client_object.data_ip) , 0, 6)
    if connection != None:
        print "ERROR, connection wasnt deleted properly\n"
        exit(1)
        
