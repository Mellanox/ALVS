#!/usr/bin/env python
import sys
sys.path.append("verification/testing/")
from common_infra import *
from client_infra import *
from e2e_infra import *

server_count   = 0
client_count   = 1
service_count  = 0

def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	dict = generic_init(setup_num, service_count, server_count, client_count)
	
	return dict

def main():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	config = fill_default_config(generic_main())
	
	dict = user_init(config['setup_num'])
	
	init_players(dict, config)
	
	server_list, ezbox, client_list, vip_list = convert_generic_init_to_user_format(dict)
	
	results, expected = run_user_test(server_list, ezbox, client_list, vip_list)
	
	clean_players(dict, True, config['stop_ezbox'])
	
	pcap_rc = pcap_checker(results , expected)
	
	if pcap_rc:
		print 'Test passed !!!'
		exit(0)
	else:
		print 'Test failed !!!'
		exit(1)

def run_user_test(server_list, ezbox, client_list, vip_list):
	test_results = [] #will include dictionaries for each step result
	test_expects = [] #will include dictionaries for each step expected
	mac_sa = client_list[0].get_mac_adress()

####################################################################################################################
########### scenario 1: send an arp packet to the nps - packet should be send to host, not my mac ##################
####################################################################################################################
	print
	print "Step 0 - Send an arp packet to the nps - packet should be send to host, not my mac"
	step_res = {}
	step_expect = {}
	# create packet
	mac_da = 'ff ff ff ff ff ff ' # broadcast
	ether_type = ' 08 06 ' # arp packet type
	data = '00 01 08 00 06 04 00 01 52 54 00 c5 15 44 0a 07 7c 05 00 00 00 00 00 00 ' + ezbox.setup['data_ip_hex_display']
	temp_str = mac_da + mac_sa + ether_type + data

	packets_to_send_list = [temp_str] * 50
	pcap_to_send = create_pcap_file(packets_list=packets_to_send_list, output_pcap_file_name='verification/testing/dp/pcap_files/temp_packet.pcap')
	# capture packets on host
	ezbox.capture_packets()
	
	# gather stats before
	stats_before = 0
	stats_before_syslog_prints = 0
	
	ezbox.get_ipvs_stats();
	stats_results = ezbox.read_stats_on_syslog()
	stats_before_syslog_prints = 0
	
	for i in range(4):
		if 'NOT_MY_MAC' in stats_results['interface_stats'][i]:
			stats_before_syslog_prints += stats_results['interface_stats'][i]['NOT_MY_MAC']

	for i in range(5):
		stats_before += ezbox.get_interface_stats(i)['NW_IF_STATS_NOT_MY_MAC']
		
	# send packet to NPS
	print "Send arp packet to NPS"
	client_list[0].send_packet_to_nps(pcap_to_send)
	time.sleep(5)
	
	# check statistics
	stats_after = 0
	for i in range(5):
		stats_after += ezbox.get_interface_stats(i)['NW_IF_STATS_NOT_MY_MAC']
		
	# read how many packets were captured
	captured_packets = ezbox.stop_capture()   
	print "captured_packets %d"%captured_packets
	step_res['compare_stats'] = stats_after
	step_expect['compare_stats'] = stats_before + 50
	step_res['count_pcap_checker'] = captured_packets
	step_expect['count_pcap_checker'] = 50
	
	test_results.append(step_res)
	test_expects.append(step_expect)
	
####################################################################################################################
############################### scenario 2: send udp packet - send to host #########################################
####################################################################################################################
	print "Step 1 - Send an IP packet (not udp not tcp) to nps - packet should be send to host"
	step_res = {}
	step_expect = {}

	# create packet
	mac_da = ezbox.setup['mac_address']
	ether_type = ' 08 00' # ip packet type
	data = ' 45 00 00 2e 00 00 40 00 40 11 34 BA 01 01 01 01 02 02 02 02 00 00 00 00 00 1A F9 B4 00 00 00 00 00 00 00 00 00 00 00 00 00 00 '
	temp_str = mac_da + ' ' + mac_sa + ether_type + data
	pcap_file = 'verification/testing/dp/pcap_files/udp_packet.pcap'
	string_to_pcap_file(temp_str, pcap_file)

	packets_to_send_list = [temp_str] * 500
	pcap_to_send = create_pcap_file(packets_list=packets_to_send_list, output_pcap_file_name='verification/testing/dp/pcap_files/500_udp_packets.pcap')

	# gather stats before
	stats_before = 0
	for i in range(5):
		stats_before += ezbox.get_interface_stats(i)['NW_IF_STATS_NOT_TCP']

	# capture packets on host
	ezbox.capture_packets('dst 2.2.2.2 and udp and ether host ' + ezbox.setup['mac_address'].replace(' ',':'))

	# send packet to NPS
	print "Send the packet to NPS"
	client_list[0].send_packet_to_nps(pcap_file)
	
	# read how many packets were captured
	time.sleep(5)
	captured_packets = ezbox.stop_capture()

	# gather stats before
	stats_after = 0
	for i in range(5):
		stats_after += ezbox.get_interface_stats(i)['NW_IF_STATS_NOT_TCP']
		
	step_res['compare_stats'] = stats_after
	step_expect['compare_stats'] = stats_before + 1
	step_res['count_pcap_checker'] = captured_packets
	step_expect['count_pcap_checker'] = 1
	
	test_results.append(step_res)
	test_expects.append(step_expect)

