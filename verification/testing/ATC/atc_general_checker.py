#===============================================================================
# imports
#===============================================================================
# system  
import cmd
import os
import inspect
import sys
import traceback
import re
import abc
# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir)


from common_infra import *
from atc_commands import *
from cli_commands import *
from test_failed_exception import *


class atc_checker(object):
	def __init__(self, test_resources, expected={}):
		self.test_resources = test_resources
		self.expected       = expected
		self.general_checker()
	
	def default_general_checker_expected(self):
		default_expected = {'checksum_stats'      : True,
							'host_stats'          : True,
							'mirror_vm'           : True}
		
		# Add missing configuration
		for key in default_expected:
			if key not in self.expected:
				self.expected[key] = default_expected[key]
	
	def general_checker(self):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		self.default_general_checker_expected()
		
		gen_rc = True
		stats_rc = True
		
		if self.expected['checksum_stats']:
			gen_rc = self.checksum_checker(self.expected['dst_host'])
		
		if self.expected['host_stats']:
			gen_rc = gen_rc and self.host_and_nps_stats_checker()
		
		for expected_case in self.expected['dlist']:
			if 'expected_packets_on_filter' in expected_case:
				stats_rc = stats_rc and self.matches_on_cli_stats_checker(expected_case)
			
			if 'num_of_packets_on_dest' in expected_case:
				stats_rc = stats_rc and self.captured_packets_on_dest_checker(expected_case)
		
		if not (gen_rc and stats_rc):
			raise TestFailedException('General Checker Failed')
	
	def checksum_checker(self, host):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		rc, exit_code = host.execute_command('tshark -r ' + host.dump_pcap_file + ' | grep INCORRECT')
		if rc == True:
			print "CHECKER ERROR: checksum checker failed. Exit code:  %s" %exit_code
		return not(rc)
	
	def matches_on_cli_stats_checker(self, expected):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		rc = True
		priority = expected['priority']
		handel = expected['handel']
		stats_dic = self.test_resources['ezbox'].cli.get_statistics('nps_mac0', priority, handel, 'tc')   # change to atc when cli is ready!!!!!!
		if stats_dic[0]['packets'] != expected['expected_packets_on_filter']:
			print "CHECKER ERROR: unexpected number of packets in cli. Expected: %s . Captured: %s" %(expected['expected_packets_on_filter'],stats_dic[0]['packets'])
			rc = False
		return rc
	
	def captured_packets_on_dest_checker(self, expected):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		rc = True
		num_of_packets = expected['dst_host'].stop_capture()
		if num_of_packets != expected['num_of_packets_on_dest']:
			print "CHECKER ERROR: num of received packets: %s. Excepted: %s" %(num_of_packets, expected['num_of_packets_on_dest'])
			rc = False
		return rc
	
	# not finished 
	def host_and_nps_stats_checker(self):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		tc_stats_dic = self.test_resources['ezbox'].cli.get_statistics('nps_mac0', priority, handel, 'tc')
		atc_stats_dic = self.test_resources['ezbox'].cli.get_statistics('nps_mac0', priority, handel, 'atc')
		
		#need to compare dictionaries !!!!!!!!!!!!!!!!!!!!!!!!
		result = True
		return result
		
		