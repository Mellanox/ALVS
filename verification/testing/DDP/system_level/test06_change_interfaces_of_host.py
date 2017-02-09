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
from test_failed_exception import *
host_count = 1


class Test6(DDP_Tester):
	
	def bring_interface_up(self,host,interface): #bring interface up and bring down all the rest
		host.change_interface(interface)
		host.config_interface()
		 	
	
	def is_interface_up(self,host,interface): 
		host_ifconfig = host.read_ifconfig()
		return host.all_eths[interface] in host_ifconfig
	
	def are_other_interfaces_down(self,host,interface):
		host_ifconfig = host.read_ifconfig()
		for down_interface in Network_Interface: #test that all other vlans are down
					if down_interface != interface and host.all_eths[down_interface] in host_ifconfig:
							raise TestFailedException ("Failed to bring down interface " + interface.name)
		
		
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		self.test_resources = DDP_Players_Factory.generic_init_ddp(setup_num, host_count)
		host = self.test_resources['host_list'][0]
		host.change_interface(Network_Interface.SUBNET1)
	
	
	def run_user_test(self):
		host = self.test_resources['host_list'][0]
		for interface in Network_Interface:
			if interface != Network_Interface.REGULAR: #iterate over DDP subnets
				print "Testing " + interface.name 
				self.bring_interface_up(host,interface)
				if not self.is_interface_up(host,interface):
					raise TestFailedException ("Failed to bring up interface " + interface.name)
				self.are_other_interfaces_down(host, interface)


current_test = Test6()
current_test.main()