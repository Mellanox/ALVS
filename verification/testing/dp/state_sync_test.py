#!/usr/bin/env python


#===============================================================================
# imports
#===============================================================================

# system  
import sys
import random
import re
import time


# pythons modules 
# local
sys.path.append("verification/testing")
from test_infra import *


###########################################################################################################################################################
#l3 & above header functions
###########################################################################################################################################################
#Calculates and returns the IP checksum based on the given IP Header
def splitN(str1,n):
	return [str1[start:start+n] for start in range(0, len(str1), n)]

def ip_checksum(iphdr):
	words = splitN(''.join(iphdr.split()),4)
	csum = 0;
	for word in words:
		csum += int(word, base=16)
	csum += (csum >> 16)
	csum = csum & 0xFFFF ^ 0xFFFF
	return csum


def get_ip_header(tot_len, protocol, sip):
	header = ('45'
	  '00'
	  'XXXX'			#Length - will be calculated and replaced later
	  '0000'
	  '0000ff'
	  'PP'				#PP = Protocol
	  'CCCC'			#Checksum - will be calculated and replaced later	  
	  'YYYYYYYY'		#Source IP 
	  'E0000051')		#Dest IP 224.0.0.81
	header = header.replace('PP',"%02x"%protocol)
	header = header.replace('XXXX',"%04x"%tot_len)
	header = header.replace('YYYYYYYY', sip)
	header = header.replace('CCCC',"%04x"%ip_checksum(header.replace('CCCC','0000')))
	return header

def get_eth_type():
	return ('0800')

def get_multicast_eth_header():
	return ('01005e000051aabbccddeeff')

def get_udp_hdr(tot_len, port):
	header = ('PPPP'	#sourcestate sync port
	  '2290'			# SS port	  
	  'LLLL'			#Length - will be calculated and replaced later
	  '0000')			#CHECKSUM - unused 
	header = header.replace('PPPP',"%04x"%port)
	header = header.replace('LLLL',"%04x"%tot_len)
	return header


#===============================================================================
# State sync
#===============================================================================
def get_ss_hdr(tot_len, syncid, conn_count):
	header = ('00'
	  'SS'				# SYNC ID	  
	  'LLLL'			#Length - will be calculated and replaced later
	  'NN'				#number of connections
	  '01'				#Version				
	  '0000')
	header = header.replace('SS',"%02x"%syncid)
	header = header.replace('LLLL',"%04x"%tot_len)
	header = header.replace('NN',"%02x"%conn_count)
	return header


def get_ss_conn(protocol, flags, state, cport, vport, dport, fwmark, timeout, caddr, vaddr, daddr):
	header = ('00'		#type = ipv4
	  'PP'				#protocol					  
	  '0025'			#ver_size - version is 0 and length is 36B (no optional params)
	  'FFFFFFFF'		#number of connections
	  'SSSS'			#State				
	  'XXXX'			#Client port
	  'YYYY'			#Virtual port
	  'ZZZZ'			#Server port
	  'MMMMMMMM'		#fwmark
	  'TTTTTTTT'		#timeout
	  'AAAAAAAA'		#client ip
	  'BBBBBBBB'		#Virtual ip
	  'CCCCCCCC'		#Server ip
	  )					#Client port
	
	header = header.replace('PP',"%02x"%protocol)
	header = header.replace('FFFFFFFF',"%08x"%flags)
	header = header.replace('SSSS',"%04x"%state)
	header = header.replace('XXXX',"%04x"%cport)
	header = header.replace('YYYY',"%04x"%vport)
	header = header.replace('ZZZZ',"%04x"%dport)
	header = header.replace('MMMMMMMM',"%08x"%fwmark)
	header = header.replace('TTTTTTTT',"%08x"%timeout)
	header = header.replace('AAAAAAAA', caddr)
	header = header.replace('BBBBBBBB', vaddr)
	header = header.replace('CCCCCCCC', daddr)
	return header


#===============================================================================
# Test Globals
#===============================================================================
client_count = 1
server_count = 3
service_count = 2

#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

def init_log(args):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	log_file = "scheduling_algorithm_test.log"
	if 'log_file' in args:
		log_file = args['log_file']
	init_logging(log_file)


