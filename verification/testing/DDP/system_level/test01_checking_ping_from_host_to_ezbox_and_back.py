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



host_count = 1


class Test1(DDP_Tester):
	
	def user_init(self, setup_num):
			print "FUNCTION " + sys._getframe().f_code.co_name + " called"
			
			self.test_resources = DDP_Players_Factory.generic_init_ddp(setup_num, host_count)
			
			for host in self.test_resources['host_list']:
				host.change_interface(Network_Interface.SUBNET1)
			
	def run_user_test(self):
		host = self.test_resources['host_list'][0]
		remote_host = self.test_resources['remote_host']
		
		print "Testing ping from host to remote host"
		rc = host.send_ping(remote_host.ip[0])
		if rc != 0:
			raise RuntimeError('no ping from host to remote host')
		
		print "Testing ping from remote host to host"
		rc = remote_host.send_ping(host.ip)
		if rc != 0:
			raise RuntimeError('no ping from remote host to host')
		
current_test = Test1()
current_test.main()
		
