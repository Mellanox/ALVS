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

host_count = 1

class Test1(DDP_Tester):
	
	def user_init(self, setup_num):
			print "FUNCTION " + sys._getframe().f_code.co_name + " called"
			
			self.test_resources = generic_init(setup_num, host_count)
			
			for host in self.test_resources['host_list']:
				host.change_mode(1)
				
	
	def run_user_test(self):
		self.test_resources['host_list'][0].send_ping(self.test_resources['ezbox'].ip)
		self.test_resources['ezbox'].send_ping(self.test_resources['host_list'][0].ip)
		

current_test = Test1()
current_test.main()
		