def user_init(setup_num):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	vip_list = [get_setup_vip(setup_num,i) for i in range(service_count)]
	
	setup_list = get_setup_list(setup_num)
	
	index = 0
	server_list=[]
	for i in range(server_count):
		server_list.append(real_server(management_ip=setup_list[index]['hostname'], data_ip=setup_list[index]['ip']))
		index+=1

	client_list=[]
	for i in range(client_count):
		client_list.append(client(management_ip=setup_list[index]['hostname'], data_ip=setup_list[index]['ip']))
		index+=1
	
	# EZbox
	ezbox = ezbox_host(setup_num)
	
	return (client_list, server_list, ezbox, vip_list)


def init_ezbox(args,ezbox):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	if args['hard_reset']:
		ezbox.reset_ezbox()
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


def test_2():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	rc = ezbox.start_state_sync_daemon(state = "backup", syncid = 1)
	if rc == False:
		print "ERROR: Can't start backup state sync daemon with syncid 1"
		return False
	time.sleep(1)
	
	# create packet
	conn = get_ss_conn(6, 0x100, 7, 12304, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].data_ip)))[2:])
	# syncid = 2
	ss_hdr = get_ss_hdr(36+8, 2, 1)
	udp_hdr = get_udp_hdr(36+8+8, 1234)
	ip_hdr = get_ip_header(36+8+8+20, 17, '0a9d0701')
	eth = get_eth_type()
	eth_hdr = get_multicast_eth_header()
	
	packet = eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr + conn

	string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file='verification/testing/dp/temp_packet.pcap')
	
	client.send_packet_to_nps('verification/testing/dp/temp_packet.pcap')

	time.sleep(1)
	
	conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, 12304, 6)
	
	rc = ezbox.stop_state_sync_daemon(state = "backup")
	if rc == False:
		print "ERROR: Can't stop backup state sync daemon"
		return False
	
	if conn != None:
		print "ERROR, connection was created for wrong SyncID message"
		return False		
	
	return True


def test_3():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	# create packet
	conn = get_ss_conn(6, 0x100, 7, 12303, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].data_ip)))[2:])
	ss_hdr = get_ss_hdr(36+8, 2, 1)
	udp_hdr = get_udp_hdr(36+8+8, 1234)
	ip_hdr = get_ip_header(36+8+8+20, 17, '0a9d0701')
	eth = get_eth_type()
	eth_hdr = get_multicast_eth_header()
	
	packet = eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr + conn

	string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file='verification/testing/dp/temp_packet.pcap')
	client.send_packet_to_nps('verification/testing/dp/temp_packet.pcap')

	time.sleep(1)

	conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, 12303, 6)

	if conn != None:
		print "ERROR, connection was created even when backup sync daemon wasn't configured."
		return False		
	
	return True

def test_4():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	rc = ezbox.start_state_sync_daemon(state = "backup", syncid = 1)
	if rc == False:
		print "ERROR: Can't start backup state sync daemon with syncid 1"
		return False
	time.sleep(1)
	
	# create packet
	conn = get_ss_conn(6, 0x100, 7, 12304, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].data_ip)))[2:])
	ss_hdr = get_ss_hdr(36+8, 1, 1)
	udp_hdr = get_udp_hdr(36+8+8, 1234)
	ip_hdr = get_ip_header(36+8+8+20, 17, '0a9d0701')
	eth = get_eth_type()
	eth_hdr = get_multicast_eth_header()
	
	packet = eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr + conn

	string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file='verification/testing/dp/temp_packet.pcap')
	
	client.send_packet_to_nps('verification/testing/dp/temp_packet.pcap')

	time.sleep(1)
	
	conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, 12304, 6)
	
	rc = ezbox.stop_state_sync_daemon(state = "backup")
	if rc == False:
		print "ERROR: Can't stop backup state sync daemon"
		return False
	
	if conn == None:
		print "ERROR, connection wasn't created"
		return False
	
	if conn['bound'] != 1 or conn['server'] != 0 or conn['state'] != 7 or conn['flags'] != 0x120:
		print "ERROR, connection was created, but not with correct parameters"
		print conn
		return False
	
	return True

