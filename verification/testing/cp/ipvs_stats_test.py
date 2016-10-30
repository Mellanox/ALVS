#!/usr/bin/env python

import sys
sys.path.append("verification/testing/")
from test_infra import *
from time import sleep

from common_infra import *
from e2e_infra import *

server_count   = 4
client_count   = 1
service_count  = 3

def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
	dict = generic_init(setup_num, service_count, server_count, client_count)
	
	w = 1
	for i,s in enumerate(dict['server_list']):
		if i < 2:
			s.vip = dict['vip_list'][1]
		else:
			s.vip = dict['vip_list'][2]
			
		s.weight = w
	
	return dict

def main():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	config = fill_default_config(generic_main())
	
	# in this test statistic is mendatory so need to start ezbox
	config['start_ezbox'] = True
	config['stats'] = True
	config['use_director'] = False
	print 'start_ezbox and stats was changed to True'
	
	dict = user_init(config['setup_num'])
	
	init_players(dict, config)
	
	server_list, ezbox, client_list, vip_list = convert_generic_init_to_user_format(dict)
	
	run_user_test(server_list, ezbox, client_list, vip_list)
	
	clean_players(dict, True, config['stop_ezbox'])
	
	print "\nTest Passed\n"

def compare_service_stats(service_stats, in_packets, in_bytes, out_packets, out_bytes, conn_sched):
	if service_stats['IN_PACKETS'] != in_packets:
		print "ERROR, wrong in packet statistics"
		exit(1)
	
	if service_stats['IN_BYTES'] != in_bytes:
		print "ERROR, wrong in bytes statistics"
		exit(1)
		
	if service_stats['OUT_PACKETS'] != out_packets:
		print "ERROR, wrong out packet statistics"
		exit(1)
	
	if service_stats['OUT_BYTES'] != out_bytes:
		print "ERROR, wrong out bytes statistics"
		exit(1)
		
	if service_stats['CONN_SCHED'] != conn_sched:
		print "ERROR, wrong conn sched statistics"
		exit(1)

def compare_server_stats(server_stats, in_packets, in_bytes, out_packets, out_bytes, conn_sched, conn_total, active, inactive):
	
	if server_stats['IN_PACKETS'] != in_packets:
		print "ERROR, wrong in packet statistics"
		exit(1)
	
	if server_stats['IN_BYTES'] != in_bytes:
		print "ERROR, wrong in byte statistics"
		exit(1)
		
	if server_stats['OUT_PACKETS'] != out_packets:
		print "ERROR, wrong out packet statistics"
		exit(1)
	
	if server_stats['OUT_BYTES'] != out_bytes:
		print "ERROR, wrong out byte statistics"
		exit(1)
		
	if server_stats['CONN_SCHED'] != conn_sched:
		print "ERROR, wrong conn sched statistics"
		exit(1)
	
	if server_stats['CONN_TOTAL'] != conn_total:
		print "ERROR, wrong conn total statistics"
		exit(1)
	
	if server_stats['ACTIVE_CONN'] != active:
		print "ERROR, wrong active conn statistics"
		exit(1)
		
	if server_stats['INACTIVE_CONN'] != inactive:
		print "ERROR, wrong inactive conn statistics"
		exit(1)
		
