#!/usr/bin/env python

import sys
sys.path.append("verification/testing/")
from time import sleep
from random import shuffle
from e2e_infra import *
from common_infra import *

server_count   = 2
client_count   = 1
service_count  = 1

def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	dict = generic_init(setup_num, service_count, server_count, client_count)
	
	for s in dict['server_list']:
		s.vip = dict['vip_list'][0]
	
	return dict

def main():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	config = fill_default_config(generic_main())
	
	#need to turn off director
	config['use_director'] = False
	
	dict = user_init(config['setup_num'])
	
	init_players(dict, config)
	
	server_list, ezbox, client_list, vip_list = convert_generic_init_to_user_format(dict)
	
	run_user_test(server_list, ezbox, client_list, vip_list)
	
	clean_players(dict, True, config['stop_ezbox'])
	
	print "Test Passed"
	

def send_packet_and_check_servers(ezbox,
								  service_vip, 
								  packet, 
								  server_list, client_list, 
								  expected_packets = [0 , 0]):
	# capture packets from both servers
	print "capture packets from both servers"
	server_list[0].capture_packets_from_service(service_vip = service_vip)
	server_list[1].capture_packets_from_service(service_vip = service_vip)
	 
	time.sleep(2)
	# send packet
	print "Send packet"
	client_list[0].send_packet_to_nps(packet)
	 
	# check where the packet was send
	print"check where the packet was send"
	time.sleep(2)
	packets_received1 = server_list[0].stop_capture()
	packets_received2 = server_list[1].stop_capture()
	 
	print "packets received in server 1 %d"%packets_received1
	print "packets received in server 2 %d"%packets_received2
	 
	# check if packet was dropped
 	if packets_received2 != expected_packets[1] or packets_received1 != expected_packets[0]:
 		print "ERROR, packet should be send to the second server"
 		exit(1)	
 		

def run_user_test(server_list, ezbox, client_list, vip_list):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	ezbox.flush_ipvs()
	ezbox.flush_fib_entries()
	sleep(5)
	
	process_list = []
	port = '80'
	sched_algorithm = 'sh'

	ezbox.execute_command_on_host("arp -s %s %s"%(server_list[1].ip, server_list[1].mac_address))
	  
	for i in range(service_count):
		print "service %s is set with %s scheduling algorithm" %(vip_list[i],sched_algorithm)
		ezbox.add_service(vip_list[i], port, sched_alg=sched_algorithm, sched_alg_opt='')
	
	ezbox.add_server(server_list[0].vip, port, server_list[0].ip, port)
	ezbox.execute_command_on_host("arp -s %s %s"%(server_list[0].ip, server_list[0].mac_address))
