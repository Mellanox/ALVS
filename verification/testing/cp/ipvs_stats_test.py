#!/usr/bin/env python

import sys
sys.path.append("verification/testing/")
from test_infra import *
from time import sleep

args = read_test_arg(sys.argv)	

scenarios_to_run = args['scenarios']
scenarios_to_run = []

log_file = "ipvs_stats_test.log"
if 'log_file' in args:
	log_file = args['log_file']
init_logging(log_file)

ezbox = ezbox_host(args['setup_num'])

if args['hard_reset']:
	ezbox.reset_ezbox()

if args['compile']:
	compile(clean=True, debug=args['debug'])

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
ip_list = get_setup_list(2)
  
# each setup can use different the virtual ip 
virtual_ip_address_1 = get_setup_vip(args['setup_num'], 1)
virtual_ip_address_2 = get_setup_vip(args['setup_num'], 2)
  
# create servers
server1 = real_server(management_ip=ip_list[0]['hostname'], data_ip=ip_list[0]['ip'])
server2 = real_server(management_ip=ip_list[1]['hostname'], data_ip=ip_list[1]['ip'])
  
# create client
client_object = client(management_ip=ip_list[2]['hostname'], data_ip=ip_list[2]['ip'])
  
# create services on ezbox
first_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_1, port='80', schedule_algorithm = 'source_hash')
first_service.add_server(server1, weight='1')
first_service.add_server(server2, weight='1')

second_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_2, port='80', schedule_algorithm = 'source_hash')
second_service.add_server(server1, weight='1')
second_service.add_server(server2, weight='1')


# create packet
data_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src=client_object.hex_display_to_ip,
                         tcp_source_port = '00 00',
                         tcp_dst_port = '00 50', # port 80
                         tcp_reset_flag = False,
                         tcp_fin_flag = False,
                         packet_length=128)
data_packet.generate_packet()

reset_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src=client_object.hex_display_to_ip,
                         tcp_source_port = '00 02',
                         tcp_dst_port = '00 50', # port 80
                         tcp_reset_flag = True,
                         tcp_fin_flag = False,
                         packet_length=128)
reset_packet.generate_packet()

fin_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
                         mac_sa=client_object.mac_address,
                         ip_dst=first_service.virtual_ip_hex_display,
                         ip_src=client_object.hex_display_to_ip,
                         tcp_source_port = '00 03',
                         tcp_dst_port = '00 50', # port 80
                         tcp_fin_flag = True,
                         packet_length=128)
fin_packet.generate_packet()

print "Sending one packet"
client_object.send_packet_to_nps(data_packet.pcap_file_name)
client_object.send_packet_to_nps(reset_packet.pcap_file_name)
client_object.send_packet_to_nps(data_packet.pcap_file_name)
client_object.send_packet_to_nps(data_packet.pcap_file_name)
client_object.send_packet_to_nps(fin_packet.pcap_file_name)
client_object.send_packet_to_nps(data_packet.pcap_file_name)

reset_reset_packets = create_pcap_file(packets_list=[reset_packet.packet,reset_packet.packet])
print reset_reset_packets
client_object.send_packet_to_nps(reset_reset_packets)

ezbox.get_ipvs_stats();

ret, output = ezbox.execute_command_on_host("tail -n 30 /var/log/syslog")

service_stats = []
output = output.split('\n')
for i,line in enumerate(output):
	if 'Print Statistics for Services' in line:
		for j in range(2,300):
			temp = output[i+j].split()
			print temp
			temp_ip = temp[6].split(':')[0]
			temp_port = temp_ip.split(':')[1]
			
			if '(prot' not in temp[7]:
				break
		


# first_service.remove_server(server1)

# ezbox.syslog_ssh.ssh_object.expect("", 20)
# 
# ezbox.zero_all_ipvs_stats();
# 
# ezbox.get_ipvs_stats();