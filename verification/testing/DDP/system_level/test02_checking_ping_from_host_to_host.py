#!/usr/bin/env python

import sys
import os
import inspect

currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
grandparentdir =  os.path.dirname(parentdir)
sys.path.append(parentdir)
sys.path.append(grandparentdir)

from common_infra import *
from DDP_infra import *
from ddp_tester import *
from test_failed_exception import *

host_count = 2

class Test2(DDP_Tester):
	
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = generic_init_ddp(setup_num, host_count)
		
		for index,host in enumerate(self.test_resources['host_list']):
			if index==0:
				host.change_interface(Network_Interface.SUBNET1)
			else:
				host.change_interface(Network_Interface.SUBNET2)
	
	def run_user_test(self):
		ezbox = self.test_resources['ezbox']
		host_1 = self.test_resources['host_list'][0]
		host_2 = self.test_resources['host_list'][1]
		remote_host = self.test_resources['remote_host']
		
		
		ezbox.clean_arp_table()
		
		remote_host.capture_packet()
		
		time.sleep(1)
		print "Testing ping from host 1 to host 2"
		rc = host_1.send_ping(host_2.ip)
		if rc != 0:
			raise TestFailedException ("Ping from host 1 to host 2 failed")
		
		print "Testing ping from host 2 to host 1"
		rc = host_2.send_ping(host_1.ip)
		if rc != 0:
			raise TestFailedException ("Ping from host 2 to host 1 failed")
		time.sleep(1)
		
		num_of_captured = remote_host.stop_capture()
		print "num of captured packets: " + str(num_of_captured)
		if num_of_captured != 2:
			raise TestFailedException ("Ping test failed. Remote host captured %s packets"%num_of_captured)
		
current_test = Test2()
current_test.main()