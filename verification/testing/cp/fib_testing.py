#!/usr/bin/env python

import sys
sys.path.append("verification/testing/")
from test_infra import *
from time import sleep

args = read_test_arg(sys.argv)	

scenarios_to_run = args['scenarios']
scenarios_to_run = [1,2,3,4,5,6,7]

log_file = "scheduling_algorithm_test.log"
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
sleep(5)

def send_packet_and_check_servers(service, 
								  packet, 
								  server1, server2, 
								  expected_packets_1=0, expected_packets_2=0):
	# capture packets from both servers
	print "capture packets from both servers"
	server1.capture_packets_from_service(service=service)
	server2.capture_packets_from_service(service=service)
	 
	# send packet
	print "Send packet"
	client_object.send_packet_to_nps(packet)
	 
	# check where the packet was send
	print"check where the packet was send"
	time.sleep(2)
	packets_received1 = server1.stop_capture()
	packets_received2 = server2.stop_capture()
	 
	print "packets received in server 1 %d"%packets_received1
	print "packets received in server 2 %d"%packets_received2
	 
	# check if packet was dropped
 	if packets_received2 != expected_packets_2 or packets_received1 != expected_packets_1:
 		print "ERROR, packet should be send to the second server"
 		ezbox.flush_fib_entries()
 		exit(1)	
 		
# each setup can use differen VMs
ip_list = get_setup_list(args['setup_num'])
  
# each setup can use different the virtual ip 
virtual_ip_address_1 = get_setup_vip(args['setup_num'], 1)
  
# create servers
server1 = real_server(management_ip=ip_list[0]['hostname'], data_ip=ip_list[0]['ip'])
server2 = real_server(management_ip=ip_list[1]['hostname'], data_ip=ip_list[1]['ip'])
ezbox.execute_command_on_host("arp -s %s %s"%(server2.data_ip, server2.mac_address))
  
# create client
client_object = client(management_ip=ip_list[2]['hostname'], data_ip=ip_list[2]['ip'])
  
# create services on ezbox
first_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_1, port='80', schedule_algorithm = 'source_hash')
first_service.add_server(server1, weight='1')
 
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

# delete all fib entries (except the default entries)
ezbox.flush_fib_entries()

##########################################################################################
#################### add gateway fib entry, checking several kind of masks ###############
##########################################################################################
if 1 in scenarios_to_run:
	
	print "\nScenario 1 - add gateway fib entry, checking several kind of masks"
	# create fib, checking masks 17, 18, 19... to 32
	for mask in range(17,33):
		print "\nTesting gateway fib entry with mask %d"%mask
		
		server_ip = mask_ip(server1.data_ip, mask)
		result,output = ezbox.add_fib_entry(ip=server_ip, mask=mask, gateway=server2.data_ip)
		if result == False:
			print "ERROR, add fib entry was failed"
			print output
			exit(1)
			
		time.sleep(5)
		
		send_packet_and_check_servers(service=first_service, 
								  	  packet=data_packet.pcap_file_name, 
								  	  server1=server1, server2=server2, 
								  	  expected_packets_1=0, expected_packets_2=1)
	 		
	for mask in range(17,33):
		print "\nTesting delete gateway fib entry with mask %d"%mask
		
		#remove FIB configuration
		print "Remove fib entry"
		server_ip = mask_ip(server1.data_ip, mask)
		result, output = ezbox.delete_fib_entry(ip=server_ip, mask=mask)
		if result == False:
			print "ERROR, add fib entry was failed"
			print output
			exit(1)
			
			
	time.sleep(5)
	
	send_packet_and_check_servers(service=first_service, 
						  	  packet=data_packet.pcap_file_name, 
						  	  server1=server1, server2=server2, 
						  	  expected_packets_1=1, expected_packets_2=0)

 	ezbox.flush_fib_entries()

##########################################################################################
#################### add drop fib entry ##################################################
##########################################################################################
if 2 in scenarios_to_run:
	print "\nScenario 2 - add drop fib entry"
	# create fib, checking masks 17, 18, 19... to 32
	for mask in range(17,33):
		for message_type in ['throw', 'prohibit', 'blackhole', 'unreachable']:
			print "Testing on mask %d and message type %s"%(mask,message_type)
			server_ip = mask_ip(server1.data_ip, mask)
			result,output = ezbox.execute_command_on_host('ip route replace ' + message_type + ' ' + server_ip + '/' + str(mask))
			if result == False:
				print "ERROR, add fib entry was failed"
				print output
				exit(1)
	
			time.sleep(2)
			
			send_packet_and_check_servers(service=first_service, 
						  	  packet=data_packet.pcap_file_name, 
						  	  server1=server1, server2=server2, 
						  	  expected_packets_1=0, expected_packets_2=0)
			
			#remove FIB configuration
			server_ip = mask_ip(server1.data_ip, mask)
			result, output = ezbox.delete_fib_entry(ip=server_ip, mask=mask)
			if result == False:
				print "ERROR, add fib entry was failed"
				print output
				exit(1)
				
			time.sleep(2)
	
	ezbox.flush_fib_entries()
	
