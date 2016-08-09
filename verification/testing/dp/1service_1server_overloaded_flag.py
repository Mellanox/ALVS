#!/usr/bin/env python

import sys
sys.path.append("verification/testing")
from test_infra import * 
import random

args = read_test_arg(sys.argv)    

log_file = "1service_1server_overloaded_flag.log"
if 'log_file' in args:
    log_file = args['log_file']
init_logging(log_file)

# scenarios_to_run = args['scenarios']
scenarios_to_run = [1]

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

# create servers
server1 = real_server(management_ip=ip_list[0]['hostname'], data_ip=ip_list[0]['ip'])

# create services on ezbox
u_thresh = 4
l_thresh = 2
first_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_1, port='80', schedule_algorithm = 'source_hash_with_source_port')
first_service.add_server(server1, weight='1', u_thresh = u_thresh, l_thresh = l_thresh)

# create client
client_object = client(management_ip=ip_list[3]['hostname'], data_ip=ip_list[3]['ip'])

print "**************************************************************************"
print "PART 1 - U_THRESH is upper bound for new created connections"
    
 # create packets
packet_list_to_send = []
error_stats = []
long_counters = []
num_of_packets = 7
src_port_list = []
for i in range(num_of_packets):
    # set the packet size
    #packet_size = random.randint(60,1500)
    packet_size = 150
    random_source_port = '%02x %02x'%(random.randint(1,255),random.randint(1,255))

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
    
    src_port_list.append(int(random_source_port.replace(' ',''),16))
     
pcap_to_send = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name='verification/testing/dp/temp_packet1.pcap')

time.sleep(2) 
server1.capture_packets_from_service(service=first_service)
   
    # send packet
time.sleep(5)
print "Send packets"
client_object.send_packet_to_nps(pcap_to_send)
  
time.sleep(5)
print "capture from server"
packets_received_1 = server1.stop_capture()
print "Server 1  - received %d packets"%packets_received_1

 #x = ezbox.cpe.cp.prm.read_mem(channel_id=0, mem_id=0, mem_space_index=msid, lsb_address=addr, msb_address=0, single_copy=False, gci_copy=0, copy_index=0, length=4)

 #print x;
#sched_connections_on_server =  ezbox.cpe.cp.stat.get_long_counter_values(channel_id=0, partition=0, range=None, 
#                                                     start_counter=EMEM_SERVER_STATS_ON_DEMAND_OFFSET + 0 * ALVS_NUM_OF_SERVERS_ON_DEMAND_STATS, 
#                                                     num_counters=ALVS_NUM_OF_SERVERS_ON_DEMAND_STATS, 
#                                                     counters=long_counters, read=0, use_shadow_group=0)
sched_connections_on_server = ezbox.get_server_sched_connections_stats(0)
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
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), 80, ip2int(client_object.data_ip) , src_port, 6)
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
   (expected_num_of_errors == server_is_unavailable_error)):

#     server1.compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
    print "PART1 Passed"
else:
    print "PART1 Faild"
    exit(1)
     
print "**************************************************************************"
print "PART 2 - U_THRESH  = L_THRESH = 0 - disable feature - allow all connections"

u_thresh = 0
l_thresh = 0
first_service.modify_server(server1, weight=1, u_thresh=u_thresh, l_thresh=l_thresh)

time.sleep(2) 
server1.capture_packets_from_service(service=first_service)
   
    # send packet
time.sleep(5)
print "Send packets"
client_object.send_packet_to_nps(pcap_to_send)
  
time.sleep(5)
print "capture from server"
packets_received_1 = server1.stop_capture()

 #x = ezbox.cpe.cp.prm.read_mem(channel_id=0, mem_id=0, mem_space_index=msid, lsb_address=addr, msb_address=0, single_copy=False, gci_copy=0, copy_index=0, length=4)

 #print x;
#sched_connections_on_server =  ezbox.cpe.cp.stat.get_long_counter_values(channel_id=0, partition=0, range=None, 
#                                                     start_counter=EMEM_SERVER_STATS_ON_DEMAND_OFFSET + 0 * ALVS_NUM_OF_SERVERS_ON_DEMAND_STATS, 
#                                                     num_counters=ALVS_NUM_OF_SERVERS_ON_DEMAND_STATS, 
#                                                     counters=long_counters, read=0, use_shadow_group=0)

print "Server 1  - received %d packets"%packets_received_1

sched_connections_on_server = ezbox.get_server_sched_connections_stats(0)
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
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), 80, ip2int(client_object.data_ip) , src_port, 6)
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

#     server1.compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
    print "PART2 Passed"
else:
    print "PART2 Failed"
    exit(1)

