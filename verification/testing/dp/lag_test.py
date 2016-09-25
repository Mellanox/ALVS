#!/usr/bin/env python

import sys
sys.path.append("verification/testing")
from test_infra import * 
import random

ezbox,args = init_test(test_arguments=sys.argv)

# disable the ldirector - causes noise on the nps ports..
ezbox.execute_command_on_host('/usr/sbin/ldirectord stop')
ezbox.execute_command_on_host('service keepalived stop')

# each setup can use differen VMs
ip_list = get_setup_list(args['setup_num'])

# each setup can use different the virtual ip 
virtual_ip_address_1 = get_setup_vip(args['setup_num'], 0)

# create servers
server1 = real_server(management_ip=ip_list[0]['hostname'], data_ip=ip_list[0]['ip'])
 
# create client
client_object = client(management_ip=ip_list[1]['hostname'], data_ip=ip_list[1]['ip'])

# create services on ezbox
first_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_1, port='80', schedule_algorithm = 'source_hash')
first_service.add_server(server1, weight='1')

# create packet
data_packet1 = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src=client_object.hex_display_to_ip,
                         tcp_source_port = '00 00',
                         tcp_dst_port = '00 50', # port 80
                         tcp_reset_flag = False,
                         tcp_fin_flag = False,
                         packet_length=64)
data_packet1.generate_packet()

packets_to_send_list = [data_packet1.packet] * 500

pcap_to_send = create_pcap_file(packets_list=packets_to_send_list, output_pcap_file_name='verification/testing/dp/pcap_files/temp_packet.pcap')

# send 500 packets to different mac da and check the ports statistics , each port tx stats should be ~250 
print "Send 500 packets with different mac da"

mac_da = '52:54:00:c5:15:41'
for i in range(500):
	sys.stdout.write("Sending packet num %d\r" % i)
	sys.stdout.flush()
	ezbox.execute_command_on_host('arp -s %s %s'%(server1.data_ip, mac_da)) # change the arp entry to use different mac da for this service    
	client_object.send_packet_to_nps(data_packet1.pcap_file_name)
	mac_da = add2mac(mac_da, 1)

port0 = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=0,if_engine=0,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
port0 = int(port0.get_struct_dict()['counter_value'],16)
port1 = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=0,if_engine=1,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
port1 = int(port1.get_struct_dict()['counter_value'],16)
port2 = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=1,if_engine=0,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
port2 = int(port2.get_struct_dict()['counter_value'],16)
port3 = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=1,if_engine=1,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
port3 = int(port3.get_struct_dict()['counter_value'],16)
 
if port0 < 125 * 0.90 or port0 > 125 * 1.1:
    print "ERROR, lag deviation is wrong"
    ezbox.execute_command_on_host('arp -d %s'%server1.data_ip) # delete the static arp
    exit(1)
    
if port1 < 125 * 0.9 or port0 > 125 * 1.1:
    print "ERROR, lag deviation is wrong"
    ezbox.execute_command_on_host('arp -d %s'%server1.data_ip) # delete the static arp
    exit(1)

if port2 < 125 * 0.9 or port0 > 125 * 1.1:
    print "ERROR, lag deviation is wrong"
    ezbox.execute_command_on_host('arp -d %s'%server1.data_ip) # delete the static arp
    exit(1)

if port3 < 125 * 0.9 or port0 > 125 * 1.1:
    print "ERROR, lag deviation is wrong"
    ezbox.execute_command_on_host('arp -d %s'%server1.data_ip) # delete the static arp
    exit(1)
print "All packets were distributed equally to all 4 ports\n"
    
# using mac da (52:54:00:c5:15:42) and checking port 0,0 
print "Send packets to port 0,0"
before_result = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=0,if_engine=0,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
before_result = int(before_result.get_struct_dict()['counter_value'],16)
ezbox.execute_command_on_host('arp -s %s 52:54:00:c5:15:42'%server1.data_ip) # port 0,0
time.sleep(1)
# send 500 packets
client_object.send_packet_to_nps(pcap_to_send)
time.sleep(2)
after_result = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=0,if_engine=0,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
after_result = int(after_result.get_struct_dict()['counter_value'],16)
if after_result-before_result < 500:
    print "ERROR, lag port 0,0 wasnt used\n"
    ezbox.execute_command_on_host('arp -d %s'%server1.data_ip) # delete static arp
    exit(1)
print "Packet received on port 0,0"
     
# using mac da (52:54:00:c5:15:40) and checking port 0,1 
print "Send packets to port 0,1"
before_result = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=0,if_engine=1,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
before_result = int(before_result.get_struct_dict()['counter_value'],16)
ezbox.execute_command_on_host('arp -s %s 52:54:00:c5:15:40'%server1.data_ip) # port 0,1
time.sleep(1)
# send 500 packets
client_object.send_packet_to_nps(pcap_to_send)
time.sleep(1)
after_result = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=0,if_engine=1,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
after_result = int(after_result.get_struct_dict()['counter_value'],16)
if after_result-before_result < 500:
    print "ERROR, lag port 0,1 wasnt used\n"
    ezbox.execute_command_on_host('arp -d %s'%server1.data_ip) # delete static arp
    exit(1)
print "Packet received on port 0,1"   
   
# using mac da (52:54:00:c5:15:43) and checking port 1,0
print "Send packets to port 1,0"
before_result = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=1,if_engine=0,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
before_result = int(before_result.get_struct_dict()['counter_value'],16)
ezbox.execute_command_on_host('arp -s %s 52:54:00:c5:15:43'%server1.data_ip) # port 1,0
time.sleep(1)
# send 500 packets
client_object.send_packet_to_nps(pcap_to_send)
time.sleep(1)
after_result = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=1,if_engine=0,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
after_result = int(after_result.get_struct_dict()['counter_value'],16)
if after_result-before_result < 500:
    print "ERROR, lag port 1,0 wasnt used\n"
    ezbox.execute_command_on_host('arp -d %s'%server1.data_ip) # delete static arp
    exit(1)
print "Packet received on port 1,0"
     
# using mac da (52:54:00:c5:15:41) and checking port 1,1
print "Send packets to port 1,1"
before_result = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=1,if_engine=1,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
before_result = int(before_result.get_struct_dict()['counter_value'],16)
ezbox.execute_command_on_host('arp -s %s 52:54:00:c5:15:41'%server1.data_ip) # port 1,1
time.sleep(1)
# send 500 packets
client_object.send_packet_to_nps(pcap_to_send)
time.sleep(1)
after_result = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=1,if_engine=1,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
after_result = int(after_result.get_struct_dict()['counter_value'],16)
if after_result-before_result < 500:
    print "ERROR, lag port 0,1 wasnt used\n"
    ezbox.execute_command_on_host('arp -d %s'%server1.data_ip) # delete static arp
    exit(1)
print "Packet received on port 1,1" 

def stats():
    for x in [0,1]:
        for y in [0,1]:
            print "### PORT %d,%d ###" % (x,y)
#             for c in ['RX_FRM','RX_FRM_UNICAST','RX_FRM_MULTICAST','RX_FRM_BROADCAST','RX_FRM_CONTROL','RX_FRM_FCS_ERR','RX_FRM_LENGTH_ERR','RX_FRM_CODE_ERR','TX_FRM','TX_FRM_PAUSE']:
            for c in ['TX_FRM']:
                    result = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=x,if_engine=y,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__(c))
                    print "%s:: %s" % (c, int(result.get_struct_dict()['counter_value'],16)) 
stats()

ezbox.execute_command_on_host('arp -d %s'%server1.data_ip) # delete the static arp
print "\nTest Passed\n"
