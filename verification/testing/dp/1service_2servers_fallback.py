import sys
sys.path.append("verification/testing")
from test_infra import * 
import random

args = read_test_arg(sys.argv)    

log_file = "1service_2servers_fallback.log"
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
server2 = real_server(management_ip=ip_list[1]['hostname'], data_ip=ip_list[1]['ip'])

# create services on ezbox
u_thresh = 4
l_thresh = 2
first_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_1, port='80', schedule_algorithm = 'source_hash_fallback')
first_service.add_server(server1, weight='3', u_thresh = 0, l_thresh = 0)
first_service.add_server(server2, weight='253', u_thresh = u_thresh, l_thresh = l_thresh)

# create client
client_object = client(management_ip=ip_list[3]['hostname'], data_ip=ip_list[3]['ip'])

print "**************************************************************************"
print "PART 1 - Check that all connections are created - part on first server and part on server2"
print "One of the servers will be overloaded - so another server should be found"

    
num_of_packets_1 = 7
src_port_list_part_1 = []
src_port_list_part_2 = []
packet_list_to_send = []
for i in range(num_of_packets_1):
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
    
    src_port_list_part_1.append(int(random_source_port.replace(' ',''),16))
     
pcap_to_send_1 = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name='verification/testing/dp/1serv_2serv_fallback_1.pcap')

time.sleep(5) 
server1.capture_packets_from_service(service=first_service)
server2.capture_packets_from_service(service=first_service)

    # send packet
time.sleep(5)
print "Send packets"
client_object.send_packet_to_nps(pcap_to_send_1)
time.sleep(10)

    # send packet
print "capture from server"
packets_received_1 = server1.stop_capture()
print "Server 1  - received %d packets"%packets_received_1
packets_received_2 = server2.stop_capture()
print "Server 2  - received %d packets"%packets_received_2

sched_connections_on_server_1 = ezbox.get_server_connections_total_stats(0)
print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server_1
sched_connections_on_server_2 = ezbox.get_server_connections_total_stats(1)
print "Server 2  - number of scheduled connections %s on server"%sched_connections_on_server_2

error_stats = ezbox.get_error_stats()     
print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']

created_connections = 0
server_is_unavailable_error = 0

for src_port in src_port_list_part_1:   
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), 80, ip2int(client_object.data_ip) , src_port, 6)
    if connection != None:
        created_connections +=1
    else:
        server_is_unavailable_error +=1

error_stats = ezbox.get_error_stats()     
print "created_connections %d"%created_connections
print "uncreated_connections %d"%server_is_unavailable_error
expected_packets_received_2 = u_thresh
expected_packets_received_1 = num_of_packets_1 - expected_packets_received_2
expected_packets_receieved = num_of_packets_1
expected_num_of_errors = 0
if (packets_received_1 == expected_packets_received_1 and
    packets_received_2 == expected_packets_received_2 and
    packets_received_1 + packets_received_2 == expected_packets_receieved and 
    sched_connections_on_server_1 + sched_connections_on_server_2 == expected_packets_receieved and 
    error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE'] == expected_num_of_errors and
    (created_connections == expected_packets_receieved) and
   (server_is_unavailable_error == expected_num_of_errors)):

#     server1.compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
    print "PART1 Passed"
else:
    print "PART1 Faild"
    exit(1)

print "**************************************************************************"
print "PART 2 - send more frames to create new connections" 
print "Server 1 is overloaded, server 2 after modify u_thresh = 4 1 additional connection can be added"
num_of_packets_2 = 7
u_thresh = 4
l_thresh = 2
first_service.modify_server(server1, weight=3, u_thresh=u_thresh, l_thresh=l_thresh)
first_service.modify_server(server2, weight=5, u_thresh=u_thresh, l_thresh=l_thresh)

for i in range(num_of_packets_2):
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
    
    src_port_list_part_2.append(int(random_source_port.replace(' ',''),16))
     
pcap_to_send_2 = create_pcap_file(packets_list=packet_list_to_send, output_pcap_file_name='verification/testing/dp/1serv_2serv_fallback_2.pcap')

time.sleep(5) 
server1.capture_packets_from_service(service=first_service)
server2.capture_packets_from_service(service=first_service)

    # send packet
time.sleep(5)
print "Send packets"
client_object.send_packet_to_nps(pcap_to_send_2)

time.sleep(10)
    # send packet
print "capture from server"
packets_received_1 = server1.stop_capture()
print "Server 1  - received %d packets"%packets_received_1
packets_received_2 = server2.stop_capture()
print "Server 2  - received %d packets"%packets_received_2

sched_connections_on_server_1 = ezbox.get_server_connections_total_stats(0)
print "Server 1  - number of scheduled connections %s on server"%sched_connections_on_server_1
sched_connections_on_server_2 = ezbox.get_server_connections_total_stats(1)
print "Server 2  - number of scheduled connections %s on server"%sched_connections_on_server_2

error_stats = ezbox.get_error_stats()     
print "ALVS_ERROR_SERVER_IS_UNAVAILABLE %s"%error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE']

created_connections = 0
server_is_unavailable_error = 0

for src_port in src_port_list_part_1:   
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), 80, ip2int(client_object.data_ip) , src_port, 6)
    if connection != None:
        created_connections +=1
    else:
        server_is_unavailable_error +=1

for src_port in src_port_list_part_2:   
    connection=ezbox.get_connection(ip2int(first_service.virtual_ip), 80, ip2int(client_object.data_ip) , src_port, 6)
    if connection != None:
        created_connections +=1
    else:
        server_is_unavailable_error +=1

error_stats = ezbox.get_error_stats()     
print "created_connections %d"%created_connections
print "uncreated_connections %d"%server_is_unavailable_error
num_of_packets = num_of_packets_1 + num_of_packets_2
expected_packets_received_1 = min(u_thresh, num_of_packets)
expected_packets_received_2 = min(u_thresh, num_of_packets)        #sched_conns from previouse part is 3, one new connection can be created upto u_thresh
expected_num_of_errors = num_of_packets - expected_packets_received_1 - expected_packets_received_2
expected_sched_connections = 8 #MMIN (u_thresh on server1 + u_thresh on server2, num_of_packets)
if (packets_received_1 == expected_packets_received_1 and
    packets_received_2 == expected_packets_received_2 and
    sched_connections_on_server_1 + sched_connections_on_server_2 == expected_sched_connections and 
    error_stats['ALVS_ERROR_SERVER_IS_UNAVAILABLE'] == expected_num_of_errors and
    (created_connections == expected_sched_connections) and
   (server_is_unavailable_error == expected_num_of_errors)):

#     server1.compare_received_packets_to_pcap_file(pcap_file='p1.pcap', pcap_file_on_server='/tmp/server_dump.pcap')
    print "PART2 Passed"
else:
    print "PART2 Failed"
    exit(1)

