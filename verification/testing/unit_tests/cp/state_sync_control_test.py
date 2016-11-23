#!/usr/bin/env python


#===============================================================================
# imports
#===============================================================================

# system  
import random
import time

# pythons modules 
# local
import sys
import os
import inspect
my_currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
my_parentdir = os.path.dirname(my_currentdir)
my_grandparentdir =  os.path.dirname(my_parentdir)
sys.path.append(my_grandparentdir)
sys.path.append(my_parentdir)
from common_infra import *
from e2e_infra import *
from unit_tester import Unit_Tester

server_count   = 0
client_count   = 0
service_count  = 0

class State_Sync_Control_Test(Unit_Tester):
	
	def user_init(self, setup_num):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		self.test_resources = generic_init(setup_num, service_count, server_count, client_count)
		
		w = 1
		for s in self.test_resources['server_list']:
			s.vip = self.test_resources['vip_list'][0]
			s.weight = w
			
	def change_config(self, config):
		config['start_ezbox'] = True
	
	def check_alvs_state_sync_info(self, expected_alvs_ss_info, alvs_ss_info):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		error = 0
		if len(alvs_ss_info) != len(expected_alvs_ss_info) :
			print "ERROR, len(alvs_ss_info)  = %d expected = %d\n" % (len(alvs_ss_info), len(expected_alvs_ss_info))
			error = 1
		
		if alvs_ss_info['master_bit'] != expected_alvs_ss_info['master_bit'] :
			print "ERROR, master_bit  = %d expected = %d\n" % (alvs_ss_info['master_bit'], expected_alvs_ss_info['master_bit'])
			error = 1
	
		if alvs_ss_info['backup_bit'] != expected_alvs_ss_info['backup_bit'] :
			print "ERROR, backup_bit = %d, expected = %d\n" % (alvs_ss_info['backup_bit'], expected_alvs_ss_info['backup_bit'])
			error = 1
	
		if alvs_ss_info['m_sync_id'] != expected_alvs_ss_info['m_sync_id'] :
			print "ERROR, m_sync_id  = %d expected = %d\n" % (alvs_ss_info['m_sync_id'], expected_alvs_ss_info['m_sync_id'])
			error = 1
			
		if alvs_ss_info['b_sync_id'] != expected_alvs_ss_info['b_sync_id'] :
			print "ERROR, b_sync_id  = %d expected = %d\n" % (alvs_ss_info['b_sync_id'], expected_alvs_ss_info['b_sync_id'])
			error = 1
		
		return error
	
	#===============================================================================
	# Tests
	#===============================================================================
	
	def start_master(self, ezbox):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		final_rc = True
		rc = ezbox.start_state_sync_daemon(state = "master")
		if rc == False:
			print "ERROR: Can't start master state sync daemon with default syncid"
			final_rc = False
		
		print "wait 1 second for EZbox to update"
		time.sleep(1)
		
		apps_info = ezbox.get_applications_info()
		alvs_ss_info = apps_info[0]
		
		expected_alvs_ss_info = {'master_bit' : 1,
								 'backup_bit' : 0,
								 'm_sync_id' : 0,
								 'b_sync_id' : 0}
		
		print "expected state sync info:"
		print expected_alvs_ss_info
		
		rc = self.check_alvs_state_sync_info(expected_alvs_ss_info, alvs_ss_info)	
		if rc:
			print "Error in comparing alvs state sync info"
			final_rc = False
		
		rc = ezbox.stop_state_sync_daemon(state = "master")
		if rc == False:
			print "ERROR: Can't stop master state sync daemon"
			final_rc = False
		
		return final_rc
	
	
	def start_backup(self, ezbox):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		final_rc = True
		rc = ezbox.start_state_sync_daemon(state = "backup", syncid = 10)
		if rc == False:
			print "ERROR: Can't start backup state sync daemon with syncid 10"
			final_rc = False
		
		print "wait 1 second for EZbox to update"
		time.sleep(1)
		
		apps_info = ezbox.get_applications_info()
		alvs_ss_info = apps_info[0]
		
		expected_alvs_ss_info = {'master_bit' : 0,
								 'backup_bit' : 1,
								 'm_sync_id' : 0,
								 'b_sync_id' : 10}
		
		print "expected state sync info:"
		print expected_alvs_ss_info
		
		rc = self.check_alvs_state_sync_info(expected_alvs_ss_info, alvs_ss_info)	
		if rc:
			print "Error in comparing alvs state sync info"
			final_rc = False
		
		rc = ezbox.stop_state_sync_daemon(state = "backup")
		if rc == False:
			print "ERROR: Can't stop backup state sync daemon"
			final_rc = False
		
		return final_rc
	
	
	def start_master_backup(self, ezbox):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		final_rc = True
		rc = ezbox.start_state_sync_daemon(state = "master", syncid = 7)
		if rc == False:
			print "ERROR: Can't start master state sync daemon with syncid 7"
			final_rc = False
		rc = ezbox.start_state_sync_daemon(state = "backup", syncid = 17)
		if rc == False:
			print "ERROR: Can't start backup state sync daemon with syncid 17"
			final_rc = False
		
		print "wait 1 second for EZbox to update"
		time.sleep(1)
		
		apps_info = ezbox.get_applications_info()
		alvs_ss_info = apps_info[0]
		
		expected_alvs_ss_info = {'master_bit' : 1,
								 'backup_bit' : 1,
								 'm_sync_id' : 7,
								 'b_sync_id' : 17}
		
		print "expected state sync info:"
		print expected_alvs_ss_info
		
		rc = self.check_alvs_state_sync_info(expected_alvs_ss_info, alvs_ss_info)	
		if rc:
			print "Error in comparing alvs state sync info"
			final_rc = False
		
		rc = ezbox.stop_state_sync_daemon(state = "master")
		if rc == False:
			print "ERROR: Can't stop master state sync daemon"
			final_rc = False
		rc = ezbox.stop_state_sync_daemon(state = "backup")
		if rc == False:
			print "ERROR: Can't stop backup state sync daemon"
			final_rc = False
		
		return final_rc
	
	
	def start_stop_master_backup(self, ezbox):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		final_rc = True
		rc = ezbox.start_state_sync_daemon(state = "master", syncid = 7)
		if rc == False:
			print "ERROR: Can't start master state sync daemon with syncid 7"
			final_rc = False
		rc = ezbox.start_state_sync_daemon(state = "backup", syncid = 17)
		if rc == False:
			print "ERROR: Can't start backup state sync daemon with syncid 17"
			final_rc = False
		
		rc = ezbox.stop_state_sync_daemon(state = "master")
		if rc == False:
			print "ERROR: Can't stop master state sync daemon"
			final_rc = False
		rc = ezbox.stop_state_sync_daemon(state = "backup")
		if rc == False:
			print "ERROR: Can't stop backup state sync daemon"
			final_rc = False
		
		print "wait 1 second for EZbox to update"
		time.sleep(1)
		
		apps_info = ezbox.get_applications_info()
		alvs_ss_info = apps_info[0]
		
		expected_alvs_ss_info = {'master_bit' : 0,
								 'backup_bit' : 0,
								 'm_sync_id' : 0,
								 'b_sync_id' : 0}
		
		print "expected state sync info:"
		print expected_alvs_ss_info
		
		rc = self.check_alvs_state_sync_info(expected_alvs_ss_info, alvs_ss_info)	
		if rc:
			print "Error in comparing alvs state sync info"
			final_rc = False
		
		return final_rc
	
	
	def start_stop_restart_master_backup(self, ezbox):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		final_rc = True
		rc = ezbox.start_state_sync_daemon(state = "master")
		if rc == False:
			print "ERROR: Can't start master state sync daemon with default syncid"
			final_rc = False
		rc = ezbox.start_state_sync_daemon(state = "backup")
		if rc == False:
			print "ERROR: Can't start backup state sync daemon with default syncid"
			final_rc = False
		
		print "wait 1 second for EZbox to update"
		time.sleep(1)
		
		apps_info = ezbox.get_applications_info()
		alvs_ss_info = apps_info[0]
		
		expected_alvs_ss_info = {'master_bit' : 1,
								 'backup_bit' : 1,
								 'm_sync_id' : 0,
								 'b_sync_id' : 0}
		print "expected state sync info:"
		print expected_alvs_ss_info
		
		rc = self.check_alvs_state_sync_info(expected_alvs_ss_info, alvs_ss_info)	
		if rc:
			print "Error in comparing alvs state sync info"
			final_rc = False
		
		rc = ezbox.stop_state_sync_daemon(state = "master")
		if rc == False:
			print "ERROR: Can't stop master state sync daemon"
			final_rc = False
		rc = ezbox.stop_state_sync_daemon(state = "backup")
		if rc == False:
			print "ERROR: Can't stop backup state sync daemon"
			final_rc = False
		
		rc = ezbox.start_state_sync_daemon(state = "master", syncid = 5)
		if rc == False:
			print "ERROR: Can't start master state sync daemon with syncid 5"
			final_rc = False
		rc = ezbox.start_state_sync_daemon(state = "backup", syncid = 3)
		if rc == False:
			print "ERROR: Can't start backup state sync daemon with syncid 3"
			final_rc = False
		
		print "wait 1 second for EZbox to update"
		time.sleep(1)
		
		apps_info = ezbox.get_applications_info()
		alvs_ss_info = apps_info[0]
		
		expected_alvs_ss_info = {'master_bit' : 1,
								 'backup_bit' : 1,
								 'm_sync_id' : 5,
								 'b_sync_id' : 3}
		
		print "expected state sync info:"
		print expected_alvs_ss_info
		
		rc = self.check_alvs_state_sync_info(expected_alvs_ss_info, alvs_ss_info)	
		if rc:
			print "Error in comparing alvs state sync info"
			final_rc = False
		
		rc = ezbox.stop_state_sync_daemon(state = "master")
		if rc == False:
			print "ERROR: Can't stop master state sync daemon"
			final_rc = False
		rc = ezbox.stop_state_sync_daemon(state = "backup")
		if rc == False:
			print "ERROR: Can't stop backup state sync daemon"
			final_rc = False
		
		return final_rc
	
	
	def start_master_with_illegal_mcast_ifn(self, ezbox):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		
		rc = ezbox.start_state_sync_daemon(state = "master", syncid = 5, mcast_if = "eth1")
		if rc == True:
			print "ERROR: Can't start master state sync daemon with syncid 5 and mcast-interface eth1"
			return False
		return True
		
	def run_user_test(self):
		ezbox = self.test_resources['ezbox']
		server_list = self.test_resources['server_list']
		client_list = self.test_resources['client_list']
		vip_list = self.test_resources['vip_list']
		
		failed_tests = 0
		rc = 0
		
		rc = self.start_master(ezbox)
		if rc:
			print 'start_master passed !!!\n'
		else:
			print 'start_master failed !!!\n'
			failed_tests += 1
		
		rc = self.start_backup(ezbox)
		if rc:
			print 'start_backup passed !!!\n'
		else:
			print 'start_backup failed !!!\n'
			failed_tests += 1
		
		rc = self.start_master_backup(ezbox)
		if rc:
			print 'start_master_backup passed !!!\n'
		else:
			print 'start_master_backup failed !!!\n'
			failed_tests += 1
		
		rc = self.start_stop_master_backup(ezbox)
		if rc:
			print 'start_stop_master_backup passed !!!\n'
		else:
			print 'start_stop_master_backup failed !!!\n'
			failed_tests += 1
		
		rc = self.start_stop_restart_master_backup(ezbox)
		if rc:
			print 'start_stop_restart_master_backup passed !!!\n'
		else:
			print 'start_stop_restart_master_backup failed !!!\n'
			failed_tests += 1
		
		rc = self.start_master_with_illegal_mcast_ifn(ezbox)
		if rc:
			print 'start_master_with_illegal_mcast_ifn passed !!!\n'
		else:
			print 'start_master_with_illegal_mcast_ifn failed !!!\n'
			failed_tests += 1
		
		print "Cleaning EZbox..."
		ezbox.clean(use_director=False, stop_ezbox=True)
		
		if failed_tests == 0:
			print 'ALL Tests were passed !!!'
		else:
			print 'Number of failed tests: %d' %failed_tests
			exit(1)
	
current_test = State_Sync_Control_Test()
current_test.main()
	