##########################################################################################
########################### add neighbour fib entry ######################################
##########################################################################################
if 3 in scenarios_to_run:

	ezbox.execute_command_on_host("ipvsadm -d -t %s:80 -r %s:80"%(first_service.virtual_ip,server1.data_ip))
	ezbox.execute_command_on_host("ipvsadm -a -t %s:80 -r 192.168.254.254:80"%(first_service.virtual_ip))
	
	print "\nScenario 3 - check neighbour fib entry"
	# create drop entries
	for mask in range(17,24):
		server_ip = mask_ip('192.168.254.254', mask)
		result,output = ezbox.execute_command_on_host('ip route replace blackhole ' + ' ' + server_ip + '/' + str(mask))
		if result == False:
			print "ERROR, add fib entry was failed"
			print output
			exit(1)
	
	# add a neighbour entry 
	result,output = ezbox.add_fib_entry(ip="192.168.254.254", mask=32, gateway=server2.data_ip)
	if result == False:
		print "ERROR, add fib entry was failed"
		print output
		exit(1)
		
	send_packet_and_check_servers(service=first_service, 
						  	  packet=data_packet.pcap_file_name, 
						  	  server1=server1, server2=server2, 
						  	  expected_packets_1=0, expected_packets_2=1)
				
	#remove FIB configuration
	ezbox.flush_fib_entries()
	ezbox.execute_command_on_host("ipvsadm -d -t %s:80 -r 192.168.254.254:80"%(first_service.virtual_ip))
	first_service.add_server(server1, weight='1')
				
##########################################################################################
########################### check modify fib entry ######################################
##########################################################################################
if 4 in scenarios_to_run:

	print "\nScenario 4 - check modify fib entry"
	print "\nModify from neighbour to drop"
	# add a neighbour entry 
	result,output = ezbox.add_fib_entry(ip=server1.data_ip, mask=32)
	if result == False:
		print "ERROR, add fib entry was failed"
		print output
		exit(1)
			
	time.sleep(3)
	
	send_packet_and_check_servers(service=first_service, 
						  	  packet=data_packet.pcap_file_name, 
						  	  server1=server1, server2=server2, 
						  	  expected_packets_1=1, expected_packets_2=0)
	
	# modify to drop entry
	result,output = ezbox.execute_command_on_host('ip route change blackhole ' + ' ' + server1.data_ip + '/32')
	if result == False:
		print "ERROR, add fib entry was failed"
		print output
		exit(1)	 		
	
	time.sleep(3)
	
	send_packet_and_check_servers(service=first_service, 
						  	  packet=data_packet.pcap_file_name, 
						  	  server1=server1, server2=server2, 
						  	  expected_packets_1=0, expected_packets_2=0)
 		
	# modify to gateway entry
	print "\nModify from drop to gateway"
	result,output = ezbox.execute_command_on_host('ip route change ' + ' ' + server1.data_ip + '/32 via ' + server2.data_ip)
	if result == False:
		print "ERROR, add fib entry was failed"
		print output
		exit(1)	 
			
	time.sleep(3)
	
	send_packet_and_check_servers(service=first_service, 
						  	  packet=data_packet.pcap_file_name, 
						  	  server1=server1, server2=server2, 
						  	  expected_packets_1=0, expected_packets_2=1)
	
	print "\nModify from gateway to neighbour"
	result,output = ezbox.execute_command_on_host('ip route change ' + ' ' + server1.data_ip + '/32 dev ' + ezbox.setup['interface'])
	if result == False:
		print "ERROR, add fib entry was failed"
		print output
		exit(1)	 
			
	time.sleep(3)
	
	send_packet_and_check_servers(service=first_service, 
						  	  packet=data_packet.pcap_file_name, 
						  	  server1=server1, server2=server2, 
						  	  expected_packets_1=1, expected_packets_2=0)
	
	
	print "\nModify from neighbour to gateway"
	result,output = ezbox.execute_command_on_host('ip route change ' + ' ' + server1.data_ip + '/32 via ' + server2.data_ip)
	if result == False:
		print "ERROR, add fib entry was failed"
		print output
		exit(1)	 
			
	time.sleep(3)
	
	send_packet_and_check_servers(service=first_service, 
						  	  packet=data_packet.pcap_file_name, 
						  	  server1=server1, server2=server2, 
						  	  expected_packets_1=0, expected_packets_2=1)
	
	print "\nModify from gateway to drop"
	result,output = ezbox.execute_command_on_host('ip route change blackhole' + ' ' + server1.data_ip + '/32')
	if result == False:
		print "ERROR, add fib entry was failed"
		print output
		exit(1)	 
			
	time.sleep(3)
	
	send_packet_and_check_servers(service=first_service, 
						  	  packet=data_packet.pcap_file_name, 
						  	  server1=server1, server2=server2, 
						  	  expected_packets_1=0, expected_packets_2=0)
	
	
	print "\nModify from drop to neighbour"
	result,output = ezbox.execute_command_on_host('ip route change ' + ' ' + server1.data_ip + '/32 dev ' + ezbox.setup['interface'])
	if result == False:
		print "ERROR, add fib entry was failed"
		print output
		exit(1)	 
			
	time.sleep(3)
	
	send_packet_and_check_servers(service=first_service, 
						  	  packet=data_packet.pcap_file_name, 
						  	  server1=server1, server2=server2, 
						  	  expected_packets_1=1, expected_packets_2=0)
	
	#remove FIB configuration
	result, output = ezbox.delete_fib_entry(ip=server1.data_ip, mask=32)	
	if result == False:
		print "ERROR, add fib entry was failed"
		print output
		exit(1)
		
	ezbox.flush_fib_entries()
