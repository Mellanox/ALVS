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

# create servers
server1 = real_server(management_ip=ip_list[0]['hostname'], data_ip=ip_list[0]['ip'])
 
# create client
client_object = client(management_ip=ip_list[3]['hostname'], data_ip=ip_list[3]['ip'])

# create services on ezbox
first_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_1, port='80', schedule_algorithm = 'source_hash')
first_service.add_server(server1, weight='1')

# create packet
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

##########################################################################
# send packet to slow path (connection is not exist)
##########################################################################

# verify that connection is not exist
connection=ezbox.get_connection(ip2int(first_service.virtual_ip), first_service.port, ip2int(client_object.data_ip) , 0, 6)
if connection != None:
	print "ERROR, exist , fail\n"
	exit(1)
	

# capture all packets on server
server1.capture_packets_from_service(service=first_service)

# send packet
print "Send Data Packet to Service"
client_object.send_packet_to_nps(data_packet.pcap_file_name)

# verify that connection exist
time.sleep(0.5)
connection=ezbox.get_connection(ip2int(first_service.virtual_ip), first_service.port, ip2int(client_object.data_ip) , 0, 6)
if connection == None:
	print "ERROR, connection was created, even though server is not exist\n"
	exit(1)
print "Connection exist"

# check how many packets were captured
packets_received = server1.stop_capture()
 
if packets_received !=1:
	print "ERROR, wrong number of packets were received on server"
	exit(1)

# wait 5 minutes
print "Wait 5 minutes and send data packet to this connection again"
time.sleep(60*5)

# verify that connection exist
connection=ezbox.get_connection(ip2int(first_service.virtual_ip), first_service.port, ip2int(client_object.data_ip) , 0, 6)
if connection == None:
	print "ERROR, connection was created, even though server is not exist\n"
	exit(1)
	
# send data packet again
print "Send Data Packet to Service again"
client_object.send_packet_to_nps(data_packet.pcap_file_name)


# wait 5 minutes
print "Wait 16 minutes and check that connection still exist"

for i in range(16):
	# verify that connection exist
	connection=ezbox.get_connection(ip2int(first_service.virtual_ip), first_service.port, ip2int(client_object.data_ip) , 0, 6)
	if connection == None:
		print "ERROR, connection was deleted..\n"
		exit(1)
	print "Connection Exist"
	
	time.sleep(60)
	

time.sleep(20)

# check that connection was deleted
connection=ezbox.get_connection(ip2int(first_service.virtual_ip), first_service.port, ip2int(client_object.data_ip) , 0, 6)
if connection != None:
	print "ERROR, still exist, connection should be deleted..\n"
	exit(1)


print "Connection was deleted after established time"
print
print "Test Passed"
