#!/usr/bin/env python

from common_infra import *
import time

class ezbox_host_tc(ezbox_host):
	
	# list of all qdiscs that were configured, list of ingress interfaces
	ingress_qdisc_flag = [] 
	
	def add_tc_filter(self, ingress_device, priority_value="", protocol="", handle="", actions):
		if ingress_device not in ingress_qdisc_flag:
			self.execute_command_on_host("tc qdisc add dev %s ingress"%ingress_device)
			ingress_qdisc_flag.append(ingress_device)
		
		if priority_value != "":
			pref = "pref %s"%priority_value
		if protocol != "":
			protocol = "protocol %s"%protocol
		if handle != "":
			handle = "handle %s"%handle

		self.execute_command_on_host("tc filter add dev %s %s %s parent ffff: flower "%(ingress_device, pref, protocol))
		time.sleep(0.5)
		
	def remove_filters_from_interface(self, ingress_device):
		return
		
	def get_flower_filter_info(self, device, pref="", handle=""):
		result, output = self.execute_command_on_host("tc filter show dev %s pref %s handle %s parent ffff:"%(device, pref, handle))
		if result == False:
			print "ERROR, while executing command"
			exit(1)
		time.sleep(0.5)
		
		# parse the output
		
		
		