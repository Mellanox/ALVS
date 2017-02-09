#!/usr/bin/env python

import sys
import os
import inspect
my_currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
my_parentdir = os.path.dirname(my_currentdir)
my_grandparentdir =  os.path.dirname(my_parentdir)
sys.path.append(my_grandparentdir)
sys.path.append(my_parentdir)

from atc_infra import *
from atc_tester import *

host_count = 0

class Test1(atc_tester):
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		self.test_resources = ATC_Players_Factory.generic_init_atc(setup_num, host_count)
		
	def run_user_test(self):
		ezbox = self.test_resources['ezbox']
		print ezbox.atc_commands.execute_command("ifconfig","error msg")
		
current_test = Test1()
current_test.main()