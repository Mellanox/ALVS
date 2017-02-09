#!/usr/bin/env python

import sys
import os
import inspect
import time
my_currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
my_parentdir = os.path.dirname(my_currentdir)
my_grandparentdir =  os.path.dirname(my_parentdir)
sys.path.append(my_grandparentdir)
sys.path.append(my_parentdir)

from DDP_infra import *
from ddp_tester import *
from test_failed_exception import *
host_count = 0

class Test5(DDP_Tester):
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		self.test_resources = DDP_Players_Factory.generic_init_ddp(setup_num, host_count)
		
	def run_user_test(self):
		
		ezbox = self.test_resources['ezbox']
		for i in range(0,4):
			eth = self.test_resources['remote_host'].all_eths[i]
			
			print "enable interface ",eth
			self.test_resources['remote_host'].ifconfig_eth_up(eth)
			ezbox.check_if_eth_is_up(i)
			
			
			print "disable interface ", eth
			self.test_resources['remote_host'].ifconfig_eth_down(eth)
			ezbox.check_if_eth_is_down(i)
			
			print "enable interface %s again"%eth
			self.test_resources['remote_host'].ifconfig_eth_up(eth)
			ezbox.check_if_eth_is_up(i)
		
		
current_test = Test5()
current_test.main()