def test_5():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	rc = ezbox.start_state_sync_daemon(state = "backup")
	if rc == False:
		print "ERROR: Can't start backup state sync daemon with syncid 0"
		return False
	time.sleep(1)

	# create packet
	conn = get_ss_conn(6, 0x100, 7, 12305, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[1])))[2:], str(hex(ip2int(server_list[0].data_ip)))[2:])
	ss_hdr = get_ss_hdr(36+8, 1, 1)
	udp_hdr = get_udp_hdr(36+8+8, 1234)
	ip_hdr = get_ip_header(36+8+8+20, 17, '0a9d0701')
	eth = get_eth_type()
	eth_hdr = get_multicast_eth_header()

	packet = eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr + conn

	string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file='verification/testing/dp/temp_packet.pcap')
	client.send_packet_to_nps('verification/testing/dp/temp_packet.pcap')

	time.sleep(1)

	conn = ezbox.get_connection(ip2int(vip_list[1]), 80, 0x0a9d0701, 12305, 6)
	
	rc = ezbox.stop_state_sync_daemon(state = "backup")
	if rc == False:
		print "ERROR: Can't stop backup state sync daemon"
		return False
	
	if conn == None:
		print "ERROR, connection wasn't created"
		return False
	
	if conn['bound'] != 0 or conn['server'] != ip2int(server_list[0].data_ip) or conn['state'] != 7 or conn['flags'] != 0x120:
		print "ERROR, connection was created, but not with correct parameters"
		print conn
		return False
	
	return True

def test_6_9():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	rc = ezbox.start_state_sync_daemon(state = "backup", syncid = 0)
	if rc == False:
		print "ERROR: Can't start backup state sync daemon with syncid 0"
		return False
	time.sleep(1)
	# create packet
	conn0 = get_ss_conn(6, 0x100, 7, 12306, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].data_ip)))[2:])
	conn1 = get_ss_conn(6, 0x0, 1, 12306, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].data_ip)))[2:])
	ss_hdr = get_ss_hdr(36*2+8, 4, 2)
	udp_hdr = get_udp_hdr(36*2+8+8, 1234)
	ip_hdr = get_ip_header(36*2+8+8+20, 17, '0a9d0701')
	eth = get_eth_type()
	eth_hdr = get_multicast_eth_header()
	
	packet = eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr + conn0 + conn1

	string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file='verification/testing/dp/temp_packet.pcap')
	client.send_packet_to_nps('verification/testing/dp/temp_packet.pcap')

	time.sleep(1)
	
	conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, 12306, 6)
	
	rc = ezbox.stop_state_sync_daemon(state = "backup")
	if rc == False:
		print "ERROR: Can't stop backup state sync daemon"
		return False
	
	if conn == None:
		print "ERROR, connection wasn't created"
		return False
	
	if conn['bound'] != 1 or conn['server'] != 0 or conn['state'] != 1 or conn['flags'] != 0x20:
		print "ERROR, connection was created, but not with correct parameters"
		print conn
		return False
	
	return True

def test_7():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	syncid = 7
	rc = ezbox.start_state_sync_daemon(state = "backup", syncid = syncid)
	if rc == False:
		print "ERROR: Can't start backup state sync daemon with syncid %d" %syncid
		return False
	time.sleep(1)
	# create packet
	conn = get_ss_conn(6, 0x0, 1, 12307, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[1])))[2:], str(hex(ip2int(server_list[0].data_ip)))[2:])
	ss_hdr = get_ss_hdr(36+8, syncid, 1)
	udp_hdr = get_udp_hdr(36+8+8, 1234)
	ip_hdr = get_ip_header(36+8+8+20, 17, '0a9d0701')
	eth = get_eth_type()
	eth_hdr = get_multicast_eth_header()
	
	packet = eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr + conn

	string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file='verification/testing/dp/temp_packet.pcap')
	client.send_packet_to_nps('verification/testing/dp/temp_packet.pcap')

	time.sleep(1)
	
	# server not exists
	service1.add_server(server_list[0])
	
	conn = get_ss_conn(6, 0x100, 7, 12307, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[1])))[2:], str(hex(ip2int(server_list[0].data_ip)))[2:])
	ss_hdr = get_ss_hdr(36+8, syncid, 1)
	udp_hdr = get_udp_hdr(36+8+8, 1234)
	ip_hdr = get_ip_header(36+8+8+20, 17, '0a9d0701')
	eth = get_eth_type()
	eth_hdr = get_multicast_eth_header()
	
	packet = eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr + conn

	string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file='verification/testing/dp/temp_packet.pcap')
	client.send_packet_to_nps('verification/testing/dp/temp_packet.pcap')

	time.sleep(1)
	
	
	conn = ezbox.get_connection(ip2int(vip_list[1]), 80, 0x0a9d0701, 12307, 6)
	
	rc = ezbox.stop_state_sync_daemon(state = "backup")
	if rc == False:
		print "ERROR: Can't stop backup state sync daemon"
		return False
	
	if conn == None:
		print "ERROR, connection wasn't created"
		return False
	
	if conn['bound'] != 1 or conn['server'] != 3 or conn['state'] != 7 or conn['flags'] != 0x120:
		print "ERROR, connection was created, but not with correct parameters"
		print conn
		return False
	
	return True

