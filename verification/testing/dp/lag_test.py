#!/usr/bin/env python

import sys
sys.path.append("verification/testing")
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
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	config = fill_default_config(generic_main())
	
	dict = user_init(config['setup_num'])
	
	init_players(dict, config)
	
	server_list, ezbox, client_list, vip_list = convert_generic_init_to_user_format(dict)
	
	run_user_test(server_list, ezbox, client_list, vip_list)
	
	clean_players(dict, True, config['stop_ezbox'])
	
	print "\nTest Passed\n"
	

def run_user_test(server_list, ezbox, client_list, vip_list):
	port = '80'
	sched_algorithm = 'source_hash'
	# disable the ldirector - causes noise on the nps ports..
	ezbox.execute_command_on_host('/usr/sbin/ldirectord stop')
	ezbox.execute_command_on_host('service keepalived stop')


	ezbox.add_service(vip_list[0], port)
	
	for server in server_list:
		print "adding server %s to service %s" %(server.ip,server.vip)
		ezbox.add_server(server.vip, port, server.ip, port,server.weight)
# create packet

	data_packet1 = tcp_packet(mac_da=ezbox.setup['mac_address'],
	                         mac_sa=client_list[0].get_mac_adress(),
	                         ip_dst=ip_to_hex_display(vip_list[0]), #first_service.virtual_ip_hex_display,
	                         ip_src=ip_to_hex_display(client_list[0].ip),
	                         tcp_source_port = '00 00',
	                         tcp_dst_port = '00 50', # port 80
	                         tcp_reset_flag = False,
	                         tcp_fin_flag = False,
	                         packet_length=64)
	data_packet1.generate_packet()

	packets_to_send_list = [data_packet1.packet] * 500

	pcap_to_send = create_pcap_file(packets_list=packets_to_send_list, output_pcap_file_name='verification/testing/dp/pcap_files/temp_packet.pcap')

	port0 = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=0,if_engine=0,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
	port0 = int(port0.get_struct_dict()['counter_value'],16)
	port1 = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=0,if_engine=1,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
	port1 = int(port1.get_struct_dict()['counter_value'],16)
	port2 = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=1,if_engine=0,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
	port2 = int(port2.get_struct_dict()['counter_value'],16)
	port3 = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=1,if_engine=1,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
	port3 = int(port3.get_struct_dict()['counter_value'],16)

	# send 500 packets to different mac da and check the ports statistics , each port tx stats should be ~250 
	print "Send 500 packets with different mac da"
	
	mac_da = '52:54:00:c5:15:41'
	for i in range(500):
		sys.stdout.write("Sending packet num %d\r" % i)
		sys.stdout.flush()
		ezbox.execute_command_on_host('arp -s %s %s'%(server_list[0].ip, mac_da)) # change the arp entry to use different mac da for this service    
		client_list[0].send_packet_to_nps(data_packet1.pcap_file_name)
		mac_da = add2mac(mac_da, 1)
 


	port0_after = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=0,if_engine=0,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
	port0_after = int(port0_after.get_struct_dict()['counter_value'],16)
	port1_after = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=0,if_engine=1,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
	port1_after = int(port1_after.get_struct_dict()['counter_value'],16)
	port2_after = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=1,if_engine=0,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
	port2_after = int(port2_after.get_struct_dict()['counter_value'],16)
	port3_after = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=1,if_engine=1,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
	port3_after = int(port3_after.get_struct_dict()['counter_value'],16)
	
	if port0_after-port0 < 125 * 0.85 or port0_after-port0 > 125 * 1.15:
	    print "ERROR, lag deviation is wrong"
	    print "port 0 after: ", port0_after
	    print "port 0 before: ", port0
	    ezbox.execute_command_on_host('arp -d %s'%server_list[0].ip) # delete the static arp
	    exit(1)
	    
	if port1_after - port1 < 125 * 0.85 or port1_after-port1 > 125 * 1.15:
	    print "ERROR, lag deviation is wrong"
	    print "port 1 after: ", port1_after
	    print "port 1 before: ", port1
	    ezbox.execute_command_on_host('arp -d %s'%server_list[0].ip) # delete the static arp
	    exit(1)
	
	if port2_after-port2 < 125 * 0.85 or port2_after-port2 > 125 * 1.15:
	    print "ERROR, lag deviation is wrong"
	    print "port 2 after: ", port2_after
	    print "port 2 before: ", port2
	    ezbox.execute_command_on_host('arp -d %s'%server_list[0].ip) # delete the static arp
	    exit(1)
	
	if port3_after-port3 < 125 * 0.85 or port3_after-port3 > 125 * 1.15:
	    print "ERROR, lag deviation is wrong"
	    print "port 3 after: ", port3_after
	    print "port 3 before: ", port3
	    ezbox.execute_command_on_host('arp -d %s'%server_list[0].ip) # delete the static arp
	    exit(1)
	print "All packets were distributed equally to all 4 ports\n"
    

	# using mac da (52:54:00:c5:15:42) and checking port 0,0 
	print "Send packets to port 0,0"
	before_result = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=0,if_engine=0,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
	before_result = int(before_result.get_struct_dict()['counter_value'],16)
	ezbox.execute_command_on_host('arp -s %s 52:54:00:c5:15:42'%server_list[0].ip) # port 0,0
	time.sleep(1)
	client_list[0].send_packet_to_nps(pcap_to_send)
	time.sleep(2)
	after_result = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=0,if_engine=0,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
	after_result = int(after_result.get_struct_dict()['counter_value'],16)
	if after_result-before_result < 300:
	    print "ERROR, lag port 0,0 wasnt used\n"
	    ezbox.execute_command_on_host('arp -d %s'%server_list[0].ip) # delete static arp
	    exit(1)
	print "Packet received on port 0,0"
     
	# using mac da (52:54:00:c5:15:40) and checking port 0,1 
	print "Send packets to port 0,1"
	before_result = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=0,if_engine=1,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
	before_result = int(before_result.get_struct_dict()['counter_value'],16)
	ezbox.execute_command_on_host('arp -s %s 52:54:00:c5:15:40'%server_list[0].ip) # port 0,1
	time.sleep(1)
	client_list[0].send_packet_to_nps(pcap_to_send)
	time.sleep(1)
	after_result = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=0,if_engine=1,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
	after_result = int(after_result.get_struct_dict()['counter_value'],16)
	if after_result-before_result < 300:
	    print "ERROR, lag port 0,1 wasnt used\n"
	    ezbox.execute_command_on_host('arp -d %s'%server_list[0].ip) # delete static arp
	    exit(1)
	print "Packet received on port 0,1"   
   
	# using mac da (52:54:00:c5:15:43) and checking port 1,0
	print "Send packets to port 1,0"
	before_result = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=1,if_engine=0,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
	before_result = int(before_result.get_struct_dict()['counter_value'],16)
	ezbox.execute_command_on_host('arp -s %s 52:54:00:c5:15:43'%server_list[0].ip) # port 1,0
	time.sleep(1)
	client_list[0].send_packet_to_nps(pcap_to_send)
	time.sleep(1)
	after_result = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=1,if_engine=0,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
	after_result = int(after_result.get_struct_dict()['counter_value'],16)
	if after_result-before_result < 300:
	    print "ERROR, lag port 1,0 wasnt used\n"
	    ezbox.execute_command_on_host('arp -d %s'%server_list[0].ip) # delete static arp
	    exit(1)
	print "Packet received on port 1,0"
     
	# using mac da (52:54:00:c5:15:41) and checking port 1,1
	print "Send packets to port 1,1"
	before_result = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=1,if_engine=1,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
	before_result = int(before_result.get_struct_dict()['counter_value'],16)
	ezbox.execute_command_on_host('arp -s %s 52:54:00:c5:15:41'%server_list[0].ip) # port 1,1
	time.sleep(1)
	client_list[0].send_packet_to_nps(pcap_to_send)
	time.sleep(1)
	after_result = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=1,if_engine=1,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__('TX_FRM'))
	after_result = int(after_result.get_struct_dict()['counter_value'],16)
	if after_result-before_result < 300:
	    print "ERROR, lag port 0,1 wasnt used\n"
	    ezbox.execute_command_on_host('arp -d %s'%server_list[0].ip) # delete static arp
	    exit(1)
	print "Packet received on port 1,1" 

	def stats():
	    for x in [0,1]:
	        for y in [0,1]:
	            print "### PORT %d,%d ###" % (x,y)
	            for c in ['TX_FRM']:
	                result = ezbox.cpe.cp.interface.get_eth_stat_counter(channel_id=0,side=x,if_engine=y,eth_if_type='10GE',if_number=0,counter_id=ezbox.cpe.cp.interface.EthStatCounterId.__getattribute__(c))
	                print "%s:: %s" % (c, int(result.get_struct_dict()['counter_value'],16)) 
	stats()
	
	ezbox.execute_command_on_host('arp -d %s'%server_list[0].ip) # delete the static arp

main()
