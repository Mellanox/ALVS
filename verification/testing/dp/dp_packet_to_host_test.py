#!/usr/bin/env python

import sys
sys.path.append("verification/testing/")
from test_infra import *

ezbox,args = init_test(test_arguments=sys.argv)
scenarios_to_run = args['scenarios']

# get the VMs that we can use with our setup
ip_list = get_setup_list(args['setup_num'])

# connecting to a team virtual machine
client_vm = client(management_ip=ip_list[0]['hostname'], data_ip=ip_list[0]['ip'])

####################################################################################################################
########### scenario 1: send an arp packet to the nps - packet should be send to host, not my mac ##################
####################################################################################################################

if 1 in scenarios_to_run:
	print "\nScenario 1 - Send an arp packet to the nps - packet should be send to host, not my mac"
	
	# create packet
	mac_da = 'ff ff ff ff ff ff ' # broadcast
	mac_sa = client_vm.mac_address
	ether_type = ' 08 06 ' # arp packet type
	data = '00 01 08 00 06 04 00 01 52 54 00 c5 15 44 0a 07 7c 05 00 00 00 00 00 00 ' + ezbox.setup['data_ip_hex_display']
	temp_str = mac_da + mac_sa + ether_type + data
	pcap_file = 'verification/testing/dp/pcap_files/arp_tmp1.pcap'
#	 string_to_pcap_file(temp_str, pcap_file)
	
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
	client_vm.send_packet_to_nps(pcap_to_send)
	time.sleep(5)
	
	# check statistics
	print "Check not my mac error statistics"
	stats_after = 0
	for i in range(5):
		stats_after += ezbox.get_interface_stats(i)['NW_IF_STATS_NOT_MY_MAC']
		
	ezbox.get_ipvs_stats();
	stats_results = ezbox.read_stats_on_syslog()
	stats_after_syslog_prints = 0
	for i in range(4):
		if 'NOT_MY_MAC' in stats_results['interface_stats'][i]:
			stats_after_syslog_prints += stats_results['interface_stats'][i]['NOT_MY_MAC']
								 
	if stats_after < stats_before+50:
		print "ERROR, not my mac stats is incorrect"
		exit(1)
		
	if stats_after_syslog_prints < stats_before_syslog_prints+50:
		print "ERROR, ipvs stats (syslog prints) not my mac stats is incorrect"
		exit(1)
	print "Statistics OK"
	
	# read how many packets were captured
	captured_packets = ezbox.stop_capture()   
	
	print "Captured " + str(captured_packets) + " Packets"   
	
	if captured_packets != 50:
		print "Error on scenario, packet wasnt sent to host"
		exit(1)
		


	print "Scenario Passed\n"



####################################################################################################################
############################### scenario 2: send udp packet - send to host #########################################
####################################################################################################################
if 2 in scenarios_to_run:
	print "\nScenario 2 - Send an IP packet (not udp not tcp) to nps - packet should be send to host"

	# create packet
	mac_da = ezbox.setup['mac_address']
	mac_sa = client_vm.mac_address
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
	client_vm.send_packet_to_nps(pcap_to_send)
	
	# read how many packets were captured
	time.sleep(5)
	captured_packets = ezbox.stop_capture()   

	print "Captured " + str(captured_packets) + " Packets"   

	if captured_packets<500:
		print "Error on scenario, not all udp packets were captured on host"
		exit(1)
		
	# gather stats before
	stats_after = 0
	for i in range(5):
		stats_after += ezbox.get_interface_stats(i)['NW_IF_STATS_NOT_TCP']
		
	if stats_after < stats_before+500:
		print "stats before = %d"%stats_before
		print "stats after = %d"%stats_after
		print "ERROR, not ipv4 stats is incorrect"
		exit(1)
	print "Statistics OK"
	
	print "Scenario Passed\n"

####################################################################################################################
############################### scenario 3: ip checksum error - send to host #######################################
####################################################################################################################
if 3 in scenarios_to_run:
	print "\nScenario 3 - send ip checksum error packet to host - send to host"

	# create packet
	mac_da = ezbox.setup['mac_address']
	mac_sa = client_vm.mac_address
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
	client_vm.send_packet_to_nps(pcap_file)
	
	time.sleep(2)
	captured_packets = ezbox.stop_capture()   

	print "Captured " + str(captured_packets) + " Packets"   

	if captured_packets != 1:
		print "Error on scenario, packet wasnt sent to host"
		exit(1)
		
	# gather stats after
	stats_after = 0
	for i in range(5):
		stats_after += ezbox.get_interface_stats(i)['NW_IF_STATS_IPV4_ERROR']
		
	if stats_after != stats_before+1:
		print "ERROR, not ipv4 stats is incorrect"
		exit(1)
	print "Statistics OK"


	print "Scenario Passed\n"


####################################################################################################################
################## scenario 4: send a ipv6 to nps - packet should be send to host #########################
####################################################################################################################
if 4 in scenarios_to_run:
	print "\nScenario 4 - send a ipv6 packet to nps - packet should be send to host"

	# create packet
	mac_da = ezbox.setup['mac_address']
	mac_sa = client_vm.mac_address
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
	client_vm.send_packet_to_nps(pcap_file)

	time.sleep(0.5)
	captured_packets = ezbox.stop_capture()   

	
	print "Captured " + str(captured_packets) + " Packets"   
	

	if captured_packets != 1:
		print "Error on scenario, packet wasnt sent to host"
		exit(1)
		
	# gather stats after
	stats_after = 0
	for i in range(5):
		stats_after += ezbox.get_interface_stats(i)['NW_IF_STATS_NOT_IPV4']
		
	if stats_after != stats_before+1:
		print "ERROR, not ipv4 stats is incorrect"
		exit(1)
	print "Statistics OK"


	print "Scenario Passed\n"

	
#########################################################################################################
############################### scenario 5: mac error - send to host ####################################
#########################################################################################################
if 5 in scenarios_to_run:
	print "\nScenario 5 - send mac error packet to host - send to host"

	# create packet
	mac_da = '00 00 00 00 00 00'#ezbox.setup['mac_address']
	mac_sa = client_vm.mac_address
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
	client_vm.send_packet_to_nps(pcap_file)
	
	time.sleep(1)
	captured_packets = ezbox.stop_capture()   

	
	print "Captured " + str(captured_packets) + " Packets"   
	
	if captured_packets != 1:
		print "Error on scenario, packet wasnt sent to host"
		exit(1)


	# gather stats after
	stats_after = 0
	for i in range(5):
		stats_after += ezbox.get_interface_stats(i)['NW_IF_STATS_MAC_ERROR']
		
	if stats_after != stats_before+1:
		print "ERROR, not ipv4 stats is incorrect"
		exit(1)
	print "Statistics OK"
	

	print "Scenario Passed"

print
print "Test Passed\n"

