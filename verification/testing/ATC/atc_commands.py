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
# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir)

from common_infra import *

class atc_commands():
	
	def __init__(self, ssh_object):
		self.ssh_object = ssh_object
		self.ingress_qdisc_flag = []
		
	def execute_command(self, cmd, error_msg):
		result, output = self.ssh_object.execute_command(cmd)
		if result == False:
			raise RuntimeError(output)
		return output
	#temp function 
	def remove_all_qdisc(self):
		print 'remove all qdisc'
		for i in range(4):
			cmd = "tc qdisc del dev nps_mac%s ingress" %i
			self.ssh_object.execute_command(cmd)
	
	def add_qdisc_to_eth(self, eth):
		cmd = "tc qdisc add dev %s ingress" %(eth)
		self.execute_command(cmd, 'ERROR, adding qdisc to eth: %s' %eth)
		self.ingress_qdisc_flag.append(eth)

	def remove_qdisc_from_eth(self, eth):
		cmd = "tc qdisc del dev %s parent fff" %(eth)
		self.execute_command(cmd, 'ERROR, removing qdisc to eth: %s' %eth)
		
	def atc_qdisc_show(self):
		cmd = "tc qdisc show"
		return self.execute_command(cmd, 'ERROR, qdisc show')
	
	def get_protocol_str(self, filter_fields):
		protocol_str = ''
		ip_fields = ['src_ip', 'dst_ip', 'ip_proto', 'src_port', 'dst_port']
		if any( ip_field in filter_fields for ip_field in ip_fields):
			protocol_str = ' protocol ip'
		return protocol_str
	
	def atc_filter(self, act , eth, priority="", protocol="", handle="", fields = {}, actions = []):
		protocol_str = self.get_protocol_str(fields)
		self.execute_command('tc filter ' + act +\
							' dev ' + eth + protocol_str +\
							' parent ffff:'\
							' ' + self.get_pref_string(priority) +\
							' ' + self.get_handle_string(handle) +\
							' flower ' + dict_to_string(fields) +\
							' ' + self.actions_to_string(actions),'ERROR, in tc filter command')
		time.sleep(0.5)
		
	def add_atc_filter(self, eth, priority="", protocol="", handle="", fields = {}, actions = []):
		if eth not in self.ingress_qdisc_flag:
			self.add_qdisc_to_eth(eth)

		self.atc_filter("add", eth, priority, protocol, handle, fields , actions)
	
	def change_atc_filter(self, eth, priority="", protocol="", handle="", fields = {}, actions = []):
		self.atc_filter("change", eth, priority, protocol, handle, fields , actions)
		
	def replace_atc_filter(self, eth, priority="", protocol="", handle="", fields = {}, actions = []):
		self.atc_filter("replace", eth, priority, protocol, handle, fields , actions)
		
	def remove_filters_from_interface(self, eth, priority="", handle=""):
		self.execute_command("tc filter del dev %s parent ffff: %s %s flower"%(eth, self.get_pref_string(priority), self.get_handle_string(handle))
							, 'ERROR, removing tc filter from eth: %s'%(eth))
		
	def get_flower_filter_info(self, eth, priority="", handle=""):
		result, output = self.execute_command("tc filter show dev %s %s %s parent ffff:"%(eth, self.get_pref_string(priority), self.get_handle_string(handle))
											,'ERROR, showing tc filter info from eth: %s'%(eth))
		time.sleep(0.5)
	
	def get_priority_string(self, priority):
		if priority != "":
			priority = "priority %s" % priority
		return priority

	def get_handle_string(self, handle):
		if handle != "":
			handle = "handle %s" % handle
		return handle
	
	def actions_to_string(self, actions):
		if actions:
			return 'action '+ list_to_string(actions)
		return ''