####################################################################################################################
############################### scenario 3: ip checksum error - send to host #######################################
####################################################################################################################
	print "Step 2 - send ip checksum error packet to host - send to host"
	step_res = {}
	step_expect = {}

	# create packet
	mac_da = ezbox.setup['mac_address']
	ether_type = ' 08 00 ' # ip packet type
	data = '45 00 00 2e 00 00 40 00 40 06 2e bc 0a 07 7c 05 0a 07 7c 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 '
	temp_str = mac_da + ' ' + mac_sa + ether_type + data
	pcap_file = 'verification/testing/dp/pcap_files/ip_checksum_error.pcap'
	string_to_pcap_file(temp_str, pcap_file)
	
	# gather statistics before
	stats_before = 0
	for i in range(5):
		stats_before += ezbox.get_interface_stats(i)['NW_IF_STATS_IPV4_ERROR']
	
	# capture packets on host
	ezbox.capture_packets('src 10.7.124.5 and ether host ' + ezbox.setup['mac_address'].replace(' ',':'))
	
	print "Send arp packet to NPS"
	client_list[0].send_packet_to_nps(pcap_file)
	
	time.sleep(2)
	captured_packets = ezbox.stop_capture()   

	# gather stats after
	stats_after = 0
	for i in range(5):
		stats_after += ezbox.get_interface_stats(i)['NW_IF_STATS_IPV4_ERROR']
		
	step_res['compare_stats'] = stats_after
	step_expect['compare_stats'] = stats_before + 1
	step_res['count_pcap_checker'] = captured_packets
	step_expect['count_pcap_checker'] = 1
	
	test_results.append(step_res)
	test_expects.append(step_expect)

####################################################################################################################
################## scenario 4: send a ipv6 to nps - packet should be send to host #########################
####################################################################################################################
	print "Step 3 - send a ipv6 packet to nps - packet should be send to host"
	step_res = {}
	step_expect = {}

	# create packet
	mac_da = ezbox.setup['mac_address']
	ether_type = ' 86 DD ' # ipv6 packet type
	data = '60 00 00 00 00 46 06 7F 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00'
	temp_str = mac_da + ' ' + mac_sa + ether_type + data
	pcap_file = 'verification/testing/dp/pcap_files/ipv6_packet.pcap'
	string_to_pcap_file(temp_str, pcap_file)

	# gather stats before
	stats_before = 0
	for i in range(5):
		stats_before += ezbox.get_interface_stats(i)['NW_IF_STATS_NOT_IPV4']
		
	# capture packets on host
	ezbox.capture_packets('ipv6 and ether host ' + ezbox.setup['mac_address'].replace(' ',':'))

	# send packet to NPS
	print "Send arp packet to NPS"
	client_list[0].send_packet_to_nps(pcap_file)

	time.sleep(0.5)
	captured_packets = ezbox.stop_capture()   
		
	# gather stats after
	stats_after = 0
	for i in range(5):
		stats_after += ezbox.get_interface_stats(i)['NW_IF_STATS_NOT_IPV4']
		
	step_res['compare_stats'] = stats_after
	step_expect['compare_stats'] = stats_before + 1
	step_res['count_pcap_checker'] = captured_packets
	step_expect['count_pcap_checker'] = 1
	
	test_results.append(step_res)
	test_expects.append(step_expect)

#########################################################################################################
############################### scenario 5: mac error - send to host ####################################
#########################################################################################################
	print "Step 4 - send mac error packet to host - send to host"
	step_res = {}
	step_expect = {}
	# create packet
	mac_da = '00 00 00 00 00 00'#ezbox.setup['mac_address']
	ether_type = ' 08 00 ' # ip packet type
	data = '45 00 00 2e 00 00 40 00 40 06 2e bc 0a 07 7c 05 0a 07 7c 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 '
	temp_str = mac_da + ' ' + mac_sa + ether_type + data
	pcap_file = 'verification/testing/dp/pcap_files/1_packet.pcap'
	string_to_pcap_file(temp_str, pcap_file) 

	# gather stats before
	stats_before = 0
	for i in range(5):
		stats_before += ezbox.get_interface_stats(i)['NW_IF_STATS_MAC_ERROR']

	# capture packets on host
	ezbox.capture_packets('ipv4 and ether host ' + ezbox.setup['mac_address'].replace(' ',':'))
	
	print "Send arp packet to NPS"
	client_list[0].send_packet_to_nps(pcap_file)
	time.sleep(1)
	captured_packets = ezbox.stop_capture()   
	
	# gather stats after
	stats_after = 0
	for i in range(5):
		stats_after += ezbox.get_interface_stats(i)['NW_IF_STATS_MAC_ERROR']

	step_res['compare_stats'] = stats_after
	step_expect['compare_stats'] = stats_before + 1
	step_res['count_pcap_checker'] = captured_packets
	step_expect['count_pcap_checker'] = 1
	
	test_results.append(step_res)
	test_expects.append(step_expect)

	return test_results, test_expects

main()