def test_8():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	rc = ezbox.start_state_sync_daemon(state = "backup", syncid = 8)
	if rc == False:
		print "ERROR: Can't start backup state sync daemon with syncid 8"
		return False
	time.sleep(1)
	
	# create packet
	conn0 = get_ss_conn(6, 0x0, 1, 12308, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[1])))[2:], str(hex(ip2int(server_list[1].data_ip)))[2:])
	conn1 = get_ss_conn(6, 0x100, 7, 12308, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[1])))[2:], str(hex(ip2int(server_list[1].data_ip)))[2:])
	ss_hdr = get_ss_hdr(36*2+8, 8, 2)
	udp_hdr = get_udp_hdr(36*2+8+8, 1234)
	ip_hdr = get_ip_header(36*2+8+8+20, 17, '0a9d0701')
	eth = get_eth_type()
	eth_hdr = get_multicast_eth_header()
	
	packet = eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr + conn0 + conn1

	string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file='verification/testing/dp/temp_packet.pcap')
	client.send_packet_to_nps('verification/testing/dp/temp_packet.pcap')

	time.sleep(1)
	
	conn = ezbox.get_connection(ip2int(vip_list[1]), 80, 0x0a9d0701, 12308, 6)
	
	rc = ezbox.stop_state_sync_daemon(state = "backup")
	if rc == False:
		print "ERROR: Can't stop backup state sync daemon"
		return False
	
	if conn == None:
		print "ERROR, connection wasn't created"
		return False
	
	if conn['bound'] != 1 or conn['server'] != 2 or conn['state'] != 7 or conn['flags'] != 0x120:
		print "ERROR, connection was created, but not with correct parameters"
		print conn
		return False
	
	return True

def test_10():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	rc = ezbox.start_state_sync_daemon(state = "backup", syncid = 1)
	if rc == False:
		print "ERROR: Can't start backup state sync daemon with syncid 1"
		return False
	time.sleep(1)

	# create packet
	conn0 = get_ss_conn(6, 0x0, 1, 12310, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].data_ip)))[2:])
	conn1 = get_ss_conn(6, 0x0, 1, 12310, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[1].data_ip)))[2:])
	ss_hdr = get_ss_hdr(36*2+8, 1, 2)
	udp_hdr = get_udp_hdr(36*2+8+8, 1234)
	ip_hdr = get_ip_header(36*2+8+8+20, 17, '0a9d0701')
	eth = get_eth_type()
	eth_hdr = get_multicast_eth_header()
	
	packet = (eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr + conn0 + conn1)

	string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file='verification/testing/dp/temp_packet.pcap')
	client.send_packet_to_nps('verification/testing/dp/temp_packet.pcap')

	time.sleep(1)
	
	conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, 12310, 6)
	
	rc = ezbox.stop_state_sync_daemon(state = "backup")
	if rc == False:
		print "ERROR: Can't stop backup state sync daemon"
		return False
	
	if conn == None:
		print "ERROR, connection wasn't created"
		return False
	
	if conn['bound'] != 1 or conn['server'] != 1 or conn['state'] != 1 or conn['flags'] != 0x20:
		print "ERROR, connection was created, but not with correct parameters"
		print conn
		return False
	
	return True