print "**************************************************************************"
print "PART 3 - remove connection below l_thresh and check OVERLOADED is OFF" 
u_thresh = 6
l_thresh = 3
first_service.modify_server(server1, weight=1, u_thresh=u_thresh, l_thresh=l_thresh)

print "PART 3 - A - try create new connection u_thresh = %d l_thresh = %d current_connection = %d"%(u_thresh, l_thresh, sched_connections_on_server)
print "modify server with u_thresh %d instead of 0 and l_thresh %d instead of 0"%(u_thresh, l_thresh)
print "number of current_connection %d higher than  u_thresh = %d "%(sched_connections_on_server, u_thresh)
print "New connection shouldn't be created"

num_of_packets = 1
packet_size = 150
random_source_port = '%02x %02x'%(random.randint(1,255),random.randint(1,255))

# create packet
data_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src=client_object.hex_display_to_ip,
                         tcp_source_port = random_source_port,
                         tcp_dst_port = '00 50', # port 80
                         packet_length=packet_size)
data_packet.generate_packet()

time.sleep(2) 
server1.capture_packets_from_service(service=first_service)

pcap_to_send_part3 = create_pcap_file(packets_list=[data_packet.packet], output_pcap_file_name='verification/testing/dp/temp_packet3_a.pcap')
    
    # send packet
time.sleep(5)
print "Send packets"
client_object.send_packet_to_nps(pcap_to_send_part3)
  
time.sleep(5)
print "capture from server"
packets_received_1 = server1.stop_capture()

print "Server 1  - received %d packets"%packets_received_1

sched_connections_on_server = ezbox.get_server_sched_connections_stats(0)
print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server

error_stats = ezbox.get_error_stats()     
print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']

created_connections = 0
server_is_unavailable_error = 0

for src_port in src_port_list:   
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), 80, ip2int(client_object.data_ip) , src_port, 6)
    if connection != None:
        created_connections +=1
    else:
        server_is_unavailable_error +=1

connection=ezbox.get_connection(ip2int(first_service.virtual_ip), 80, ip2int(client_object.data_ip) , int(random_source_port.replace(' ',''),16), 6)
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

#     server1.compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
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
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src=client_object.hex_display_to_ip,
                         tcp_source_port = source_port,
                         tcp_dst_port = '00 50', # port 80
                         tcp_reset_flag = True,
                         packet_length=packet_size)
reset_packet.generate_packet()

source_port="%04x"%src_port_list[1]
source_port = source_port[0:2] + ' ' + source_port[2:4]
fin_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src=client_object.hex_display_to_ip,
                         tcp_source_port = source_port,
                         tcp_dst_port = '00 50', # port 80
                         tcp_fin_flag = True,
                         packet_length=packet_size)
fin_packet.generate_packet()   
    
pcap_to_send = create_pcap_file(packets_list=[reset_packet.packet,fin_packet.packet], output_pcap_file_name='verification/testing/dp/temp_packet3_b.pcap')
  
time.sleep(2) 
server1.capture_packets_from_service(service=first_service)
    
    # send packet
time.sleep(5)
print "Send packets"
client_object.send_packet_to_nps(pcap_to_send)
  
time.sleep(5)
print "capture from server"
packets_received_1 = server1.stop_capture()

time.sleep(20)

print "Server 1  - received %d packets"%packets_received_1

sched_connections_on_server = ezbox.get_server_sched_connections_stats(0)
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
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), 80, ip2int(client_object.data_ip) , src_port, 6)
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

#     server1.compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
    print "PART3 - B - Passed"
else:
    print "PART3 - B-  Failed"
    exit(1)

num_of_packets_c = 1
print "PART 3 - C - send %d frames for create connection "%(num_of_packets_c)
print "Connection shouldn't be created"

packet_size = 150
random_source_port = '%02x %02x'%(random.randint(1,255),random.randint(1,255))

# create packet
data_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src=client_object.hex_display_to_ip,
                         tcp_source_port = random_source_port,
                         tcp_dst_port = '00 50', # port 80
                         packet_length=packet_size)
data_packet.generate_packet()

time.sleep(2) 
server1.capture_packets_from_service(service=first_service)

pcap_to_send_part3 = create_pcap_file(packets_list=[data_packet.packet], output_pcap_file_name='verification/testing/dp/temp_packet3_c.pcap')
    
    # send packet
time.sleep(5)
print "Send packets"
client_object.send_packet_to_nps(pcap_to_send_part3)
  
time.sleep(5)
print "capture from server"
packets_received_1 = server1.stop_capture()

print "Server 1  - received %d packets"%packets_received_1
sched_connections_on_server = ezbox.get_server_sched_connections_stats(0)
print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server

error_stats = ezbox.get_error_stats()     
print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']


created_connections = 0
server_is_unavailable_error = 0
for src_port in src_port_list:   
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), 80, ip2int(client_object.data_ip) , src_port, 6)
    if connection != None:
        created_connections +=1
    else:
        server_is_unavailable_error +=1
