import sys
import os
import inspect
my_currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
my_parentdir = os.path.dirname(my_currentdir)
my_grandparentdir =  os.path.dirname(my_parentdir)
sys.path.append(my_grandparentdir)
sys.path.append(my_parentdir)

from common_infra import *
from DDP_infra import *
from ddp_tester import *

host_count = 0

class Test2(DDP_Tester):
		
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		self.test_resources = generic_init(setup_num, host_count)
		
	def check_if_eth_is_up(self, eth_up_index):
		interfaces = self.test_resources['remote-host'].get_all_interfaces()
		for interface in interfaces:
			if interface['key'] == eth_up_index:
				print "eth %s is up" %eth_up_index
				break
			
		print "ERROR eth %s is not up" %eth_up_index
		exit(1)
	
	def check_if_eth_is_down(self, eth_down_index):
		interfaces = self.test_resources['remote-host'].get_all_interfaces()
		for interface in interfaces:
			if interface['key'] == eth_down_index:
				print "ERROR eth %s is not down" %eth_down_index
				exit(1)
			
		print "eth %s is down" %eth_down_index
	
	def run_user_test(self):
		for i in range(0,3):
			eth = self.test_resources['remote-host']['all_eth'][i]
			print "enable interface ",eth
			self.test_resources['remote-host'].ifconfig_eth_up(eth)
			check_if_eth_is_up(i)
			
			print "disable interface ", eth
			self.test_resources['remote-host'].ifconfig_eth_down(eth)
			check_if_eth_is_down(i)
			
			print "enable interface %s again"%eth
			self.test_resources['remote-host'].ifconfig_eth_up(eth)
			check_if_eth_is_up(i)
		
	