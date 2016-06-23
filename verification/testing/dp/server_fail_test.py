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

ezbox = ezbox_host(args['setup_num'])

if args['hard_reset']:
    ezbox.reset_ezbox()

# init ALVS daemon
ezbox.connect()
ezbox.terminate_cp_app()
ezbox.reset_chip()
ezbox.flush_ipvs()
ezbox.copy_cp_bin('bin/alvs_daemon')
ezbox.run_cp()
ezbox.copy_dp_bin('bin/alvs_dp')
ezbox.wait_for_cp_app()
ezbox.run_dp(args='--run_cpus 16-31')

# each setup can use differen VMs
ip_list = get_setup_list(args['setup_num'])

# each setup can use different the virtual ip 
virtual_ip_address_1 = get_setup_vip(args['setup_num'], 0)
virtual_ip_address_2 = get_setup_vip(args['setup_num'], 1)

# create servers
server1 = real_server(management_ip=ip_list[0]['hostname'], data_ip=ip_list[0]['ip'])
server2 = real_server(management_ip=ip_list[1]['hostname'], data_ip=ip_list[1]['ip'])
server3 = real_server(management_ip=ip_list[2]['hostname'], data_ip=ip_list[2]['ip'])
 
# create services on ezbox
first_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_1, port='80', schedule_algorithm = 'source_hash')
first_service.add_server(server1, weight='1')

second_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_2, port='80', schedule_algorithm = 'source_hash')
second_service.add_server(server1, weight='1')

# create client
client_object = client(management_ip=ip_list[3]['hostname'], data_ip=ip_list[3]['ip'])

# create packet
data_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src=client_object.hex_display_to_ip,
                         tcp_source_port = '00 00',
                         tcp_dst_port = '00 50', # port 80
                         packet_length=64)
data_packet.generate_packet()

data_packet_to_the_second_service = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=second_service.virtual_ip_hex_display,
                         ip_src=client_object.hex_display_to_ip,
                         tcp_source_port = '00 00',
                         tcp_dst_port = '00 50', # port 80
                         packet_length=64)
data_packet_to_the_second_service.generate_packet()

##########################################################################
# first check: delete server without existing connection
##########################################################################

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

print 
    
##########################################################################
# second check: delete service without existing connection
##########################################################################

# delete server
print "Delete a service"
first_service.remove_service()

# capture all packets on server
server1.capture_packets_from_service(service=first_service)

# send packet
print "Send a packet to service"
client_object.send_packet_to_nps(data_packet.pcap_file_name)
  
# check if packets were received on server
print "Check if packet was received on server"
packets_received = server1.stop_capture()
if packets_received != 0:
    print "ERROR, alvs didnt forward the packets to server"
    exit(1)
    
print "Packet wasn't received on server"
print

##########################################################################
# third check: create a connection and then delete server
##########################################################################

# capture all packets on the server
server1.capture_packets_from_service(service=second_service)
 
# send a packet and create a connection
print "Create a connection"
client_object.send_packet_to_nps(data_packet_to_the_second_service.pcap_file_name)
 
# check if packet was received on server
packets_received = server1.stop_capture()
if packets_received == 0:
    print "ERROR, alvs didnt forward the packets to server"
    exit(1)
     
# verify that connection was made
connection=ezbox.get_connection(ip2int(second_service.virtual_ip), second_service.port, ip2int(client_object.data_ip) , 0, 6)
if connection == None:
    print "ERROR, connection wasn't created properly\n"
    exit(1)
 
# delete server
print "Delete the connection server from the service"
second_service.remove_server(server1)
 
# capture all packets on the server
server1.capture_packets_from_service(service=second_service)
 
# send another packet and create a connection
print "Send a packet to service"
client_object.send_packet_to_nps(data_packet_to_the_second_service.pcap_file_name)
 
# check if packets were received on server  
print "Check if packet was received on server"
packets_received = server1.stop_capture()
if packets_received != 0:
    print "ERROR, alvs didnt forward the packets to server"
    exit(1)
 
print "Packet wasn't received on server"
 
##########################################################################
# fourth check: create a connection and then delete service
##########################################################################
 
# add the deleted server from last check
second_service.add_server(server1)
 
# send a packet and create a connection
client_object.send_packet_to_nps(data_packet_to_the_second_service.pcap_file_name)
 
# delete service
second_service.remove_service()
 
# capture all packets on the server
server1.capture_packets_from_service(service=second_service)
 
# send another packet and create a connection
client_object.send_packet_to_nps(data_packet_to_the_second_service.pcap_file_name)
 
# check if packets were received on server  
packets_received = server1.stop_capture()
if packets_received != 0:
    print "ERROR, alvs didnt forward the packets to server"
    exit(1)
    
# checkers
# server1.compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')


# tear down
# server1.close()
# server2.close()
# client.close()
# ezbox.close()
