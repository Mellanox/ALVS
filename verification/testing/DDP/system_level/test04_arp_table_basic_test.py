#!/usr/bin/env python

import sys
import os
import inspect
my_currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
my_parentdir = os.path.dirname(my_currentdir)
my_grandparentdir = os.path.dirname(my_parentdir)
sys.path.append(my_grandparentdir)
sys.path.append(my_parentdir)

from common_infra import *
from DDP_infra import *
from ddp_tester import *

host_count = 0

class Test4(DDP_Tester):
	
	def user_init(self, setup_num):
			print "FUNCTION " + sys._getframe().f_code.co_name + " called"
			self.test_resources = DDP_Players_Factory.generic_init_ddp(setup_num, host_count)
			
	
	
	def run_user_test(self):
		ezbox = self.test_resources['ezbox']
		remote_host = self.test_resources['remote_host']
		network=0 #could be between 0-3
		data_ip =remote_host.ip[network] 
		ip = ezbox.get_unused_ip_on_subnet(ip=data_ip)[0]
		mac = '01:02:03:04:05:06'
		print ip
	   	#########Step 1: Add arp etnry to remote host#################
	   	x=ezbox.add_arp_entry(ip, mac)
	   	time.sleep(1)
		ret = ezbox.check_cp_entry_exist(ip, mac)
		if(ret == False):
			raise TestFailedException ("Cannot find the ARP entry in the cp ARP table")
		print "Step 1 passed!"

	   	#########Step 2: Modify arp etnry to remote host################
		new_mac = "01:02:03:04:05:07"
		ezbox.add_arp_entry(ip, new_mac)
	   	time.sleep(1)
		ret = ezbox.check_cp_entry_exist(ip, new_mac)
		if(ret == False):
			raise TestFailedException("Cannot find the ARP entry in the cp ARP table")
		print "Step 2 passed!"
		#########Step 3: Delete arp entry##################
		ezbox.delete_arp_entry(ip)
	   	time.sleep(1)
		ret = ezbox.check_cp_entry_exist(ip, new_mac)
		if(ret == True):
			raise TestFailedException ("ARP entry is still in cp ARP table")
		print "Step 3 passed!"
		           
current_test = Test4()
current_test.main()