def test_11():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	rc = ezbox.start_state_sync_daemon(state = "backup", syncid = 11)
	if rc == False:
		print "ERROR: Can't start backup state sync daemon with syncid 11"
		return False
	time.sleep(1)
	
	# create packet
	conn0 = get_ss_conn(6, 0x100, 7, 12311, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].data_ip)))[2:])
	conn1 = get_ss_conn(6, 0x100, 7, 12311, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[1].data_ip)))[2:])
	ss_hdr = get_ss_hdr(36*2+8, 11, 2)
	udp_hdr = get_udp_hdr(36*2+8+8, 1234)
	ip_hdr = get_ip_header(36*2+8+8+20, 17, '0a9d0701')
	eth = get_eth_type()
	eth_hdr = get_multicast_eth_header()
	
	packet = eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr + conn0 + conn1

	string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file='verification/testing/dp/temp_packet.pcap')
	client.send_packet_to_nps('verification/testing/dp/temp_packet.pcap')

	time.sleep(1)
	
	conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, 12311, 6)
	
	rc = ezbox.stop_state_sync_daemon(state = "backup")
	if rc == False:
		print "ERROR: Can't stop backup state sync daemon"
		return False
	
	if conn == None:
		print "ERROR, connection wasn't created"
		return False
	
	if conn['bound'] != 1 or conn['server'] != 0 or conn['state'] != 7 or conn['flags'] != 0x120:
		print "ERROR, connection was created, but not with correct parameters"
		print conn
		return False
	
	return True

def test_12():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	rc = ezbox.start_state_sync_daemon(state = "backup", syncid = 12)
	if rc == False:
		print "ERROR: Can't start backup state sync daemon with syncid 12"
		return False
	time.sleep(1)

	# create packet
	ss_hdr = get_ss_hdr(36*10+8, 12, 10)
	udp_hdr = get_udp_hdr(36*10+8+8, 1234)
	ip_hdr = get_ip_header(36*10+8+8+20, 17, '0a9d0701')
	eth = get_eth_type()
	eth_hdr = get_multicast_eth_header()
	
	packet = eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr
	
	for port in [10,20,30]:
		conn = get_ss_conn(6, 0x100, 7, port, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].data_ip)))[2:])
		packet += conn
	for port in [11,21,31]:
		conn = get_ss_conn(6, 0x0, 1, port, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[1].data_ip)))[2:])
		packet += conn
	for port in [12,22,32]:
		conn = get_ss_conn(6, 0x100, 7, port, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[2].data_ip)))[2:])
		packet += conn
	for port in [13]:
		conn = get_ss_conn(6, 0x0, 1, port, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[1])))[2:], str(hex(ip2int(server_list[1].data_ip)))[2:])
		packet += conn
		
	string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file='verification/testing/dp/temp_packet.pcap')
	client.send_packet_to_nps('verification/testing/dp/temp_packet.pcap')

	time.sleep(2)
	
	rc = ezbox.stop_state_sync_daemon(state = "backup")
	if rc == False:
		print "ERROR: Can't stop backup state sync daemon"
		return False
	
	for port in [10,20,30]:
		conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, port, 6)
	
		if conn == None:
			print "ERROR, connection wasn't created"
			return False
		
		if conn['bound'] != 1 or conn['server'] != 0 or conn['state'] != 7 or conn['flags'] != 0x120:
			print "ERROR, connection was created, but not with correct parameters"
			print conn
			return False

	for port in [11,21,31]:
		conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, port, 6)
	
		if conn == None:
			print "ERROR, connection wasn't created"
			return False
		
		if conn['bound'] != 1 or conn['server'] != 1 or conn['state'] != 1 or conn['flags'] != 0x20:
			print "ERROR, connection was created, but not with correct parameters"
			print conn
			return False
		
	for port in [12,22,32]:
		conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, port, 6)
	
		if conn == None:
			print "ERROR, connection wasn't created"
			return False
		
		if conn['bound'] != 0 or conn['server'] != ip2int(server_list[2].data_ip) or conn['state'] != 7 or conn['flags'] != 0x120:
			print "ERROR, connection was created, but not with correct parameters"
			print conn
			return False

	for port in [13]:
		conn = ezbox.get_connection(ip2int(vip_list[1]), 80, 0x0a9d0701, port, 6)
	
		if conn == None:
			print "ERROR, connection wasn't created"
			return False
		
		if conn['bound'] != 1 or conn['server'] != 2 or conn['state'] != 1 or conn['flags'] != 0x20:
			print "ERROR, connection was created, but not with correct parameters"
			print conn
			return False
	
	return True

