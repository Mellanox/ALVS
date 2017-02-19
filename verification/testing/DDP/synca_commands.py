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
import json
import time
# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir)

from common_infra import *

class synca_commands():
	
	def __init__(self, ssh_object):
		self.ssh_object = ssh_object
		
	def execute_synca_command(self, command):
		print "from synca:" + command
		self.ssh_object.execute_command(command, wait_prompt=False)
		self.ssh_object.ssh_object.expect('.*enter command \(or "exit" to quit\)\: ')		
		time.sleep(1)
		
	def enable_l7_features(self):
		func_name = sys._getframe().f_code.co_name
		print "FUNCTION %s called " %(func_name)
		for ifc_num in range(4):
			cmd = "en %s" %ifc_num
			self.execute_synca_command(cmd)
			
	def disable_l7_features(self):
		func_name = sys._getframe().f_code.co_name
		print "FUNCTION %s called " %(func_name)
		for ifc_num in range(4):
			cmd = "dis %s" %ifc_num
			self.execute_synca_command(cmd)
			
	def add_rule(self, rule_num , action, value, src_ip='*.*.*.*', dst_ip='*.*.*.*', sprt='*', dprt='*', app='*', if_index='*', protocol='*', src_is_conn_initiator='*'):
		cmd = "add %s,%s,%s,%s,%s,%s,%s,%s,%s,%s:%s" %(rule_num,src_ip,dst_ip,sprt,dprt,app,if_index,protocol,src_is_conn_initiator,action,value)
		self.execute_synca_command(cmd)