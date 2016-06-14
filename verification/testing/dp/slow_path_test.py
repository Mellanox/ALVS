#!/usr/bin/env python

import sys
sys.path.append("../")
from test_infra import *

args = read_test_arg(sys.argv)    

log_file = "dp_packet_to_host_test.log"
if 'log_file' in args:
    log_file = args['log_file']
init_logging(log_file)

scenarios_to_run = args['scenarios']
scenarios_to_run = [3]

ezbox = ezbox_host(management_ip=args['host_ip'], 
                   username='root', 
                   password='ezchip',  
                   nps_ip=args['nps_ip'],
                   data_ip = '10.7.124.100',
                   cp_app_bin=args['cp_bin'], 
                   dp_app_bin=args['dp_bin'])

if args['hard_reset']:
    ezbox.reset_ezbox(args['ezbox'])

ezbox.connect()
# ezbox.terminate_cp_app()
# ezbox.reset_chip()
# ezbox.copy_cp_bin_to_host()
# ezbox.run_cp_app()
# ezbox.copy_and_run_dp_bin()
# 

virtual_ip_address = '192.168.1.1'

server1 = real_server(management_ip='10.137.107.7', data_ip='192.168.0.7', username='root', password = '3tango')
server1.connect()
server1.catch_received_packets('tcp')
server2 = real_server(management_ip='10.137.107.8', data_ip='192.168.0.8', username='root', password = '3tango')
server2.connect()
server2.catch_received_packets('tcp')

# create service on ezbox
new_service = service(ezbox, virtual_ip='virtual_ip_address', '80', schedule_algorithm = 'source_hash')
new_service.add_server(server1, '1')
new_service.add_server(server2, '1')
 
client1 = client(management_ip='10.137.107.6', data_ip='192.168.0.6', username='root', password='3tango')
client.connect()

# create packet
packet1 = tcp_packet(mac_da='01 02 03 04 05 06',
                     mac_sa='07 08 09 0a 0b 0c',
                     ip_dst='c0 a8 00 01',
                     ip_src='c0 a8 01 01',
                     tcp_source_port = '00 00',
                     tcp_fin_flag=True,
                     tcp_reset_flag=True,
                     packet_length=70)
packet1.generate_packet()

client.send_packet_to_nps(packet1.packet)

# data = struct.pack("%dB" % len(data), *data)
 
# print ' '.join('%02X' % ord(x) for x in data)
# print "Checksum: 0x%04x" % checksum(data)
    
    
    

# client.send_packet_to_nps(update_mac)





