#!/usr/bin/env python
import os
import fcntl
import inspect
import time
import re
import os
import pwd
import sys

def open_file(file_path, permission):
	try:
		fd = open(file_path, permission)
	except:
		print '****** Error openning file %s !!!' % file_path
		exit(1)
	return fd

def main():
	dir = '/.autodirect/sw_regression/nps_sw/MARS/MARS_conf/setups/ALVS_gen/setup_allocation'
	file_path_of_all_setups = dir + '/setups.txt'
	fd_of_all_setups = open_file(file_path_of_all_setups, 'r+')

	for line in fd_of_all_setups:
		if line[0] != '#':
			is_locked = False
			file_path_of_curr_setup = dir + '/setup' + line[:-1] + '.txt'
			fd_of_curr_setup = open_file(file_path_of_curr_setup, 'r')
			#os.chmod(file_path_of_curr_setup, 0777)
			is_locked = os.stat(file_path_of_curr_setup).st_size != 0
			if is_locked:
				print 'Setup number: ' + line[:-1] + ' is locked by: %s' % fd_of_curr_setup.read()[:-1]
			fd_of_curr_setup.close()
main()