def run_user_test(server_list, ezbox, client_list, vip_list):
	port = '80'
	sched_algorithm = 'source_hash_with_source_port'
	
	ezbox.add_service(vip_list[1], port)
	ezbox.add_service(vip_list[2], port)
	
	for i,server in enumerate(server_list):
		print "adding server %s to service %s" %(server.ip,server.vip)
		if i < 2:
			ezbox.add_server(vip_list[1], port, server.ip, port, server.weight)
		else:
			ezbox.add_server(vip_list[2], port, server.ip, port, server.weight)
			
	# create packet
	data_packet_to_first_service1 = tcp_packet(mac_da=ezbox.setup['mac_address'],
	                         mac_sa=client_list[0].get_mac_adress(),
	                         ip_dst=ip_to_hex_display(vip_list[1]),
	                         ip_src='192.168.0.100',
	                         tcp_source_port = '00 00',
	                         tcp_dst_port = '00 50', # port 80
	                         tcp_reset_flag = False,
	                         tcp_fin_flag = False,
	                         packet_length=128)
	data_packet_to_first_service1.generate_packet()
	
	data_packet_to_first_service2 = tcp_packet(mac_da=ezbox.setup['mac_address'],
	                         mac_sa=client_list[0].get_mac_adress(),
	                         ip_dst=ip_to_hex_display(vip_list[1]),
	                         ip_src='192.168.0.100',
	                         tcp_source_port = '00 03',
	                         tcp_dst_port = '00 50', # port 80
	                         tcp_reset_flag = False,
	                         tcp_fin_flag = False,
	                         packet_length=222)
	data_packet_to_first_service2.generate_packet()
	
	data_packet_to_second_service1 = tcp_packet(mac_da=ezbox.setup['mac_address'],
	                         mac_sa=client_list[0].get_mac_adress(),
	                         ip_dst=ip_to_hex_display(vip_list[2]),
	                         ip_src='192.168.0.100',
	                         tcp_source_port = '00 00',
	                         tcp_dst_port = '00 50', # port 80
	                         tcp_reset_flag = False,
	                         tcp_fin_flag = False,
	                         packet_length=111)
	data_packet_to_second_service1.generate_packet()
	
	data_packet_to_second_service2 = tcp_packet(mac_da=ezbox.setup['mac_address'],
	                         mac_sa=client_list[0].get_mac_adress(),
	                         ip_dst=ip_to_hex_display(vip_list[2]),
	                         ip_src='192.168.0.100',
	                         tcp_source_port = '00 03',
	                         tcp_dst_port = '00 50', # port 80
	                         tcp_reset_flag = False,
	                         tcp_fin_flag = False,
	                         packet_length=333)
	data_packet_to_second_service2.generate_packet()
	
	reset_packet_first_service1 = tcp_packet(mac_da=ezbox.setup['mac_address'],
	                         mac_sa=client_list[0].get_mac_adress(),
	                         ip_dst=ip_to_hex_display(vip_list[1]),
	                         ip_src='192.168.0.100',
	                         tcp_source_port = '00 00',
	                         tcp_dst_port = '00 50', # port 80
	                         tcp_reset_flag = True,
	                         tcp_fin_flag = False,
	                         packet_length=111)
	reset_packet_first_service1.generate_packet()
	
	reset_packet_first_service2 = tcp_packet(mac_da=ezbox.setup['mac_address'],
	                         mac_sa=client_list[0].get_mac_adress(),
	                         ip_dst=ip_to_hex_display(vip_list[1]),
	                         ip_src='192.168.0.100',
	                         tcp_source_port = '00 03',
	                         tcp_dst_port = '00 50', # port 80
	                         tcp_reset_flag = True,
	                         tcp_fin_flag = False,
	                         packet_length=111)
	reset_packet_first_service2.generate_packet()
	
	reset_packet_second_service1 = tcp_packet(mac_da=ezbox.setup['mac_address'],
	                         mac_sa=client_list[0].get_mac_adress(),
	                         ip_dst=ip_to_hex_display(vip_list[2]),
	                         ip_src='192.168.0.100',
	                         tcp_source_port = '00 00',
	                         tcp_dst_port = '00 50', # port 80
	                         tcp_reset_flag = True,
	                         tcp_fin_flag = False,
	                         packet_length=111)
	reset_packet_second_service1.generate_packet()
	
	reset_packet_second_service2 = tcp_packet(mac_da=ezbox.setup['mac_address'],
	                         mac_sa=client_list[0].get_mac_adress(),
	                         ip_dst=ip_to_hex_display(vip_list[2]),
	                         ip_src='192.168.0.100',
	                         tcp_source_port = '00 03',
	                         tcp_dst_port = '00 50', # port 80
	                         tcp_reset_flag = True,
	                         tcp_fin_flag = False,
	                         packet_length=111)
	reset_packet_second_service2.generate_packet()
	
	fin_packet_to_first_service = tcp_packet(mac_da=ezbox.setup['mac_address'],
	                         mac_sa=client_list[0].get_mac_adress(),
	                         ip_dst=ip_to_hex_display(vip_list[1]),
	                         ip_src='192.168.0.100',
	                         tcp_source_port = '00 03',
	                         tcp_dst_port = '00 50', # port 80
	                         tcp_fin_flag = True,
	                         packet_length=128)
	fin_packet_to_first_service.generate_packet()
	
	print "Sending several packets"
	client_list[0].send_packet_to_nps(data_packet_to_first_service1.pcap_file_name)
	client_list[0].send_packet_to_nps(data_packet_to_first_service2.pcap_file_name)
	client_list[0].send_packet_to_nps(data_packet_to_second_service1.pcap_file_name)
	client_list[0].send_packet_to_nps(data_packet_to_second_service1.pcap_file_name)
	client_list[0].send_packet_to_nps(data_packet_to_second_service2.pcap_file_name)
	ezbox.get_ipvs_stats();
	stats_results = ezbox.read_stats_on_syslog()
	
	# service stats
	print "Check service syslog statistics prints"
	compare_service_stats(service_stats=stats_results['service_stats'][vip_list[1]], 
						 in_packets=2, 
						 in_bytes=data_packet_to_first_service1.packet_length + data_packet_to_first_service2.packet_length, 
						 out_packets=0, 
						 out_bytes=0, 
						 conn_sched=2)
	
	compare_service_stats(service_stats=stats_results['service_stats'][vip_list[2]], 
						 in_packets=3, 
						 in_bytes=data_packet_to_second_service1.packet_length*2 + data_packet_to_second_service2.packet_length, 
						 out_packets=0, 
						 out_bytes=0, 
						 conn_sched=2)
	
	# server stats
	print "Check servers syslog statistics prints"
	compare_server_stats(server_stats=stats_results['server_stats'][vip_list[1]+':80'][server_list[0].ip+':80'], 
						in_packets=1, 
						in_bytes=data_packet_to_first_service1.packet_length, 
						out_packets=0, 
						out_bytes=0, 
						conn_sched=1, 
						conn_total=1, 
						active=1, 
						inactive=0)
	
	compare_server_stats(server_stats=stats_results['server_stats'][vip_list[1]+':80'][server_list[1].ip+':80'], 
						in_packets=1, 
						in_bytes=data_packet_to_first_service2.packet_length, 
						out_packets=0, 
						out_bytes=0, 
						conn_sched=1, 
						conn_total=1, 
						active=1, 
						inactive=0)
	
	compare_server_stats(server_stats=stats_results['server_stats'][vip_list[2]+':80'][server_list[2].ip+':80'], 
						in_packets=2, 
						in_bytes=data_packet_to_second_service1.packet_length*2, 
						out_packets=0, 
						out_bytes=0, 
						conn_sched=1, 
						conn_total=1, 
						active=1, 
						inactive=0)
	
	compare_server_stats(server_stats=stats_results['server_stats'][vip_list[2]+':80'][server_list[3].ip+':80'], 
						in_packets=1, 
						in_bytes=data_packet_to_second_service2.packet_length, 
						out_packets=0, 
						out_bytes=0, 
						conn_sched=1, 
						conn_total=1, 
						active=1, 
						inactive=0)
	
	# send fin packet and check inactive connections
	print "Send fin packet"
	client_list[0].send_packet_to_nps(fin_packet_to_first_service.pcap_file_name)
	ezbox.get_ipvs_stats();
	stats_results = ezbox.read_stats_on_syslog()
	
	print "Check Service Statistics"
	compare_service_stats(service_stats=stats_results['service_stats'][vip_list[1]], 
						 in_packets=3, 
						 in_bytes=data_packet_to_first_service1.packet_length + data_packet_to_first_service2.packet_length + fin_packet_to_first_service.packet_length, 
						 out_packets=0, 
						 out_bytes=0, 
						 conn_sched=2)
	
	print "Check Server Statistics"
	compare_server_stats(server_stats=stats_results['server_stats'][vip_list[1]+':80'][server_list[1].ip+':80'], 
						in_packets=2, 
						in_bytes=data_packet_to_first_service2.packet_length + fin_packet_to_first_service.packet_length, 
						out_packets=0, 
						out_bytes=0, 
						conn_sched=1, 
						conn_total=1, 
						active=0, 
						inactive=1)
	
	time.sleep(40)
	ezbox.get_ipvs_stats();
	stats_results = ezbox.read_stats_on_syslog()
	print server_list[1]
	print vip_list[1]
	print stats_results
	compare_server_stats(server_stats=stats_results['server_stats'][vip_list[1]+':80'][server_list[1].ip+':80'], 
						in_packets=2, 
						in_bytes=data_packet_to_first_service2.packet_length + fin_packet_to_first_service.packet_length, 
						out_packets=0, 
						out_bytes=0, 
						conn_sched=1, 
						conn_total=0, 
						active=0, 
						inactive=0)
	
	
	print "Zero All Statistics"
	ezbox.zero_all_ipvs_stats();
	ezbox.get_ipvs_stats();
	stats_results = ezbox.read_stats_on_syslog()
	print "Check service syslog statistics prints"
	compare_service_stats(service_stats=stats_results['service_stats'][vip_list[1]], 
						 in_packets=0, 
						 in_bytes=0, 
						 out_packets=0, 
						 out_bytes=0, 
						 conn_sched=0)
	
	compare_service_stats(service_stats=stats_results['service_stats'][vip_list[2]], 
						 in_packets=0, 
						 in_bytes=0, 
						 out_packets=0, 
						 out_bytes=0, 
						 conn_sched=0)
	
	# server stats
	print "Check servers syslog statistics prints"
	compare_server_stats(server_stats=stats_results['server_stats'][vip_list[1]+':80'][server_list[0].ip+':80'], 
						in_packets=0, 
						in_bytes=0, 
						out_packets=0, 
						out_bytes=0, 
						conn_sched=0, 
						conn_total=1, 
						active=1, 
						inactive=0)
	
	compare_server_stats(server_stats=stats_results['server_stats'][vip_list[1]+':80'][server_list[1].ip+':80'], 
						in_packets=0, 
						in_bytes=0, 
						out_packets=0, 
						out_bytes=0, 
						conn_sched=0, 
						conn_total=0, 
						active=0, 
						inactive=0)
	
	compare_server_stats(server_stats=stats_results['server_stats'][vip_list[2]+':80'][server_list[2].ip+':80'], 
						in_packets=0, 
						in_bytes=0, 
						out_packets=0, 
						out_bytes=0, 
						conn_sched=0, 
						conn_total=1, 
						active=1, 
						inactive=0)
	
	compare_server_stats(server_stats=stats_results['server_stats'][vip_list[2]+':80'][server_list[3].ip+':80'], 
						in_packets=0, 
						in_bytes=0, 
						out_packets=0, 
						out_bytes=0, 
						conn_sched=0, 
						conn_total=1, 
						active=1, 
						inactive=0)
	
	print "Sending more packets"
	# reset connections
	client_list[0].send_packet_to_nps(reset_packet_first_service1.pcap_file_name)
	client_list[0].send_packet_to_nps(reset_packet_first_service2.pcap_file_name)
	client_list[0].send_packet_to_nps(reset_packet_second_service1.pcap_file_name)
	client_list[0].send_packet_to_nps(reset_packet_second_service2.pcap_file_name)
	time.sleep(32)
	ezbox.zero_all_ipvs_stats();
	time.sleep(1)
	client_list[0].send_packet_to_nps(data_packet_to_first_service1.pcap_file_name)
	client_list[0].send_packet_to_nps(data_packet_to_first_service2.pcap_file_name)
	client_list[0].send_packet_to_nps(data_packet_to_first_service2.pcap_file_name)
	client_list[0].send_packet_to_nps(data_packet_to_second_service1.pcap_file_name)
	client_list[0].send_packet_to_nps(data_packet_to_second_service1.pcap_file_name)
	client_list[0].send_packet_to_nps(data_packet_to_second_service2.pcap_file_name)
	client_list[0].send_packet_to_nps(data_packet_to_second_service2.pcap_file_name)
	ezbox.get_ipvs_stats();
	stats_results = ezbox.read_stats_on_syslog()
	
	# service stats
	print "Check service syslog statistics prints"
	compare_service_stats(service_stats=stats_results['service_stats'][vip_list[1]], 
						 in_packets=3, 
						 in_bytes=data_packet_to_first_service1.packet_length + data_packet_to_first_service2.packet_length*2, 
						 out_packets=0, 
						 out_bytes=0, 
						 conn_sched=2)
	
	compare_service_stats(service_stats=stats_results['service_stats'][vip_list[2]], 
						 in_packets=4, 
						 in_bytes=data_packet_to_second_service1.packet_length*2 + data_packet_to_second_service2.packet_length*2, 
						 out_packets=0, 
						 out_bytes=0, 
						 conn_sched=2)
	
	# server stats
	print "Check servers syslog statistics prints"
	compare_server_stats(server_stats=stats_results['server_stats'][vip_list[1]+':80'][server_list[0].ip+':80'], 
						in_packets=1, 
						in_bytes=data_packet_to_first_service1.packet_length, 
						out_packets=0, 
						out_bytes=0, 
						conn_sched=1, 
						conn_total=1, 
						active=1, 
						inactive=0)
	
	compare_server_stats(server_stats=stats_results['server_stats'][vip_list[1]+':80'][server_list[1].ip+':80'], 
						in_packets=2, 
						in_bytes=data_packet_to_first_service2.packet_length*2, 
						out_packets=0, 
						out_bytes=0, 
						conn_sched=1, 
						conn_total=1, 
						active=1, 
						inactive=0)
	
	compare_server_stats(server_stats=stats_results['server_stats'][vip_list[2]+':80'][server_list[2].ip+':80'], 
						in_packets=2, 
						in_bytes=data_packet_to_second_service1.packet_length*2, 
						out_packets=0, 
						out_bytes=0, 
						conn_sched=1, 
						conn_total=1, 
						active=1, 
						inactive=0)
	
	compare_server_stats(server_stats=stats_results['server_stats'][vip_list[2]+':80'][server_list[3].ip+':80'], 
						in_packets=2, 
						in_bytes=data_packet_to_second_service2.packet_length*2, 
						out_packets=0, 
						out_bytes=0, 
						conn_sched=1, 
						conn_total=1, 
						active=1, 
						inactive=0)

main()
	