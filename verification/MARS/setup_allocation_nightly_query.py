#!/usr/bin/env python
import os
import fcntl
import inspect
import time
import re
import os
import pwd
import sys
import smtplib
from email.mime.text import MIMEText
from thread import allocate
from _dbus_bindings import Message


def open_file(file_path, permission):
	try:
		fd = open(file_path, permission)
	except:
		print '****** Error openning file %s !!!' % file_path
		exit(1)
	return fd

################################################################################
# Function: Main
################################################################################
def main():
	file_path_to_allocation_files ='/.autodirect/sw_regression/nps_sw/MARS/MARS_conf/setups/ALVS_gen/setup_allocation'
	file_path_of_all_setups = file_path_to_allocation_files + '/setups.txt'
	fd_of_all_setups = open_file(file_path_of_all_setups, 'r+')


	allocated_setups = {}
	for line in fd_of_all_setups:
		
		if line[0] != '#':
			is_locked = False
			file_path_of_curr_setup = file_path_to_allocation_files + '/setup' + line[:-1] + '.txt'
			fd_of_curr_setup = open_file(file_path_of_curr_setup, 'r')
			is_locked = os.stat(file_path_of_curr_setup).st_size != 0
			
			#send email to one who holds a setup
			if is_locked:
				curr_setup_inf = fd_of_curr_setup.read()
				allocated_setups[line[:-1]] = curr_setup_inf
				
				msg = MIMEText('You allocated setup number: ' +line[:-1] + '  since ' + curr_setup_inf.partition(' ')[2])
				msg['Subject'] = 'Setup allocation status report'
				msg['From'] = 'automation@mellanox.com'
				msg['To'] = curr_setup_inf.split('\t')[0] + '@mellanox.com'
				
				s = smtplib.SMTP('localhost')
				s.sendmail(msg['From'],[msg['To']],msg.as_string())
				s.quit()
				
			fd_of_curr_setup.close()
	
	
	# send email with full report
	if allocated_setups:
		message = ""
		for setup in allocated_setups:
			message += 'Setup number: ' + setup + ' is allocated by ' + allocated_setups[setup] + '\n'
		
		msg = MIMEText(message)
		msg['Subject'] = 'Setup allocation status report'
		msg['From'] = 'automation@mellanox.com'
		msg['To'] = 'anat@mellanox.com'
		
		s = smtplib.SMTP('localhost')
		s.sendmail(msg['From'],[msg['To']],msg.as_string())
		s.quit()
main()

