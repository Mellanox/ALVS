#!/usr/bin/env python

import sys
sys.path.append("verification/testing/")
from test_infra import *
from time import sleep

ezbox,args = init_test(test_arguments=sys.argv)
scenarios_to_run = args['scenarios']

# each setup can use differen VMs
ip_list = get_setup_list(args['setup_num'])
  
# each setup can use different the virtual ip 
virtual_ip_address_1 = get_setup_vip(args['setup_num'], 1)
virtual_ip_address_2 = get_setup_vip(args['setup_num'], 2)

# create servers
server1 = real_server(management_ip=ip_list[0]['hostname'], data_ip=ip_list[0]['ip'])
server2 = real_server(management_ip=ip_list[1]['hostname'], data_ip=ip_list[1]['ip'])
  
# create client
client_object = client(management_ip=ip_list[2]['hostname'], data_ip=ip_list[2]['ip'])
  
# create services on ezbox
first_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_1, port='80', schedule_algorithm = 'source_hash_with_source_port')
first_service.add_server(server1, weight='1')
first_service.add_server(server2, weight='1')

second_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_2, port='80', schedule_algorithm = 'source_hash_with_source_port')
second_service.add_server(server1, weight='1')
second_service.add_server(server2, weight='1')

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

# create packet
data_packet_to_first_service1 = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src='192.168.0.100',
                         tcp_source_port = '00 00',
                         tcp_dst_port = '00 50', # port 80
                         tcp_reset_flag = False,
                         tcp_fin_flag = False,
                         packet_length=128)
data_packet_to_first_service1.generate_packet()

data_packet_to_first_service2 = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src='192.168.0.100',
                         tcp_source_port = '00 03',
                         tcp_dst_port = '00 50', # port 80
                         tcp_reset_flag = False,
                         tcp_fin_flag = False,
                         packet_length=222)
data_packet_to_first_service2.generate_packet()

data_packet_to_second_service1 = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=second_service.virtual_ip_hex_display,
                         ip_src='192.168.0.100',
                         tcp_source_port = '00 00',
                         tcp_dst_port = '00 50', # port 80
                         tcp_reset_flag = False,
                         tcp_fin_flag = False,
                         packet_length=111)
data_packet_to_second_service1.generate_packet()

data_packet_to_second_service2 = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=second_service.virtual_ip_hex_display,
                         ip_src='192.168.0.100',
                         tcp_source_port = '00 03',
                         tcp_dst_port = '00 50', # port 80
                         tcp_reset_flag = False,
                         tcp_fin_flag = False,
                         packet_length=333)
data_packet_to_second_service2.generate_packet()

reset_packet_first_service1 = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src='192.168.0.100',
                         tcp_source_port = '00 00',
                         tcp_dst_port = '00 50', # port 80
                         tcp_reset_flag = True,
                         tcp_fin_flag = False,
                         packet_length=111)
reset_packet_first_service1.generate_packet()

reset_packet_first_service2 = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src='192.168.0.100',
                         tcp_source_port = '00 03',
                         tcp_dst_port = '00 50', # port 80
                         tcp_reset_flag = True,
                         tcp_fin_flag = False,
                         packet_length=111)
reset_packet_first_service2.generate_packet()

reset_packet_second_service1 = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=second_service.virtual_ip_hex_display,
                         ip_src='192.168.0.100',
                         tcp_source_port = '00 00',
                         tcp_dst_port = '00 50', # port 80
                         tcp_reset_flag = True,
                         tcp_fin_flag = False,
                         packet_length=111)
reset_packet_second_service1.generate_packet()

reset_packet_second_service2 = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=second_service.virtual_ip_hex_display,
                         ip_src='192.168.0.100',
                         tcp_source_port = '00 03',
                         tcp_dst_port = '00 50', # port 80
                         tcp_reset_flag = True,
                         tcp_fin_flag = False,
                         packet_length=111)
reset_packet_second_service2.generate_packet()

fin_packet_to_first_service = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src='192.168.0.100',
                         tcp_source_port = '00 03',
                         tcp_dst_port = '00 50', # port 80
                         tcp_fin_flag = True,
                         packet_length=128)
fin_packet_to_first_service.generate_packet()

print "Sending several packets"
client_object.send_packet_to_nps(data_packet_to_first_service1.pcap_file_name)
client_object.send_packet_to_nps(data_packet_to_first_service2.pcap_file_name)
client_object.send_packet_to_nps(data_packet_to_second_service1.pcap_file_name)
client_object.send_packet_to_nps(data_packet_to_second_service1.pcap_file_name)
client_object.send_packet_to_nps(data_packet_to_second_service2.pcap_file_name)
ezbox.get_ipvs_stats();
stats_results = ezbox.read_stats_on_syslog()

