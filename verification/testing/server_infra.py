#!/usr/bin/env python
#===============================================================================
# imports
#===============================================================================
# system  
import cmd
import logging
import os
import sys

from common_infra import *
from pexpect import pxssh
import pexpect



#===============================================================================
# Classes
#===============================================================================

class HttpServer(player):
	def __init__(self, ip, hostname,
				username    = "root",
				password    = "3tango",
				exe_path    = None,
				exe_script  = None,
				exec_params = None,
				vip         = None,
				net_mask    = "255.255.255.255",
				eth         = 'ens6',
				weight      = 1):
		# init parent class
		super(HttpServer, self).__init__(ip, hostname, username, password, exe_path, exe_script, exec_params)
		# Init class variables
		self.net_mask = net_mask
		self.vip = vip
		self.eth = eth
		self.weight = weight
	def init_server(self, index_str):
		self.connect()
		self.clear_arp_table()
		self.start_http_daemon()
		self.configure_loopback()
		self.disable_arp()
		self.set_index_html(index_str)
		self.set_test_html()

	def clean_server(self, verbose = True):
		self.stop_http_daemon(verbose)
		self.take_down_loopback(verbose)
		self.enable_arp()
		self.delete_index_html(verbose)
		self.delete_test_html(verbose)
		self.logout()
		
	def start_http_daemon(self):
		rc, output = self.ssh.execute_command("service httpd start")
		if rc != True:
			print "ERROR: Start HTTP daemon failed. rc=" + str(rc) + " " + output
			return
		
		rc, output = self.ssh.execute_command("chkconfig httpd on")
		if rc != True:
			print "ERROR: setting chkconfig ON failed. rc=" + str(rc) + " " + output
			return

	def stop_http_daemon(self, verbose = True):
		rc, output = self.ssh.execute_command("service httpd stop")
		if rc != True and verbose:
			print "ERROR: Stop HTTP daemon failed. rc=" + str(rc) + " " + output
	
	def update_vip(self,new_vip):
		self.take_down_loopback()
		self.vip= new_vip
		self.configure_loopback()
	
	def configure_loopback(self):
		cmd = "ifconfig lo:0 " + str(self.vip) + " netmask " + str(self.net_mask)
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: Configuting loopback failed. rc=" + str(rc) + " " + output
			return
	
	def take_down_loopback(self, verbose = True):
		cmd = "ifconfig lo:0 down"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True and verbose:
			print "ERROR: Take down loopback failed. rc=" + str(rc) + " " + output

	def disable_arp(self):
		# Disable IP forwarding
		rc, output = self.ssh.execute_command("echo \"0\" > /proc/sys/net/ipv4/ip_forward")
		if rc != True:
			print "ERROR: Disable IP forwarding failed. rc=" + str(rc) + " " + output
			return

		# Enable arp ignore
		rc, output = self.ssh.execute_command("echo \"1\" > /proc/sys/net/ipv4/conf/all/arp_ignore")
		if rc != True:
			print "ERROR: Enable IP forwarding failed. rc=" + str(rc) + " " + output
			return
		cmd = "echo \"1\" >/proc/sys/net/ipv4/conf/" + self.eth + "/arp_ignore"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: Enable arp ignore ("+ self.eth + ") failed. rc=" + str(rc) + " " + output
			return
		
		# Enable arp announce
		rc, output = self.ssh.execute_command("echo \"2\" > /proc/sys/net/ipv4/conf/all/arp_announce")
		if rc != True:
			print "ERROR: Enable arp announce (all) failed. rc=" + str(rc) + " " + output
			return
		cmd = "echo \"2\" > /proc/sys/net/ipv4/conf/" + self.eth + "/arp_announce"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: Enable arp announce ("+ self.eth + ") failed. rc=" + str(rc) + " " + output
			return


	def enable_arp(self):
		# Enable IP forwarding
		rc, output = self.ssh.execute_command("echo \"1\" >/proc/sys/net/ipv4/ip_forward")
		if rc != True:
			print "ERROR: Enable IP forwarding failed. rc=" + str(rc) + " " + output
			return

		# Disable arp ignore
		rc, output = self.ssh.execute_command("echo \"0\" > /proc/sys/net/ipv4/conf/all/arp_ignore")
		if rc != True:
			print "ERROR: Disable IP forwarding failed. rc=" + str(rc) + " " + output
			return
		cmd = "echo \"0\" >/proc/sys/net/ipv4/conf/" + self.eth + "/arp_ignore"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: Disable arp ignore (" + self.eth +") failed. rc=" + str(rc) + " " + output
			return
		
		# Disable arp announce
		rc, output = self.ssh.execute_command("echo \"0\" > /proc/sys/net/ipv4/conf/all/arp_announce")
		if rc != True:
			print "ERROR: Disable arp announce (all) failed. rc=" + str(rc) + " " + output
			return
		cmd = "echo \"0\" > /proc/sys/net/ipv4/conf/" + self.eth + "/arp_announce"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: Disable arp announce (" + self.eth + ") failed. rc=" + str(rc) + " " + output
			return


	def set_index_html(self, str):
		cmd = "echo " + str + " >/var/www/html/index.html"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: setting index.html failed. rc=" + str(rc) + " " + output
			return
		
		
	def set_large_index_html(self):
		# This command repeates index.html into a ~3MB file
		cmd = "for i in {1..18}; do cat /var/www/html/index.html /var/www/html/index.html > /var/www/html/index2.html && mv -f /var/www/html/index2.html /var/www/html/index.html; done"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: setting index.html failed. rc=" + str(rc) + " " + output
			return

	def set_extra_large_index_html(self):
		# This command repeates index.html into a ~52MB file
		cmd = "for i in {1..22}; do cat /var/www/html/index.html /var/www/html/index.html > /var/www/html/index2.html && mv -f /var/www/html/index2.html /var/www/html/index.html; done"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: setting index.html failed. rc=" + str(rc) + " " + output
			return
		
	def set_test_html(self):
		cmd = 'echo "Still alive" >/var/www/html/test.html'
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: setting test.html failed. rc=" + str(rc) + " " + output
			return

	def delete_index_html(self, verbose = True):
		cmd = "rm -f /var/www/html/index.html"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True and verbose:
			print "ERROR: deleting index.html failed. rc=" + str(rc) + " " + output

	def delete_test_html(self, verbose = True):
		cmd = "rm -f /var/www/html/test.html"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True and verbose:
			print "ERROR: deleting test.html failed. rc=" + str(rc) + " " + output
	