##########################################################################################
########################### add max entries to fib table #################################
##########################################################################################
if 5 in scenarios_to_run:
	print "\nScenario 5 - add max entries to fib table"
	# create drop entries
	for i in range(8191):
		if i%100 == 0:
			print i
		server_ip = add2ip('192.168.100.1',i*2)
		server_ip=mask_ip(server_ip, 31)
		result,output = ezbox.add_fib_entry(ip=server_ip, mask=31, gateway=server2.data_ip)
		if result == False:
			print "ERROR, add fib entry was failed"
			print output
			exit(1)
			
	result,output = ezbox.add_fib_entry(ip=server1.data_ip, mask=32, gateway=server2.data_ip)
	if result == False:
		print "ERROR, add fib entry was failed"
		print output
		exit(1)
		
	result,output = ezbox.add_fib_entry(ip='192.168.200.1', mask=32)
	if result == False:
		print "ERROR, add fib entry was failed"
		print output
		exit(1)
		
	# check for an error message on syslog
		
		
	send_packet_and_check_servers(service=first_service, 
					  	  packet=data_packet.pcap_file_name, 
					  	  server1=server1, server2=server2, 
					  	  expected_packets_1=0, expected_packets_2=1)
		
	# capture packets from both servers
	print "capture packets from both servers"
	server1.capture_packets_from_service(service=first_service)
	server2.capture_packets_from_service(service=first_service)
		 
	# send packet
	print "Send packet"
	client_object.send_packet_to_nps(data_packet.pcap_file_name)
		 
	# check where the packet was send
	print"check where the packet was send"
	time.sleep(2)
	packets_received1 = server1.stop_capture()
	packets_received2 = server2.stop_capture()
		 
	print "packets received in server 1 %d"%packets_received1
	print "packets received in server 2 %d"%packets_received2
		 
 	if packets_received2 != 1 or packets_received1 != 0:
 		print "ERROR, packet should be send to the second server"
 		ezbox.delete_fib_entry(ip=mask_ip(server1.data_ip, mask), mask=mask)
 		exit(1)

	# delete entries
	for i in range(8191):
		server_ip = add2ip('192.168.100.1',i*2)
		result,output = ezbox.delete_fib_entry(ip=server_ip, mask=31, gateway=server2.data_ip)
		if result == False:
			print "ERROR, add fib entry was failed"
			print output
			exit(1)
			
	result,output = ezbox.delete_fib_entry(ip=server1.data_ip, mask=32, gateway=server2.data_ip)
	if result == False:
		print "ERROR, add fib entry was failed"
		print output
		exit(1)
		
		
	ezbox.flush_fib_entries()	
		
	### check if no match statistics was added


