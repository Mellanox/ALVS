#!/usr/bin/env python
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
from pip._vendor.pyparsing import Empty
# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir)

from common_infra import *

class cli_commands():
	
	def __init__(self, ssh_object):
		self.ssh_object = ssh_object
	
	def execute_command(self, cmd, error_msg):
		result, output = self.ssh_object.execute_command(cmd)
		if result == False:
			raise RuntimeError(error_msg)
		return output	
	
	def check_if_handle_line(self, handle, line_cleaned):
		if len(line_cleaned.split(' ')) >= 2:
			return line_cleaned.split(' ')[-2] == 'handle' and int(line_cleaned.split(' ')[-1], 16) == int(handle)
		return False
	
	def check_if_end_of_handle(self, line_cleaned):
		return len(line_cleaned) >= 1 and line_cleaned.split(' ')[0] == 'filter'
	
	def filter_handle_statistics(self, all_priority_stats, handle):
		handle_string = ""
		found = False
		priority_stats_splitted = all_priority_stats.split('\n')
		for line in priority_stats_splitted:
			line_cleaned = line.rstrip().replace('\t','')
			if found:
				if self.check_if_end_of_handle(line_cleaned):
					break
				else:
					handle_string += line_cleaned + "\n"
			elif not found and self.check_if_handle_line(handle, line_cleaned):
				found = True
		return handle_string
	
	def get_actions_statistics(self, handle_stats):
		lst_of_actions_stats = []
		handle_stats_splitted = handle_stats.split('\n')
		action = ''
		start_flag = False
		for line in handle_stats_splitted:
			if len(line.split(' ')) > 0 and line.split(' ')[0] == 'action':
				start_flag = True
				if action != '':
					lst_of_actions_stats.append(action)
				action = line + '\n'
			elif start_flag:
				action += line + '\n'
		if action != '':
			lst_of_actions_stats.append(action)
		return lst_of_actions_stats
	
	def get_dict_from_action_stats(self, action):
		dict = {}
		action_stats_splitted = action.split(' ')
		installed_pos = action_stats_splitted.index('installed')
		dict['last_installed'] = int(action_stats_splitted[installed_pos + 1])
		used_pos = action_stats_splitted.index('used')
		dict['last_used'] = int(action_stats_splitted[used_pos + 1])
		bytes_pos = action_stats_splitted.index('bytes')
		dict['bytes'] = int(action_stats_splitted[bytes_pos - 1])
		packets_pos = action_stats_splitted.index('pkt')
		dict['packets'] = int(action_stats_splitted[packets_pos - 1])
		return dict
		
	def parse_actions_stats_into_list_of_dict(self, list_of_actions_statistics):
		lst_of_dict = []
		for action in list_of_actions_statistics:
			lst_of_dict.append(self.get_dict_from_action_stats(action))
		return lst_of_dict
	
	def get_statistics(self, eth, priority, handle, tc_or_atc):
		cmd = tc_or_atc + ' -s filter show dev %s parent ffff: priority %s' %(eth, priority)
		#cmd = 'atc -s filter show dev %s parent ffff: priority %s' %(eth, priority)
		output = self.execute_command(cmd, 'ERROR, showing statistics failed')
		handle_stats = self.filter_handle_statistics(output, handle)
		list_of_actions_statistics = self.get_actions_statistics(handle_stats)
		return self.parse_actions_stats_into_list_of_dict(list_of_actions_statistics)
