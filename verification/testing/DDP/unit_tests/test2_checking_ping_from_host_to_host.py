#!/usr/bin/env python

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

host_count = 2

class Test2(DDP_Tester):
		
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = generic_init(setup_num, host_count)
		
		for index,host in enumerate(self.test_resources['host_list']):
			if index==0:
				host.change_mode(1)
			else:
				host.change_mode(2)
	
	
	def run_user_test(self):
		self.test_resources['host_list'][0].send_ping(self.test_resources['host_list'][1].ip)
		self.test_resources['host_list'][1].send_ping(self.test_resources['host_list'][0].ip)
	
current_test = Test2()
current_test.main()