def test_13():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	rc = ezbox.start_state_sync_daemon(state = "backup", syncid = 13)
	if rc == False:
		print "ERROR: Can't start backup state sync daemon with syncid 13"
		return False
	time.sleep(1)
	# create packet
	ss_hdr = get_ss_hdr(36*9+8, 13, 9)
	udp_hdr = get_udp_hdr(36*9+8+8, 1234)
	ip_hdr = get_ip_header(36*9+8+8+20, 17, '0a9d0701')
	eth = get_eth_type()
	eth_hdr = get_multicast_eth_header()
	
	packet = (eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr)
	
	conn = get_ss_conn(6, 0x0, 1, 10, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].data_ip)))[2:])
	packet += conn
	
	conn = get_ss_conn(6, 0x100, 7, 20, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[1].data_ip)))[2:])
	packet += conn

	conn = get_ss_conn(6, 0x100, 7, 30, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[2].data_ip)))[2:])
	packet += conn

	conn = get_ss_conn(6, 0x100, 7, 11, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[1].data_ip)))[2:])
	packet += conn

	conn = get_ss_conn(6, 0x0, 1, 21, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].data_ip)))[2:])
	packet += conn

	conn = get_ss_conn(6, 0x0, 1, 31, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[2].data_ip)))[2:])
	packet += conn	

	conn = get_ss_conn(6, 0x0, 1, 12, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[2].data_ip)))[2:])
	packet += conn

	conn = get_ss_conn(6, 0x100, 7, 22, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].data_ip)))[2:])
	packet += conn

	conn = get_ss_conn(6, 0x100, 7, 32, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[1].data_ip)))[2:])
	packet += conn
		
	string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file='verification/testing/dp/temp_packet.pcap')
	client.send_packet_to_nps('verification/testing/dp/temp_packet.pcap')

	time.sleep(2)
	
	rc = ezbox.stop_state_sync_daemon(state = "backup")
	if rc == False:
		print "ERROR: Can't stop backup state sync daemon"
		return False
	
	for port in [10,21]:
		conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, port, 6)
	
		if conn == None:
			print "ERROR, connection wasn't created"
			return False
		
		if conn['bound'] != 1 or conn['server'] != 0 or conn['state'] != 1 or conn['flags'] != 0x20:
			print "ERROR, connection (cport=%d) was created, but not with correct parameters"%port
			print conn
			return False

	for port in [11]:
		conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, port, 6)
	
		if conn == None:
			print "ERROR, connection wasn't created"
			return False
	
		if conn['bound'] != 1 or conn['server'] != 1 or conn['state'] != 7 or conn['flags'] != 0x120:
			print "ERROR, connection (cport=%d) was created, but not with correct parameters"%port
			print conn
			return False

	for port in [22, 32]:
		conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, port, 6)
	
		if conn == None:
			print "ERROR, connection wasn't created"
			return False
		
		if conn['bound'] != 0 or conn['server'] != ip2int(server_list[2].data_ip) or conn['state'] != 7 or conn['flags'] != 0x120:
			print "ERROR, connection (cport=%d) was created, but not with correct parameters"%port
			print conn
			return False

	for port in [31, 12]:
		conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, port, 6)
	
		if conn == None:
			print "ERROR, connection wasn't created"
			return False

		if conn['bound'] != 0 or conn['server'] != ip2int(server_list[2].data_ip) or conn['state'] != 1 or conn['flags'] != 0x20:
			print "ERROR, connection (cport=%d) was created, but not with correct parameters"%port
			print conn
			return False

	for port in [20, 30]:
		conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, port, 6)
	
		if conn == None:
			print "ERROR, connection wasn't created"
			return False
		
		if conn['bound'] != 1 or conn['server'] != 0 or conn['state'] != 7 or conn['flags'] != 0x120:
			print "ERROR, connection (cport=%d) was created, but not with correct parameters"%port
			print conn
			return False

	for port in [13]:
		conn = ezbox.get_connection(ip2int(vip_list[1]), 80, 0x0a9d0701, port, 6)
	
		if conn == None:
			print "ERROR, connection wasn't created"
			return False
		
		if conn['bound'] != 1 or conn['server'] != 2 or conn['state'] != 1 or conn['flags'] != 0x20:
			print "ERROR, connection (cport=%d) was created, but not with correct parameters"%port
			print conn
			return False
	
	return True


