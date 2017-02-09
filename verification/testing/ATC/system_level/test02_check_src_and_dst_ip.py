#!/usr/bin/env python
import sys
import os
import inspect
from time import sleep
my_currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
my_parentdir = os.path.dirname(my_currentdir)
my_grandparentdir =  os.path.dirname(my_parentdir)
sys.path.append(my_grandparentdir)
sys.path.append(my_parentdir)

from atc_infra import *
from atc_tester import *
from atc_general_checker import *

host_count = 2


class Test2(atc_tester):
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		self.test_resources = ATC_Players_Factory.generic_init_atc(setup_num, host_count)
		
		self.test_resources['host_list'][0].change_interface(Network_Interface.REGULAR)
		self.test_resources['host_list'][1].change_interface(Network_Interface.REGULAR)
		
	def run_user_test(self):
		ezbox = self.test_resources['ezbox']
		src_host = self.test_resources['host_list'][0]
		dst_host = self.test_resources['host_list'][1]
		
		ezbox.send_ping(src_host.ip)
		ezbox.send_ping(dst_host.ip)
		
		ezbox.atc_commands.remove_all_qdisc()
		ezbox.atc_commands.add_atc_filter(eth = 'nps_mac0', priority = 10, handle = 5, fields = {'dst_ip': dst_host.ip, 'src_ip': src_host.ip}, actions = ['ok'])
		
		tcpdump_params = 'tcpdump -v -w ' + dst_host.dump_pcap_file + ' -i ' + dst_host.eth + ' src host ' + src_host.ip + ' &'
		dst_host.capture_packets(tcpdump_params)
		
		packet_to_dst_host = tcp_packet(mac_da      = ezbox.get_interface(0)['mac_address'],
									mac_sa          = src_host.mac_address,
									ip_dst          = ip_to_hex_display(dst_host.ip),
									ip_src          = ip_to_hex_display(src_host.ip),
									tcp_source_port = '00 00',
									tcp_dst_port    = '00 50') # port 80
		packet_to_dst_host.generate_packet()
		src_host.copy_and_send_pcap(packet_to_dst_host.pcap_file_name)
		
		dict_list = []
		# the field num_of_packets_on_dest we can used once on specific destenation host
		dict = {'expected_packets_on_filter': 0,
				'priority'                  : 10,
				'handel'                    : 5,
				'num_of_packets_on_dest'    : 1,
				'dst_host'                  : dst_host}
		dict_list.append(dict)
		expected_dict = {'host_stats'     : False,
						'mirror_vm'       : False,
						'dlist'           : dict_list}
		
		result = atc_checker(self.test_resources, expected_dict)
		if not result:
			raise TestFailedException('general checker failed')
	
current_test = Test2()
current_test.main()