# service stats
print "Check service syslog statistics prints"
compare_service_stats(service_stats=stats_results['service_stats'][first_service.virtual_ip], 
					 in_packets=2, 
					 in_bytes=data_packet_to_first_service1.packet_length + data_packet_to_first_service2.packet_length, 
					 out_packets=0, 
					 out_bytes=0, 
					 conn_sched=2)

compare_service_stats(service_stats=stats_results['service_stats'][second_service.virtual_ip], 
					 in_packets=3, 
					 in_bytes=data_packet_to_second_service1.packet_length*2 + data_packet_to_second_service2.packet_length, 
					 out_packets=0, 
					 out_bytes=0, 
					 conn_sched=2)

# server stats
print "Check servers syslog statistics prints"
compare_server_stats(server_stats=stats_results['server_stats'][first_service.virtual_ip+':80'][server1.data_ip+':80'], 
					in_packets=1, 
					in_bytes=data_packet_to_first_service1.packet_length, 
					out_packets=0, 
					out_bytes=0, 
					conn_sched=1, 
					conn_total=1, 
					active=1, 
					inactive=0)

compare_server_stats(server_stats=stats_results['server_stats'][first_service.virtual_ip+':80'][server2.data_ip+':80'], 
					in_packets=1, 
					in_bytes=data_packet_to_first_service2.packet_length, 
					out_packets=0, 
					out_bytes=0, 
					conn_sched=1, 
					conn_total=1, 
					active=1, 
					inactive=0)

compare_server_stats(server_stats=stats_results['server_stats'][second_service.virtual_ip+':80'][server1.data_ip+':80'], 
					in_packets=2, 
					in_bytes=data_packet_to_second_service1.packet_length*2, 
					out_packets=0, 
					out_bytes=0, 
					conn_sched=1, 
					conn_total=1, 
					active=1, 
					inactive=0)