# 	 
	# create packet
	data_packet = tcp_packet(mac_da=ezbox.setup['mac_address'],
	                         mac_sa=client_list[0].mac_address.replace(':',' '),
	                         ip_dst=ip_to_hex_display(vip_list[0]),
	                         ip_src=ip_to_hex_display(client_list[0].ip),
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
		
	print "\nScenario 1 - add gateway fib entry, checking several kind of masks"
	# create fib, checking masks 17, 18, 19... to 32
	masks_to_try = range(17,33)
	shuffle(masks_to_try)
	print "adding masks in this order %s"%masks_to_try
	for mask in masks_to_try:
		print "\nTesting gateway fib entry with mask %d"%mask
		
		server_ip = mask_ip(server_list[0].ip, mask)
		result,output = ezbox.add_fib_entry(ip=server_ip, mask=mask, gateway=server_list[1].ip)
		if result == False:
			print "ERROR, add fib entry was failed"
			print output
			exit(1)
			
		time.sleep(1)
		
		send_packet_and_check_servers(ezbox,
									  service_vip = vip_list[0],
								  	  packet=data_packet.pcap_file_name,
								  	  server_list = server_list, client_list = client_list,
								  	  expected_packets = [0 , 1])
	 		
	masks_to_try = range(17,33)
	shuffle(masks_to_try)
	print "delete entries with masks in this order %s"%masks_to_try
	for mask in masks_to_try:
		print "\nTesting delete gateway fib entry with mask %d"%mask
		
		#remove FIB configuration
		print "Remove fib entry"
		server_ip = mask_ip(server_list[0].ip, mask)
		result, output = ezbox.delete_fib_entry(ip=server_ip, mask=mask)
		if result == False:
			print "ERROR, add fib entry was failed"
			print output
			exit(1)
			
	ezbox.add_server(server_list[0].vip, port, server_list[0].ip, port)
	
	send_packet_and_check_servers(ezbox,
							  service_vip = vip_list[0], 
						  	  packet=data_packet.pcap_file_name, 
						  	  server_list = server_list, client_list = client_list, 
						  	  expected_packets = [1 , 0])
 	ezbox.flush_fib_entries()
	
	##########################################################################################
	#################### add drop fib entry ##################################################
	##########################################################################################
	print "\nScenario 2 - add drop fib entry"
	# create fib, checking masks 17, 18, 19... to 32
	masks_to_try = range(17,33)
	shuffle(masks_to_try)
	print "adding masks in this order %s"%masks_to_try
	for mask in masks_to_try:
		for message_type in ['throw', 'prohibit', 'blackhole', 'unreachable']:
			print "Testing on mask %d and message type %s"%(mask,message_type)
			server_ip = mask_ip(server_list[0].ip, mask)
			result,output = ezbox.execute_command_on_host('ip route replace ' + message_type + ' ' + server_ip + '/' + str(mask))
			if result == False:
				print "ERROR, add fib entry was failed"
				print output
				exit(1)
	
			time.sleep(1)
			
			send_packet_and_check_servers(ezbox,
							  service_vip=vip_list[0], 
						  	  packet=data_packet.pcap_file_name, 
						  	  server_list = server_list, client_list = client_list, 
						  	  expected_packets = [0 , 0])
			
			#remove FIB configuration
			server_ip = mask_ip(server_list[0].ip, mask)
			result, output = ezbox.delete_fib_entry(ip=server_ip, mask=mask)
			if result == False:
				print "ERROR, add fib entry was failed"
				print output
				exit(1)
				
			time.sleep(1)
			
	ezbox.flush_fib_entries()
	time.sleep(5)
	
#########################################################################################################################
########################### check longest prefix match ##################################################################
###### small mask match should drop the packet and the biggest mask on match will send the packet to second server) #####
#########################################################################################################################
	
	print "\nScenario 3 check longest prefix match"
	ezbox.execute_command_on_host("ipvsadm -d -t %s:80 -r %s:80"%(vip_list[0] ,server_list[0].ip))
	send_packet_and_check_servers(ezbox,
							  service_vip=vip_list[0], 
						  	  packet=data_packet.pcap_file_name, 
						  	  server_list = server_list, client_list = client_list, 
						  	  expected_packets = [0 , 0])
		
	time.sleep(16)
	ezbox.execute_command_on_host("ipvsadm -a -t %s:80 -r 192.168.254.254:80"%(vip_list[0]))
	ezbox.execute_command_on_host("arp -s %s %s"%(server_list[1].ip, server_list[1].mac_address)) #TODO check if sits mac address

	##########################################################################################
	########################### add neighbour fib entry ######################################
	##########################################################################################
	
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
	result,output = ezbox.add_fib_entry(ip="192.168.254.254", mask=32, gateway=server_list[1].ip)
	if result == False:
		print "ERROR, add fib entry was failed"
		print output
		exit(1)
		
	result,output = ezbox.execute_command_on_host("route")
	print output
	
	time.sleep(5)
	ezbox.get_ipvs_stats()				

	print server_list[1].ip
	send_packet_and_check_servers(ezbox,
							  service_vip=vip_list[0],
						  	  packet=data_packet.pcap_file_name, 
						  	  server_list = server_list, client_list = client_list, 
						  	  expected_packets = [0,1])

	ezbox.get_ipvs_stats()
	#remove FIB configuration
	ezbox.flush_fib_entries()
	ezbox.execute_command_on_host("ipvsadm -d -t %s:80 -r 192.168.254.254:80"%(vip_list[0]))
	send_packet_and_check_servers(ezbox,
							  service_vip=vip_list[0],
						  	  packet=data_packet.pcap_file_name, 
						  	  server_list = server_list, client_list = client_list, 
						  	  expected_packets = [0,0])
	time.sleep(20)
	ezbox.add_server(vip_list[0], port, server_list[0].ip, port, weight= 1)
				
##########################################################################################
########################### check modify fib entry ######################################
##########################################################################################
	print "\nScenario 4 - check modify fib entry"
	print "\nModify from neighbour to drop"
	# add a neighbour entry 
	print server_list[0].ip
	result,output = ezbox.add_fib_entry(ip=server_list[0].ip, mask=32)
	if result == False:
		print "ERROR, add fib entry was failed"
		print output
		exit(1)
			
	time.sleep(2)
	
	send_packet_and_check_servers(ezbox,
							  service_vip= vip_list[0], 
						  	  packet=data_packet.pcap_file_name, 
						  	  server_list = server_list, client_list = client_list,  
						  	  expected_packets = [1 , 0])
	
	# modify to drop entry
	result,output = ezbox.execute_command_on_host('ip route change blackhole ' + ' ' + server_list[0].ip + '/32')
	if result == False:
		print "ERROR, add fib entry was failed"
		print output
		exit(1)	 		
	
	time.sleep(2)
	
	send_packet_and_check_servers(ezbox,
							  service_vip= vip_list[0], 
						  	  packet=data_packet.pcap_file_name, 
						  	  server_list = server_list, client_list = client_list, 
						  	  expected_packets = [0 , 0])
 		
	# modify to gateway entry
	print "\nModify from drop to gateway"
	result,output = ezbox.execute_command_on_host('ip route change ' + ' ' + server_list[0].ip + '/32 via ' + server_list[1].ip)
	if result == False:
		print "ERROR, add fib entry was failed"
		print output
		exit(1)	 
			
	time.sleep(2)
	
	send_packet_and_check_servers(ezbox,
							  service_vip= vip_list[0], 
						  	  packet=data_packet.pcap_file_name, 
						  	  server_list = server_list, client_list = client_list, 
						  	  expected_packets = [0 , 1])
	
	print "\nModify from gateway to neighbour"
	result,output = ezbox.execute_command_on_host('ip route change ' + ' ' + server_list[0].ip + '/32 dev ' + ezbox.setup['interface'])
	if result == False:
		print "ERROR, add fib entry was failed"
		print output
		exit(1)	 
			
	time.sleep(2)
	
	send_packet_and_check_servers(ezbox,
							  service_vip= vip_list[0], 
						  	  packet=data_packet.pcap_file_name, 
						  	  server_list = server_list, client_list = client_list, 
						  	  expected_packets = [1 , 0])
	
	
	print "\nModify from neighbour to gateway"
	result,output = ezbox.execute_command_on_host('ip route change ' + ' ' + server_list[0].ip + '/32 via ' + server_list[1].ip)
	if result == False:
		print "ERROR, add fib entry was failed"
		print output
		exit(1)	 
			
	time.sleep(2)
	
	send_packet_and_check_servers(ezbox,
							  service_vip= vip_list[0], 
						  	  packet=data_packet.pcap_file_name, 
						  	  server_list = server_list, client_list = client_list,  
						  	  expected_packets = [0 , 1])
	
	print "\nModify from gateway to drop"
	result,output = ezbox.execute_command_on_host('ip route change blackhole' + ' ' + server_list[0].ip + '/32')
	if result == False:
		print "ERROR, add fib entry was failed"
		print output
		exit(1)	 
			
	time.sleep(2)
	
	send_packet_and_check_servers(ezbox,
							  service_vip= vip_list[0],
						  	  packet=data_packet.pcap_file_name, 
						  	  server_list = server_list, client_list = client_list,  
						  	  expected_packets = [0 , 0])
	
	
	print "\nModify from drop to neighbour"
	result,output = ezbox.execute_command_on_host('ip route change ' + ' ' + server_list[0].ip + '/32 dev ' + ezbox.setup['interface'])
	if result == False:
		print "ERROR, add fib entry was failed"
		print output
		exit(1)	 
			
	time.sleep(2)
	
	send_packet_and_check_servers(ezbox,
							  service_vip= vip_list[0], 
						  	  packet=data_packet.pcap_file_name, 
						  	  server_list = server_list, client_list = client_list, 
						  	  expected_packets = [1 , 0])
	
	#remove FIB configuration
	result, output = ezbox.delete_fib_entry(ip=server_list[0].ip, mask=32)	
	if result == False:
		print "ERROR, add fib entry was failed"
		print output
		exit(1)
		
	ezbox.flush_fib_entries()
##########################################################################################
########################### add max entries to fib table #################################
##########################################################################################
# 	print "\nScenario 5 - add max entries to fib table"
# 	ezbox.flush_fib_entries()
# 	time.sleep(5)
# 	
# 	result,output = ezbox.execute_command_on_host("route | wc -l")
# 	if result == False:
# 		print "ERROR, when executing command on host"
# 		print output
# 		exit(1)
# 	fib_entries_on_system = int(output) - 2
# 		
# 	
# 	# create drop entries
# 	i=0
# 	print "Creating %d FIB entries - %d fib entries already exist on system"%((8191-fib_entries_on_system),fib_entries_on_system)
# 	while i < 8191-fib_entries_on_system:
# 		
# 		sys.stdout.write('Creating fib entry number %d\r'%i)
# 		sys.stdout.flush()
# 		
# 		server_ip = '192.168.%d.%d'%(random.randint(2,255),random.randint(2,255))
# 		mask=random.randint(20,31)
# 		server_ip=mask_ip(server_ip, mask)
# 		result,output = ezbox.add_fib_entry(ip=server_ip, mask=mask, gateway= server_list[1].ip)
# 		if result == False:
# 			continue
# 		i+=1
# 		time.sleep(0.05)
# 		
# 	
#  	result,output = ezbox.add_fib_entry(ip=server_list[0].ip, mask=32)
# 	if result == False:
# 		print "ERROR, add fib entry was failed"
# 		print output
# 		ezbox.flush_fib_entries()
# 		exit(1)
# 		
# 	send_packet_and_check_servers(ezbox,
# 							  service_vip= vip_list[0], 
# 						  	  packet=data_packet.pcap_file_name, 
# 						  	  server_list = server_list, client_list = client_list, 
# 						  	  expected_packets = [1 , 0])
# 
# 	print "\nCreating 32 mask entry - after reaching the max entries on system"
# 	
# 	if ezbox.check_syslog_message("<notice>  Problem adding FIB entry:") == True or \
# 		ezbox.check_syslog_message("<err>  Can't add FIB entry") == True or			\
# 		ezbox.check_syslog_message("out of memory") == True:
# 		
# 		print "ERROR, syslog error message was found"
# 		ezbox.flush_fib_entries()
# 		exit(1)
# 		
# 	server_ip = '192.168.1.1'
# 	result,output = ezbox.add_fib_entry(ip=server_ip, mask=32, gateway=server2.data_ip)
# 	if result == False:
# 		print "ERROR, add fib entry was failed"
# 		print output
# 		ezbox.flush_fib_entries()
# 		exit(1)
# 		
# 	if ezbox.check_syslog_message("<notice>  Problem adding FIB entry:") == True and \
# 		ezbox.check_syslog_message("<err>  Can't add FIB entry") == True and 		 \
# 		ezbox.check_syslog_message("out of memory") == True:
# 		
# 		print "Received error message on syslog - reached to the max num of FIB entries"
# 	else:
# 		print "ERROR, didnt receive error message on syslog"
# 		ezbox.flush_fib_entries()
# 		exit(1)
# 	
# 	ezbox.flush_fib_entries()
# 	
# 	print "\nTest Passed"




# 	# check flush entries - TODO
# 	for i in range(8000):
# 		server_ip = add2ip('192.168.100.1',i*2)
# 		result,output = ezbox.delete_fib_entry(ip=server_ip, mask=31, gateway=server2.data_ip)
# 		if result == False:
# 			print "ERROR, add fib entry was failed"
# 			print output
# 			exit(1)
# 			
# 	result,output = ezbox.delete_fib_entry(ip=server1.data_ip, mask=32, gateway=server2.data_ip)
# 	if result == False:
# 		print "ERROR, add fib entry was failed"
# 		print output
# 		exit(1)
# 		
# 		
# 	ezbox.flush_fib_entries()	
		
	### check if no match statistics was added

main()