connection=ezbox.get_connection(ip2int(first_service.virtual_ip), 80, ip2int(client_object.data_ip) , int(random_source_port.replace(' ',''),16), 6)
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

#     server1.compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
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
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src=client_object.hex_display_to_ip,
                         tcp_source_port = source_port,
                         tcp_dst_port = '00 50', # port 80
                         tcp_reset_flag = True,
                         packet_length=packet_size)
reset_packet_d.generate_packet()

source_port="%04x"%src_port_list[3]
source_port = source_port[0:2] + ' ' + source_port[2:4]
fin_packet_d_1 = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src=client_object.hex_display_to_ip,
                         tcp_source_port = source_port,
                         tcp_dst_port = '00 50', # port 80
                         tcp_fin_flag = True,
                         packet_length=packet_size)
fin_packet_d_1.generate_packet()   

source_port="%04x"%src_port_list[4]
source_port = source_port[0:2] + ' ' + source_port[2:4]
fin_packet_d_2 = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src=client_object.hex_display_to_ip,
                         tcp_source_port = source_port,
                         tcp_dst_port = '00 50', # port 80
                         tcp_fin_flag = True,
                         packet_length=packet_size)
fin_packet_d_2.generate_packet()   
    
pcap_to_send = create_pcap_file(packets_list=[reset_packet_d.packet,fin_packet_d_1.packet,fin_packet_d_2.packet], output_pcap_file_name='verification/testing/dp/temp_packet3_d.pcap')
  
time.sleep(2) 
server1.capture_packets_from_service(service=first_service)
    
    # send packet
time.sleep(5)
print "Send packets"
client_object.send_packet_to_nps(pcap_to_send)
  
time.sleep(5)
print "capture from server"
packets_received_1 = server1.stop_capture()

time.sleep(20)

print "Server 1  - received %d packets"%packets_received_1
sched_connections_on_server = ezbox.get_server_sched_connections_stats(0)
print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server

error_stats = ezbox.get_error_stats()     
print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']

created_connections = 0
server_is_unavailable_error = 0
for src_port in src_port_list:   
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), 80, ip2int(client_object.data_ip) , src_port, 6)
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

#     server1.compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
    print "PART3 D Passed"
else:
    print "PART3 D Failed"
    exit(1)

num_of_packets_e = len(packet_list_to_send)
print "PART 3 - E - send %d frames to create new connections "%(num_of_packets_e)
print "New connections should be created up to  u_thresh = %d "%u_thresh

pcap_to_send = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name='verification/testing/dp/temp_packet3_e.pcap')

time.sleep(2) 
server1.capture_packets_from_service(service=first_service)
   
    # send packet
time.sleep(5)
print "Send packets"
client_object.send_packet_to_nps(pcap_to_send)
  
time.sleep(5)
print "capture from server"
packets_received_1 = server1.stop_capture()
print "Server 1  - received %d packets"%packets_received_1

time.sleep(20)
sched_connections_on_server = ezbox.get_server_sched_connections_stats(0)
print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server
error_stats = ezbox.get_error_stats()     
print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']

created_connections = 0
server_is_unavailable_error = 0
for src_port in src_port_list:   
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), 80, ip2int(client_object.data_ip) , src_port, 6)
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

#     server1.compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
    print "PART3 E Passed"
else:
    print "PART3 E Failed"
    exit(1)


u_thresh = 7
l_thresh = 3
print "**************************************************************************"
print "PART 4 - modify server with u_thresh = %d higher than it was and check that new connections are created"%u_thresh

num_of_packets_4 = len(packet_list_to_send)

first_service.modify_server(server1, weight=1, u_thresh=u_thresh, l_thresh=l_thresh)

pcap_to_send = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name='verification/testing/dp/temp_packet4.pcap')

time.sleep(2) 
server1.capture_packets_from_service(service=first_service)
   
    # send packet
time.sleep(5)
print "Send packets"
client_object.send_packet_to_nps(pcap_to_send)
  
time.sleep(5)
print "capture from server"
packets_received_1 = server1.stop_capture()
 
print "Server 1  - received %d packets"%packets_received_1

time.sleep(20)
sched_connections_on_server = ezbox.get_server_sched_connections_stats(0)
print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server
error_stats = ezbox.get_error_stats()     
print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']

created_connections = 0
server_is_unavailable_error = 0
for src_port in src_port_list:   
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), 80, ip2int(client_object.data_ip) , src_port, 6)
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

#     server1.compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
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

first_service.modify_server(server1, weight=1, u_thresh=u_thresh, l_thresh=l_thresh)