compare_server_stats(server_stats=stats_results['server_stats'][second_service.virtual_ip+':80'][server2.data_ip+':80'], 
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
client_object.send_packet_to_nps(fin_packet_to_first_service.pcap_file_name)
ezbox.get_ipvs_stats();
stats_results = ezbox.read_stats_on_syslog()

print "Check Service Statistics"
compare_service_stats(service_stats=stats_results['service_stats'][first_service.virtual_ip], 
					 in_packets=3, 
					 in_bytes=data_packet_to_first_service1.packet_length + data_packet_to_first_service2.packet_length + fin_packet_to_first_service.packet_length, 
					 out_packets=0, 
					 out_bytes=0, 
					 conn_sched=2)

print "Check Server Statistics"
compare_server_stats(server_stats=stats_results['server_stats'][first_service.virtual_ip+':80'][server2.data_ip+':80'], 
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
compare_server_stats(server_stats=stats_results['server_stats'][first_service.virtual_ip+':80'][server2.data_ip+':80'], 
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
compare_service_stats(service_stats=stats_results['service_stats'][first_service.virtual_ip], 
					 in_packets=0, 
					 in_bytes=0, 
					 out_packets=0, 
					 out_bytes=0, 
					 conn_sched=0)

compare_service_stats(service_stats=stats_results['service_stats'][second_service.virtual_ip], 
					 in_packets=0, 
					 in_bytes=0, 
					 out_packets=0, 
					 out_bytes=0, 
					 conn_sched=0)

# server stats
print "Check servers syslog statistics prints"
compare_server_stats(server_stats=stats_results['server_stats'][first_service.virtual_ip+':80'][server1.data_ip+':80'], 
					in_packets=0, 
					in_bytes=0, 
					out_packets=0, 
					out_bytes=0, 
					conn_sched=0, 
					conn_total=1, 
					active=1, 
					inactive=0)

compare_server_stats(server_stats=stats_results['server_stats'][first_service.virtual_ip+':80'][server2.data_ip+':80'], 
					in_packets=0, 
					in_bytes=0, 
					out_packets=0, 
					out_bytes=0, 
					conn_sched=0, 
					conn_total=0, 
					active=0, 
					inactive=0)

compare_server_stats(server_stats=stats_results['server_stats'][second_service.virtual_ip+':80'][server1.data_ip+':80'], 
					in_packets=0, 
					in_bytes=0, 
					out_packets=0, 
					out_bytes=0, 
					conn_sched=0, 
					conn_total=1, 
					active=1, 
					inactive=0)

compare_server_stats(server_stats=stats_results['server_stats'][second_service.virtual_ip+':80'][server2.data_ip+':80'], 
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
client_object.send_packet_to_nps(reset_packet_first_service1.pcap_file_name)
client_object.send_packet_to_nps(reset_packet_first_service2.pcap_file_name)
client_object.send_packet_to_nps(reset_packet_second_service1.pcap_file_name)
client_object.send_packet_to_nps(reset_packet_second_service2.pcap_file_name)
time.sleep(32)
ezbox.zero_all_ipvs_stats();
time.sleep(1)
client_object.send_packet_to_nps(data_packet_to_first_service1.pcap_file_name)
client_object.send_packet_to_nps(data_packet_to_first_service2.pcap_file_name)
client_object.send_packet_to_nps(data_packet_to_first_service2.pcap_file_name)
client_object.send_packet_to_nps(data_packet_to_second_service1.pcap_file_name)
client_object.send_packet_to_nps(data_packet_to_second_service1.pcap_file_name)
client_object.send_packet_to_nps(data_packet_to_second_service2.pcap_file_name)
client_object.send_packet_to_nps(data_packet_to_second_service2.pcap_file_name)
ezbox.get_ipvs_stats();
stats_results = ezbox.read_stats_on_syslog()

# service stats
print "Check service syslog statistics prints"
compare_service_stats(service_stats=stats_results['service_stats'][first_service.virtual_ip], 
					 in_packets=3, 
					 in_bytes=data_packet_to_first_service1.packet_length + data_packet_to_first_service2.packet_length*2, 
					 out_packets=0, 
					 out_bytes=0, 
					 conn_sched=2)

compare_service_stats(service_stats=stats_results['service_stats'][second_service.virtual_ip], 
					 in_packets=4, 
					 in_bytes=data_packet_to_second_service1.packet_length*2 + data_packet_to_second_service2.packet_length*2, 
					 out_packets=0, 
					 out_bytes=0, 
					 conn_sched=2)

# server stats
print "Check servers syslog statistics prints"
compare_server_stats(server_stats=stats_results['server_stats'][first_service.virtual_ip+':80'][server1.data_ip+':80'], 
					in_packets=1, 
					in_bytes=data_packet_to_first_service1.packet_length, 
					out_packets=0, 
					out_bytes=0, 
					conn_sched=1, 
					conn_total=1, 
					active=1, 
					inactive=0)

compare_server_stats(server_stats=stats_results['server_stats'][first_service.virtual_ip+':80'][server2.data_ip+':80'], 
					in_packets=2, 
					in_bytes=data_packet_to_first_service2.packet_length*2, 
					out_packets=0, 
					out_bytes=0, 
					conn_sched=1, 
					conn_total=1, 
					active=1, 
					inactive=0)

compare_server_stats(server_stats=stats_results['server_stats'][second_service.virtual_ip+':80'][server1.data_ip+':80'], 
					in_packets=2, 
					in_bytes=data_packet_to_second_service1.packet_length*2, 
					out_packets=0, 
					out_bytes=0, 
					conn_sched=1, 
					conn_total=1, 
					active=1, 
					inactive=0)

compare_server_stats(server_stats=stats_results['server_stats'][second_service.virtual_ip+':80'][server2.data_ip+':80'], 
					in_packets=2, 
					in_bytes=data_packet_to_second_service2.packet_length*2, 
					out_packets=0, 
					out_bytes=0, 
					conn_sched=1, 
					conn_total=1, 
					active=1, 
					inactive=0)

print "Test Passed"
# pprint.pprint(stats_results)

# client_object.send_packet_to_nps(reset_packet.pcap_file_name)
# ezbox.get_ipvs_stats();
# client_object.send_packet_to_nps(data_packet.pcap_file_name)
# ezbox.get_ipvs_stats();
# client_object.send_packet_to_nps(data_packet.pcap_file_name)
# ezbox.get_ipvs_stats();
# client_object.send_packet_to_nps(fin_packet.pcap_file_name)
# ezbox.get_ipvs_stats();
# client_object.send_packet_to_nps(data_packet.pcap_file_name)
# ezbox.get_ipvs_stats();
# 
# reset_reset_packets = create_pcap_file(packets_list=[reset_packet.packet,reset_packet.packet])
# print reset_reset_packets
# client_object.send_packet_to_nps(reset_reset_packets)
# 
# a = ezbox.get_ipvs_stats();
# 
# b = ezbox.read_stats_on_syslog()

# ret, output = ezbox.execute_command_on_host("tail -n 30 /var/log/syslog")
# 
# service_stats = []
# output = output.split('\n')
# for i,line in enumerate(output):
# 	if 'Print Statistics for Services' in line:
# 		for j in range(2,300):
# 			temp = output[i+j].split()
# 			print temp
# 			temp_ip = temp[6].split(':')[0]
# 			temp_port = temp_ip.split(':')[1]
# 			
# 			if '(prot' not in temp[7]:
# 				break
		


# first_service.remove_server(server1)

# ezbox.syslog_ssh.ssh_object.expect("", 20)
# 
# ezbox.zero_all_ipvs_stats();
# 
# ezbox.get_ipvs_stats();