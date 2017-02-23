#!/usr/bin/env python


#===============================================================================
# imports
#===============================================================================

# system  
import random
import re
import time

# pythons modules 
# local
import sys
import os
import inspect
my_currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
my_parentdir = os.path.dirname(my_currentdir)
my_grandparentdir =  os.path.dirname(my_parentdir)
sys.path.append(my_grandparentdir)
sys.path.append(my_parentdir)
from common_infra import *
from alvs_infra import *
from unit_tester import Unit_Tester
from alvs_players_factory import *

server_count   = 5
client_count   = 1
service_count  = 2

class State_Sync_Test(Unit_Tester):

	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = ALVS_Players_Factory.generic_init(setup_num, service_count, server_count, client_count)
		
		w = 1
		for i,s in enumerate(self.test_resources['server_list']):
			if i<2:
				s.vip = self.test_resources['vip_list'][0]
				s.weight = w
			else:
				# the last 2 servers will not be added to any service eventhough it has a vip, for this test purpose.
				s.vip = self.test_resources['vip_list'][1]
				s.weight = w
	
	def change_config(self, config):
		config['start_ezbox'] = True
	
	###########################################################################################################################################################
	#l3 & above header functions
	###########################################################################################################################################################
	#Calculates and returns the IP checksum based on the given IP Header
	def splitN(self, str1,n):
		return [str1[start:start+n] for start in range(0, len(str1), n)]
	
	def ip_checksum(self, iphdr):
		words = self.splitN(''.join(iphdr.split()),4)
		csum = 0;
		for word in words:
			csum += int(word, base=16)
		csum += (csum >> 16)
		csum = csum & 0xFFFF ^ 0xFFFF
		return csum
	
	
	def get_ip_header(self, tot_len, protocol, sip):
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
		header = header.replace('CCCC',"%04x"%self.ip_checksum(header.replace('CCCC','0000')))
		return header
	
	def get_eth_type(self):
		return ('0800')
	
	def get_multicast_eth_header(self):
		return ('01005e000051aabbccddeeff')
	
	def get_udp_hdr(self, tot_len, port):
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
	def get_ss_hdr(self, tot_len, syncid, conn_count):
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
	
	
	def get_ss_conn(self, protocol, flags, state, cport, vport, dport, fwmark, timeout, caddr, vaddr, daddr):
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
	
	def test_2(self, server_list, ezbox, client_list, vip_list):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		rc = ezbox.start_state_sync_daemon(state = "backup", syncid = 1)
		if rc == False:
			print "ERROR: Can't start backup state sync daemon with syncid 1"
			return False
		time.sleep(1)
		
		# create packet
		conn = self.get_ss_conn(6, 0x100, 7, 12304, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].ip)))[2:])
		# syncid = 2
		ss_hdr = self.get_ss_hdr(36+8, 2, 1)
		udp_hdr = self.get_udp_hdr(36+8+8, 1234)
		ip_hdr = self.get_ip_header(36+8+8+20, 17, '0a9d0701')
		eth = self.get_eth_type()
		eth_hdr = self.get_multicast_eth_header()
		
		packet = eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr + conn
	
		string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file=ALVSdir + '/verification/testing/dp/temp_packet.pcap')
		
		client_list[0].send_packet_to_nps(ALVSdir + '/verification/testing/dp/temp_packet.pcap')
	
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
	
	
	def test_3(self, server_list, ezbox, client_list, vip_list):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
		# create packet
		conn = self.get_ss_conn(6, 0x100, 7, 12303, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].ip)))[2:])
		ss_hdr = self.get_ss_hdr(36+8, 2, 1)
		udp_hdr = self.get_udp_hdr(36+8+8, 1234)
		ip_hdr = self.get_ip_header(36+8+8+20, 17, '0a9d0701')
		eth = self.get_eth_type()
		eth_hdr = self.get_multicast_eth_header()
		
		packet = eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr + conn
	
		string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file=ALVSdir + '/verification/testing/dp/temp_packet.pcap')
		client_list[0].send_packet_to_nps(ALVSdir + '/verification/testing/dp/temp_packet.pcap')
	
		time.sleep(1)
	
		conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, 12303, 6)
	
		if conn != None:
			print "ERROR, connection was created even when backup sync daemon wasn't configured."
			return False		
		
		return True
	
	def test_4(self, server_list, ezbox, client_list, vip_list):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		port = '80'
		rc = ezbox.start_state_sync_daemon(state = "backup", syncid = 1)
		if rc == False:
			print "ERROR: Can't start backup state sync daemon with syncid 1"
			return False
		time.sleep(1)
		
		# create packet
		conn = self.get_ss_conn(6, 0x100, 7, 12304, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].ip)))[2:])
		ss_hdr = self.get_ss_hdr(36+8, 1, 1)
		udp_hdr = self.get_udp_hdr(36+8+8, 1234)
		ip_hdr = self.get_ip_header(36+8+8+20, 17, '0a9d0701')
		eth = self.get_eth_type()
		eth_hdr = self.get_multicast_eth_header()
		
		packet = eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr + conn
	
		string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file=ALVSdir + '/verification/testing/dp/temp_packet.pcap')
		
		client_list[0].send_packet_to_nps(ALVSdir + '/verification/testing/dp/temp_packet.pcap')
	
		time.sleep(1)
		
		conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, 12304, 6)
		
		rc = ezbox.stop_state_sync_daemon(state = "backup")
		server0 = ezbox.get_server(server_list[0].vip, port, server_list[0].ip, port, 6)
		if rc == False:
			print "ERROR: Can't stop backup state sync daemon"
			return False
		
		if conn == None:
			print "ERROR, connection wasn't created"
			return False
		
		if conn['bound'] != 1 or conn['server'] != server0['index'] or conn['state'] != 7 or conn['flags'] != 0x120:
			print "ERROR, connection was created, but not with correct parameters"
			print conn
			return False
		
		return True
	
	def test_5(self, server_list, ezbox, client_list, vip_list):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		rc = ezbox.start_state_sync_daemon(state = "backup")
		if rc == False:
			print "ERROR: Can't start backup state sync daemon with syncid 0"
			return False
		time.sleep(1)
	
		# create packet
		conn = self.get_ss_conn(6, 0x100, 7, 12305, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[1])))[2:], str(hex(ip2int(server_list[0].ip)))[2:])
		ss_hdr = self.get_ss_hdr(36+8, 1, 1)
		udp_hdr = self.get_udp_hdr(36+8+8, 1234)
		ip_hdr = self.get_ip_header(36+8+8+20, 17, '0a9d0701')
		eth = self.get_eth_type()
		eth_hdr = self.get_multicast_eth_header()
	
		packet = eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr + conn
	
		string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file=ALVSdir + '/verification/testing/dp/temp_packet.pcap')
		client_list[0].send_packet_to_nps(ALVSdir + '/verification/testing/dp/temp_packet.pcap')
	
		time.sleep(1)
	
		conn = ezbox.get_connection(ip2int(vip_list[1]), 80, 0x0a9d0701, 12305, 6)
		
		rc = ezbox.stop_state_sync_daemon(state = "backup")
		if rc == False:
			print "ERROR: Can't stop backup state sync daemon"
			return False
		
		if conn == None:
			print "ERROR, connection wasn't created"
			return False
		
		if conn['bound'] != 0 or conn['server'] != ip2int(server_list[0].ip) or conn['state'] != 7 or conn['flags'] != 0x120:
			print "ERROR, connection was created, but not with correct parameters"
			print conn
			return False
		
		return True
	
	def test_6_9(self, server_list, ezbox, client_list, vip_list):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		port = '80'
		
		rc = ezbox.start_state_sync_daemon(state = "backup", syncid = 0)
		if rc == False:
			print "ERROR: Can't start backup state sync daemon with syncid 0"
			return False
		time.sleep(1)
		# create packet
		conn0 = self.get_ss_conn(6, 0x100, 7, 12306, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].ip)))[2:])
		conn1 = self.get_ss_conn(6, 0x0, 1, 12306, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].ip)))[2:])
		ss_hdr = self.get_ss_hdr(36*2+8, 4, 2)
		udp_hdr = self.get_udp_hdr(36*2+8+8, 1234)
		ip_hdr = self.get_ip_header(36*2+8+8+20, 17, '0a9d0701')
		eth = self.get_eth_type()
		eth_hdr = self.get_multicast_eth_header()
		
		packet = eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr + conn0 + conn1
	
		string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file=ALVSdir + '/verification/testing/dp/temp_packet.pcap')
		client_list[0].send_packet_to_nps(ALVSdir + '/verification/testing/dp/temp_packet.pcap')
	
		time.sleep(1)
		
		conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, 12306, 6)
		server0 = ezbox.get_server(server_list[0].vip, port, server_list[0].ip, port, 6)
		
		rc = ezbox.stop_state_sync_daemon(state = "backup")
		if rc == False:
			print "ERROR: Can't stop backup state sync daemon"
			return False
		
		if conn == None:
			print "ERROR, connection wasn't created"
			return False
		
		if conn['bound'] != 1 or conn['server'] != server0['index'] or conn['state'] != 1 or conn['flags'] != 0x20:
			print "ERROR, connection was created, but not with correct parameters"
			print conn
			return False
		
		return True
	
	def test_7(self, server_list, ezbox, client_list, vip_list):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		port = '80'
		syncid = 7
		rc = ezbox.start_state_sync_daemon(state = "backup", syncid = syncid)
		if rc == False:
			print "ERROR: Can't start backup state sync daemon with syncid %d" %syncid
			return False
		time.sleep(1)
		# create packet
		conn = self.get_ss_conn(6, 0x0, 1, 12307, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[1])))[2:], str(hex(ip2int(server_list[3].ip)))[2:])
		ss_hdr = self.get_ss_hdr(36+8, syncid, 1)
		udp_hdr = self.get_udp_hdr(36+8+8, 1234)
		ip_hdr = self.get_ip_header(36+8+8+20, 17, '0a9d0701')
		eth = self.get_eth_type()
		eth_hdr = self.get_multicast_eth_header()
		
		packet = eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr + conn
	
		string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file=ALVSdir + '/verification/testing/dp/temp_packet.pcap')
		client_list[0].send_packet_to_nps(ALVSdir + '/verification/testing/dp/temp_packet.pcap')
	
		time.sleep(1)
		
		# server not exists
		ezbox.add_server(server_list[3].vip, port, server_list[3].ip, port)
		
		conn = self.get_ss_conn(6, 0x100, 7, 12307, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[1])))[2:], str(hex(ip2int(server_list[3].ip)))[2:])
		ss_hdr = self.get_ss_hdr(36+8, syncid, 1)
		udp_hdr = self.get_udp_hdr(36+8+8, 1234)
		ip_hdr = self.get_ip_header(36+8+8+20, 17, '0a9d0701')
		eth = self.get_eth_type()
		eth_hdr = self.get_multicast_eth_header()
		
		packet = eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr + conn
	
		string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file=ALVSdir + '/verification/testing/dp/temp_packet.pcap')
		client_list[0].send_packet_to_nps(ALVSdir + '/verification/testing/dp/temp_packet.pcap')
	
		time.sleep(1)
		
		conn = ezbox.get_connection(ip2int(vip_list[1]), 80, 0x0a9d0701, 12307, 6)
		server3 = ezbox.get_server(server_list[3].vip, port, server_list[3].ip, port, 6)
		
		rc = ezbox.stop_state_sync_daemon(state = "backup")
		if rc == False:
			print "ERROR: Can't stop backup state sync daemon"
			return False
		
		if conn == None:
			print "ERROR, connection wasn't created"
			return False
		
		if conn['bound'] != 1 or conn['server'] != server3['index'] or conn['state'] != 7 or conn['flags'] != 0x120:
			print "ERROR, connection was created, but not with correct parameters"
			print conn
			return False
		
		return True
	
	def test_8(self, server_list, ezbox, client_list, vip_list):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		port = '80'
		rc = ezbox.start_state_sync_daemon(state = "backup", syncid = 8)
		if rc == False:
			print "ERROR: Can't start backup state sync daemon with syncid 8"
			return False
		time.sleep(1)
		
		# create packet
		conn0 = self.get_ss_conn(6, 0x0, 1, 12308, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[1])))[2:], str(hex(ip2int(server_list[2].ip)))[2:])
		conn1 = self.get_ss_conn(6, 0x100, 7, 12308, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[1])))[2:], str(hex(ip2int(server_list[2].ip)))[2:])
		ss_hdr = self.get_ss_hdr(36*2+8, 8, 2)
		udp_hdr = self.get_udp_hdr(36*2+8+8, 1234)
		ip_hdr = self.get_ip_header(36*2+8+8+20, 17, '0a9d0701')
		eth = self.get_eth_type()
		eth_hdr = self.get_multicast_eth_header()
		
		packet = eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr + conn0 + conn1
	
		string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file=ALVSdir + '/verification/testing/dp/temp_packet.pcap')
		client_list[0].send_packet_to_nps(ALVSdir + '/verification/testing/dp/temp_packet.pcap')
	
		time.sleep(1)
		
		conn = ezbox.get_connection(ip2int(vip_list[1]), 80, 0x0a9d0701, 12308, 6)
		server2 = ezbox.get_server(server_list[2].vip, port, server_list[2].ip, port, 6)
		
		rc = ezbox.stop_state_sync_daemon(state = "backup")
		if rc == False:
			print "ERROR: Can't stop backup state sync daemon"
			return False
		
		if conn == None:
			print "ERROR, connection wasn't created"
			return False
		
		if conn['bound'] != 1 or conn['server'] != server2['index'] or conn['state'] != 7 or conn['flags'] != 0x120:
			print "ERROR, connection was created, but not with correct parameters"
			print conn
			return False
		
		return True
	
	def test_10(self, server_list, ezbox, client_list, vip_list):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		port = '80'
		rc = ezbox.start_state_sync_daemon(state = "backup", syncid = 1)
		if rc == False:
			print "ERROR: Can't start backup state sync daemon with syncid 1"
			return False
		time.sleep(1)
		
		server1 = ezbox.get_server(server_list[1].vip, port, server_list[1].ip, port, 6)
	
		# create packet
		conn0 = self.get_ss_conn(6, 0x0, 1, 12310, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].ip)))[2:])
		conn1 = self.get_ss_conn(6, 0x0, 1, 12310, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[1].ip)))[2:])
		ss_hdr = self.get_ss_hdr(36*2+8, 1, 2)
		udp_hdr = self.get_udp_hdr(36*2+8+8, 1234)
		ip_hdr = self.get_ip_header(36*2+8+8+20, 17, '0a9d0701')
		eth = self.get_eth_type()
		eth_hdr = self.get_multicast_eth_header()
		
		packet = (eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr + conn0 + conn1)
	
		string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file=ALVSdir + '/verification/testing/dp/temp_packet.pcap')
		client_list[0].send_packet_to_nps(ALVSdir + '/verification/testing/dp/temp_packet.pcap')
	
		time.sleep(1)
		
		conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, 12310, 6)
		
		rc = ezbox.stop_state_sync_daemon(state = "backup")
		if rc == False:
			print "ERROR: Can't stop backup state sync daemon"
			return False
		
		if conn == None:
			print "ERROR, connection wasn't created"
			return False
		
		if conn['bound'] != 1 or conn['server'] != server1['index'] or conn['state'] != 1 or conn['flags'] != 0x20:
			print "ERROR, connection was created, but not with correct parameters"
			print conn
			return False
		
		return True
	
	def test_11(self, server_list, ezbox, client_list, vip_list):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
		port = '80'
		rc = ezbox.start_state_sync_daemon(state = "backup", syncid = 11)
		if rc == False:
			print "ERROR: Can't start backup state sync daemon with syncid 11"
			return False
		time.sleep(1)
		
		# create packet
		conn0 = self.get_ss_conn(6, 0x100, 7, 12311, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].ip)))[2:])
		conn1 = self.get_ss_conn(6, 0x100, 7, 12311, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[1].ip)))[2:])
		ss_hdr = self.get_ss_hdr(36*2+8, 11, 2)
		udp_hdr = self.get_udp_hdr(36*2+8+8, 1234)
		ip_hdr = self.get_ip_header(36*2+8+8+20, 17, '0a9d0701')
		eth = self.get_eth_type()
		eth_hdr = self.get_multicast_eth_header()
		
		packet = eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr + conn0 + conn1
	
		string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file=ALVSdir + '/verification/testing/dp/temp_packet.pcap')
		client_list[0].send_packet_to_nps(ALVSdir + '/verification/testing/dp/temp_packet.pcap')
	
		time.sleep(1)
		
		conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, 12311, 6)
		server0 = ezbox.get_server(server_list[0].vip, port, server_list[0].ip, port, 6)
		
		rc = ezbox.stop_state_sync_daemon(state = "backup")
		if rc == False:
			print "ERROR: Can't stop backup state sync daemon"
			return False
		
		if conn == None:
			print "ERROR, connection wasn't created"
			return False
		
		if conn['bound'] != 1 or conn['server'] != server0['index'] or conn['state'] != 7 or conn['flags'] != 0x120:
			print "ERROR, connection was created, but not with correct parameters"
			print conn
			return False
		
		return True
	
	def test_12(self, server_list, ezbox, client_list, vip_list):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
		server_port = '80'
		rc = ezbox.start_state_sync_daemon(state = "backup", syncid = 12)
		if rc == False:
			print "ERROR: Can't start backup state sync daemon with syncid 12"
			return False
		time.sleep(1)
	
		# create packet
		ss_hdr = self.get_ss_hdr(36*10+8, 12, 10)
		udp_hdr = self.get_udp_hdr(36*10+8+8, 1234)
		ip_hdr = self.get_ip_header(36*10+8+8+20, 17, '0a9d0701')
		eth = self.get_eth_type()
		eth_hdr = self.get_multicast_eth_header()
		
		packet = eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr
		
		for port in [10,20,30]:
			conn = self.get_ss_conn(6, 0x100, 7, port, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].ip)))[2:])
			packet += conn
		for port in [11,21,31]:
			conn = self.get_ss_conn(6, 0x0, 1, port, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[1].ip)))[2:])
			packet += conn
		for port in [12,22,32]:
			conn = self.get_ss_conn(6, 0x100, 7, port, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[4].ip)))[2:])
			packet += conn
		for port in [13]:
			conn = self.get_ss_conn(6, 0x0, 1, port, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[1])))[2:], str(hex(ip2int(server_list[2].ip)))[2:])
			packet += conn
			
		string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file=ALVSdir + '/verification/testing/dp/temp_packet.pcap')
		client_list[0].send_packet_to_nps(ALVSdir + '/verification/testing/dp/temp_packet.pcap')
	
		time.sleep(2)
		
		rc = ezbox.stop_state_sync_daemon(state = "backup")
		if rc == False:
			print "ERROR: Can't stop backup state sync daemon"
			return False
		
		server0 = ezbox.get_server(server_list[0].vip, server_port, server_list[0].ip, server_port, 6)
		server1 = ezbox.get_server(server_list[1].vip, server_port, server_list[1].ip, server_port, 6)
		server2 = ezbox.get_server(server_list[2].vip, server_port, server_list[2].ip, server_port, 6)
		for port in [10,20,30]:
			conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, port, 6)
		
			if conn == None:
				print "ERROR, connection wasn't created"
				return False
			
			if conn['bound'] != 1 or conn['server'] != server0['index'] or conn['state'] != 7 or conn['flags'] != 0x120:
				print "ERROR, connection was created, but not with correct parameters"
				print conn
				return False
		
		for port in [11,21,31]:
			conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, port, 6)
		
			if conn == None:
				print "ERROR, connection wasn't created"
				return False
			
			if conn['bound'] != 1 or conn['server'] != server1['index'] or conn['state'] != 1 or conn['flags'] != 0x20:
				print "ERROR, connection was created, but not with correct parameters"
				print conn
				return False
			
		for port in [12,22,32]:
			conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, port, 6)
			
			if conn == None:
				print "ERROR, connection wasn't created"
				return False
			
			if conn['bound'] != 0 or conn['server'] != ip2int(server_list[4].ip) or conn['state'] != 7 or conn['flags'] != 0x120:
				print "ERROR, connection was created, but not with correct parameters"
				print conn
				return False
	
		for port in [13]:
			conn = ezbox.get_connection(ip2int(vip_list[1]), 80, 0x0a9d0701, port, 6)
		
			if conn == None:
				print "ERROR, connection wasn't created"
				return False
			
			if conn['bound'] != 1 or conn['server'] != server2['index'] or conn['state'] != 1 or conn['flags'] != 0x20:
				print "ERROR, connection was created, but not with correct parameters"
				print conn
				return False
		
		return True
	
	def test_13(self, server_list, ezbox, client_list, vip_list):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
		server_port = '80'
		rc = ezbox.start_state_sync_daemon(state = "backup", syncid = 13)
		if rc == False:
			print "ERROR: Can't start backup state sync daemon with syncid 13"
			return False
		time.sleep(1)
		# create packet
		ss_hdr = self.get_ss_hdr(36*9+8, 13, 9)
		udp_hdr = self.get_udp_hdr(36*9+8+8, 1234)
		ip_hdr = self.get_ip_header(36*9+8+8+20, 17, '0a9d0701')
		eth = self.get_eth_type()
		eth_hdr = self.get_multicast_eth_header()
		
		packet = (eth_hdr + eth + ip_hdr + udp_hdr + ss_hdr)
		
		conn = self.get_ss_conn(6, 0x0, 1, 10, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].ip)))[2:])
		packet += conn
		
		conn = self.get_ss_conn(6, 0x100, 7, 20, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[1].ip)))[2:])
		packet += conn
	
		conn = self.get_ss_conn(6, 0x100, 7, 30, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[4].ip)))[2:])
		packet += conn
	
		conn = self.get_ss_conn(6, 0x100, 7, 11, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[1].ip)))[2:])
		packet += conn
	
		conn = self.get_ss_conn(6, 0x0, 1, 21, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].ip)))[2:])
		packet += conn
	
		conn = self.get_ss_conn(6, 0x0, 1, 31, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[4].ip)))[2:])
		packet += conn	
	
		conn = self.get_ss_conn(6, 0x0, 1, 12, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[4].ip)))[2:])
		packet += conn
	
		conn = self.get_ss_conn(6, 0x100, 7, 22, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[0].ip)))[2:])
		packet += conn
	
		conn = self.get_ss_conn(6, 0x100, 7, 32, 80, 80, 0, 0, '0a9d0701', str(hex(ip2int(vip_list[0])))[2:], str(hex(ip2int(server_list[1].ip)))[2:])
		packet += conn
			
		string_to_pcap_file(' '.join(re.findall('.{%d}' % 2, packet)), output_pcap_file=ALVSdir + '/verification/testing/dp/temp_packet.pcap')
		client_list[0].send_packet_to_nps(ALVSdir + '/verification/testing/dp/temp_packet.pcap')
	
		time.sleep(2)
		
		rc = ezbox.stop_state_sync_daemon(state = "backup")
		if rc == False:
			print "ERROR: Can't stop backup state sync daemon"
			return False
		
		server0 = ezbox.get_server(server_list[0].vip, server_port, server_list[0].ip, server_port, 6)
		server1 = ezbox.get_server(server_list[1].vip, server_port, server_list[1].ip, server_port, 6)
		server2 = ezbox.get_server(server_list[2].vip, server_port, server_list[2].ip, server_port, 6)
		for port in [10,21]:
			conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, port, 6)
		
			if conn == None:
				print "ERROR, connection wasn't created"
				return False
			
			if conn['bound'] != 1 or conn['server'] != server0['index'] or conn['state'] != 1 or conn['flags'] != 0x20:
				print "ERROR, connection (cport=%d) was created, but not with correct parameters"%port
				print conn
				return False
	
		for port in [11]:
			conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, port, 6)
		
			if conn == None:
				print "ERROR, connection wasn't created"
				return False
		
			if conn['bound'] != 1 or conn['server'] != server1['index'] or conn['state'] != 7 or conn['flags'] != 0x120:
				print "ERROR, connection (cport=%d) was created, but not with correct parameters"%port
				print conn
				return False
	
		for port in [22, 32]:
			conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, port, 6)
		
			if conn == None:
				print "ERROR, connection wasn't created"
				return False
			
			if conn['bound'] != 0 or conn['server'] != ip2int(server_list[4].ip) or conn['state'] != 7 or conn['flags'] != 0x120:
				print "ERROR, connection (cport=%d) was created, but not with correct parameters"%port
				print conn
				return False
	
		for port in [31, 12]:
			conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, port, 6)
		
			if conn == None:
				print "ERROR, connection wasn't created"
				return False
	
			if conn['bound'] != 0 or conn['server'] != ip2int(server_list[4].ip) or conn['state'] != 1 or conn['flags'] != 0x20:
				print "ERROR, connection (cport=%d) was created, but not with correct parameters"%port
				print conn
				return False
	
		for port in [20, 30]:
			conn = ezbox.get_connection(ip2int(vip_list[0]), 80, 0x0a9d0701, port, 6)
		
			if conn == None:
				print "ERROR, connection wasn't created"
				return False
			
			if conn['bound'] != 1 or conn['server'] != server0['index'] or conn['state'] != 7 or conn['flags'] != 0x120:
				print "ERROR, connection (cport=%d) was created, but not with correct parameters"%port
				print conn
				return False
			
		for port in [13]:
			conn = ezbox.get_connection(ip2int(vip_list[1]), 80, 0x0a9d0701, port, 6)
		
			if conn == None:
				print "ERROR, connection wasn't created"
				return False
			
			if conn['bound'] != 1 or conn['server'] != server2['index'] or conn['state'] != 1 or conn['flags'] != 0x20:
				print "ERROR, connection (cport=%d) was created, but not with correct parameters"%port
				print conn
				return False
		
		return True
	
	
	def clear_test(self, ezbox):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		print "Test clear ezbox test"
		ezbox.flush_ipvs()
		time.sleep(1)
		if ezbox.get_num_of_services()!= 0:
			print "after FLUSH all, num_of_services should be 0\n"
			return False
		return True
		
	def run_user_test(self):
		port = '80'
		sched_algorithm = 'sh'
		ezbox = self.test_resources['ezbox']
		server_list = self.test_resources['server_list']
		client_list = self.test_resources['client_list']
		vip_list = self.test_resources['vip_list']
		
		failed_tests = 0
		ezbox.add_service(vip_list[0], port, sched_alg=sched_algorithm, sched_alg_opt='')
		ezbox.add_service(vip_list[1], port, sched_alg=sched_algorithm, sched_alg_opt='')
		
		for i,server in enumerate(server_list):
			# add only the first 3 servers
			if i < 3:
				ezbox.add_server(server.vip, port, server.ip, port)
		
		print "\nTest 2 - check dropping packet with syncid 2 while backup was started with syncid 1"
		rc = self.test_2(server_list, ezbox, client_list, vip_list)
		if rc == False:
			print 'Test2 failed !!!\n'
			failed_tests += 1
		else:
			print 'Test2 passed !!!\n'
		
		print "Test 3 - Backup is not started"
		rc = self.test_3(server_list, ezbox, client_list, vip_list)
		if rc == False:
			print 'Test3 failed !!!\n'
			failed_tests += 1
		else:
			print 'Test3 passed !!!\n'
		
		print "Test 4 - Create a new connection (bound)"
		print "In addition Test 4 check receiving packet with syncid 1 while backup was started with syncid 1"
		rc = self.test_4(server_list, ezbox, client_list, vip_list)
		if rc == False:
			print 'Test4 failed !!!\n'
			failed_tests += 1
		else:
			print 'Test4 passed !!!\n'
		
		print "Test 5 - Create a new connection (unbound)"
		print "In addition Test 5 check receiving packet with syncid 1 while backup was started with syncid 0"
		rc = self.test_5(server_list, ezbox, client_list, vip_list)
		if rc == False:
			print 'Test5 failed !!!\n'
			failed_tests += 1
		else:
			print 'Test5 passed !!!\n'
		
		print "Test 6_9 - Update a connection (bound) with new state"
		print "In addition Test 6_9 check receiving packet with syncid 4 while backup was started with syncid 0"
		rc = self.test_6_9(server_list, ezbox, client_list, vip_list)
		if rc == False:
			print 'Test6_9 failed !!!\n'
			failed_tests += 1
		else:
			print 'Test6_9 passed !!!\n'
		
		print "Test 7 - "
		rc = self.test_7(server_list, ezbox, client_list, vip_list)
		if rc == False:
			print 'Test7 failed !!!\n'
			failed_tests += 1
		else:
			print 'Test7 passed !!!\n'
		
		print "Test 8 - "
		rc = self.test_8(server_list, ezbox, client_list, vip_list)
		if rc == False:
			print 'Test8 failed !!!\n'
			failed_tests += 1
		else:
			print 'Test8 passed !!!\n'
		
		print "Test 10 - "
		rc = self.test_10(server_list, ezbox, client_list, vip_list)
		if rc == False:
			print 'Test10 failed !!!\n'
			failed_tests += 1
		else:
			print 'Test10 passed !!!\n'
		
		print "Test 11 - "
		rc = self.test_11(server_list, ezbox, client_list, vip_list)
		if rc == False:
			print 'Test11 failed !!!\n'
			failed_tests += 1
		else:
			print 'Test11 passed !!!\n'
		
		print "Test 12 - "
		rc = self.test_12(server_list, ezbox, client_list, vip_list)
		if rc == False:
			print 'Test12 failed !!!\n'
			failed_tests += 1
		else:
			print 'Test12 passed !!!\n'
		
		print "Test 13 - "
		rc = self.test_13(server_list, ezbox, client_list, vip_list)
		if rc == False:
			print 'Test13 failed !!!\n'
			failed_tests += 1
		else:
			print 'Test13 passed !!!\n'
		
		rc = (ezbox)
		if rc == False:
			print 'Clear test failed !!!\n'
			failed_tests += 1
		else:
			print 'Clear test passed !!!\n'
		
		if failed_tests == 0:
			print 'ALL Tests were passed !!!'
		else:
			print 'Number of failed tests: %d' %failed_tests
			exit(1)
			
current_test = State_Sync_Test()
current_test.main()