pcap_to_send = create_pcap_file(packets_list=[fin_packet_d_2.packet], output_pcap_file_name='verification/testing/dp/temp_packet5_a.pcap')

time.sleep(2) 
server1.capture_packets_from_service(service=first_service)
   
    # send packet
time.sleep(5)
print "Send packets"
client_object.send_packet_to_nps(pcap_to_send)
  
time.sleep(5)
print "capture from server"
packets_received_1 = server1.stop_capture()
 
print "Server 1  - received %d packets"%packets_received_1
time.sleep(20)

sched_connections_on_server = ezbox.get_server_sched_connections_stats(0)
print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server
error_stats = ezbox.get_error_stats()     
print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']

created_connections = 0
server_is_unavailable_error = 0
for src_port in src_port_list:   
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), 80, ip2int(client_object.data_ip) , src_port, 6)
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

#     server1.compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
    print "PART5 A Passed"
else:
    print "PART5 A Failed"
    exit(1)

print "PART 5 B - try to create new connections - should fail - as OVERLOADED flag is still set"
num_of_packets_5_b = 1
pcap_to_send = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name='verification/testing/dp/temp_packet5_b.pcap')

time.sleep(2) 
server1.capture_packets_from_service(service=first_service)
   
    # send packet
time.sleep(5)
print "Send packets"
client_object.send_packet_to_nps(pcap_to_send)
  
time.sleep(5)
print "capture from server"
packets_received_1 = server1.stop_capture()
 
print "Server 1  - received %d packets"%packets_received_1

created_connections = 0
server_is_unavailable_error = 0
for src_port in src_port_list:   
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), 80, ip2int(client_object.data_ip) , src_port, 6)
    if connection != None:
        created_connections +=1
    else:
        server_is_unavailable_error +=1

time.sleep(20)
sched_connections_on_server = ezbox.get_server_sched_connections_stats(0)
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

#     server1.compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
    print "PART5 B Passed"
else:
    print "PART5 B Failed"
    exit(1)


print "PART 5 C - remove connection to be below sched_conns*3 > u_thresh*4"
num_of_packets_5_c = 1

pcap_to_send = create_pcap_file(packets_list=[fin_packet_d_1.packet], output_pcap_file_name='verification/testing/dp/temp_packet5_c.pcap')

time.sleep(2) 
server1.capture_packets_from_service(service=first_service)
   
    # send packet
time.sleep(5)
print "Send packets"
client_object.send_packet_to_nps(pcap_to_send)
  
time.sleep(5)
print "capture from server"
packets_received_1 = server1.stop_capture()
 
print "Server 1  - received %d packets"%packets_received_1
time.sleep(20)

sched_connections_on_server = ezbox.get_server_sched_connections_stats(0)
print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server
error_stats = ezbox.get_error_stats()     
print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']

created_connections = 0
server_is_unavailable_error = 0
for src_port in src_port_list:   
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), 80, ip2int(client_object.data_ip) , src_port, 6)
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

#     server1.compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
    print "PART5 C Passed"
else:
    print "PART5 C Failed"
    exit(1)

print "PART 5 D - try to create new connections - should PASS - as OVERLOADED flag is off"
pcap_to_send = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name='verification/testing/dp/temp_packet5_d.pcap')

time.sleep(2) 
server1.capture_packets_from_service(service=first_service)
   
    # send packet
time.sleep(5)
print "Send packets"
client_object.send_packet_to_nps(pcap_to_send)
  
time.sleep(5)
print "capture from server"
packets_received_1 = server1.stop_capture()
 
print "Server 1  - received %d packets"%packets_received_1

created_connections = 0
server_is_unavailable_error = 0
for src_port in src_port_list:   
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), 80, ip2int(client_object.data_ip) , src_port, 6)
    if connection != None:
        created_connections +=1
    else:
        server_is_unavailable_error +=1

time.sleep(20)
sched_connections_on_server = ezbox.get_server_sched_connections_stats(0)
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

#     server1.compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
    print "PART5 D Passed"
else:
    print "PART5 D Failed"
    exit(1)

print "**************************************************************************"
print "PART 6 - wait till all connections are aged - due to timeout"
time.sleep(1000)
server1.capture_packets_from_service(service=first_service)
     
time.sleep(5)
print "capture from server"
packets_received_1 = server1.stop_capture()
 
print "Server 1  - received %d packets"%packets_received_1
created_connections = 0
server_is_unavailable_error = 0
for src_port in src_port_list:   
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), 80, ip2int(client_object.data_ip) , src_port, 6)
    if connection != None:
        created_connections +=1
    else:
        server_is_unavailable_error +=1
        
time.sleep(20)
sched_connections_on_server = ezbox.get_server_sched_connections_stats(0)
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
    print "PART6 Passed"
else:
    print "PART6 Failed"
    exit(1)