def clear_test(ezbox):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	print "Test clear ezbox test"
	ezbox.flush_ipvs()
	time.sleep(1)
	if ezbox.get_num_of_services()!= 0:
		print "after FLUSH all, num_of_services should be 0\n"
		return False
	print "Cleaning EZbox..."
	ezbox.clean(use_director=False, stop_ezbox=True)
	return True
	

#===============================================================================
# main function
#===============================================================================
args = read_test_arg(sys.argv)	
init_log(args)
client_list, server_list, ezbox, vip_list = user_init(args['setup_num'])
init_ezbox(args,ezbox)

client = client_list[0]
service0 = service(ezbox=ezbox, virtual_ip=vip_list[0], port='80', schedule_algorithm = 'source_hash')
service1 = service(ezbox=ezbox, virtual_ip=vip_list[1], port='80', schedule_algorithm = 'source_hash')
service0.add_server(server_list[0])
service0.add_server(server_list[1])
service1.add_server(server_list[1])
	
failed_tests = 0

print "\nTest 2 - check dropping packet with syncid 2 while backup was started with syncid 1"
rc = test_2()
if rc == False:
	print 'Test2 failed !!!\n'
	failed_tests += 1
else:
	print 'Test2 passed !!!\n'

print "Test 3 - Backup is not started"
rc = test_3()
if rc == False:
	print 'Test3 failed !!!\n'
	failed_tests += 1
else:
	print 'Test3 passed !!!\n'

print "Test 4 - Create a new connection (bound)"
print "In addition Test 4 check receiving packet with syncid 1 while backup was started with syncid 1"
rc = test_4()
if rc == False:
	print 'Test4 failed !!!\n'
	failed_tests += 1
else:
	print 'Test4 passed !!!\n'

print "Test 5 - Create a new connection (unbound)"
print "In addition Test 5 check receiving packet with syncid 1 while backup was started with syncid 0"
rc = test_5()
if rc == False:
	print 'Test5 failed !!!\n'
	failed_tests += 1
else:
	print 'Test5 passed !!!\n'

print "Test 6_9 - Update a connection (bound) with new state"
print "In addition Test 6_9 check receiving packet with syncid 4 while backup was started with syncid 0"
rc = test_6_9()
if rc == False:
	print 'Test6_9 failed !!!\n'
	failed_tests += 1
else:
	print 'Test6_9 passed !!!\n'

print "Test 7 - "
rc = test_7()
if rc == False:
	print 'Test7 failed !!!\n'
	failed_tests += 1
else:
	print 'Test7 passed !!!\n'

print "Test 8 - "
rc = test_8()
if rc == False:
	print 'Test8 failed !!!\n'
	failed_tests += 1
else:
	print 'Test8 passed !!!\n'

print "Test 10 - "
rc = test_10()
if rc == False:
	print 'Test10 failed !!!\n'
	failed_tests += 1
else:
	print 'Test10 passed !!!\n'

print "Test 11 - "
rc = test_11()
if rc == False:
	print 'Test11 failed !!!\n'
	failed_tests += 1
else:
	print 'Test11 passed !!!\n'

print "Test 12 - "
rc = test_12()
if rc == False:
	print 'Test12 failed !!!\n'
	failed_tests += 1
else:
	print 'Test12 passed !!!\n'

print "Test 13 - "
rc = test_13()
if rc == False:
	print 'Test13 failed !!!\n'
	failed_tests += 1
else:
	print 'Test13 passed !!!\n'

rc = clear_test(ezbox)
if rc == False:
	print 'Clear test failed !!!\n'
	failed_tests += 1
else:
	print 'Clear test passed !!!\n'

if failed_tests == 0:
	print 'ALL Tests were passed !!!'
	exit(0)
else:
	print 'Number of failed tests: %d' %failed_tests
	exit(1)
