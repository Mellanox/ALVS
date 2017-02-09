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
# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir)

from optparse import OptionParser
from multiprocessing import Process
import cmd
from cmd import Cmd
dir = os.path.dirname(parentdir)
from common_infra import *
from atc_commands import *
from cli_commands import *


class atc_ezbox(ezbox_host):
	
	def __init__(self, setup_id):
		# init parent class
		super(atc_ezbox, self).__init__(setup_id,"atc")
		self.atc_commands = atc_commands(self.ssh_object) 
		self.cli = cli_commands(self.ssh_object)
		self.install_path = "atc_install"
		
	def copy_and_install_package(self, atc_package=None):
		if atc_package is None:
			# get package name
			atc_package = "atc_*_amd64.deb"
		self.copy_package(atc_package)
		self.install_package(atc_package)
	
	def atc_service(self, cmd):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called for " + cmd + " service"
		self.run_app_ssh.execute_command("service atc " + cmd)

	def service_start(self):
		self.run_app_ssh.execute_command("cat /dev/null > /var/log/syslog")
		self.syslog_ssh.wait_for_msgs(['file truncated'])
		self.atc_service("start")

	def service_stop(self):
		self.atc_service("stop")

	def service_restart(self):
		self.atc_service("restart")

	def clean(self,stop_ezbox):
		if stop_ezbox:
			self.service_stop()
		self.logout()

#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
