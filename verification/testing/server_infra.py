#!/usr/bin/env python
#===============================================================================
# imports
#===============================================================================
# system  
import os
import sys

# pythons modules 
import pexpect
from pexpect import pxssh
import cmd
import logging

# local 
from common_infra import *

#===============================================================================
# Classes
#===============================================================================
class HttpServer(player):
	def __init__(self, ip, hostname, username, password, exe_path, exe_script, exec_params, vip, net_mask="255.255.255.255"):
		# init parent class
		super(HttpServer, self).__init__(ip, hostname, username, password, exe_path, exe_script, exec_params)
		# Init class variables
		self.net_mask = net_mask
		self.vip = vip

	def init_server(self, index_str):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		self.connect()
		self.start_http_daemon()
		self.configure_loopback()
		self.disable_arp()
		self.set_index_html(index_str)

	def clean_server(self):
		print "FUNCTION " + sys._getframe().f_code.co_name + " called"
		self.stop_http_daemon()
		self.take_down_loopback()
		self.enable_arp()
		self.delete_index_html()
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

	def stop_http_daemon(self):
		rc, output = self.ssh.execute_command("service httpd stop")
		if rc != True:
			print "ERROR: Stop HTTP daemon failed. rc=" + str(rc) + " " + output
			return


	def configure_loopback(self):
		cmd = "ifconfig lo:0 " + str(self.vip) + " netmask " + str(self.net_mask)
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: Configuting loopback failed. rc=" + str(rc) + " " + output
			return

	
	def take_down_loopback(self):
		cmd = "ifconfig lo:0 down"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: Take down loopback failed. rc=" + str(rc) + " " + output
			return
	

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
		rc, output = self.ssh.execute_command(cmd = "echo \"1\" >/proc/sys/net/ipv4/conf/eth0/arp_ignore")
		if rc != True:
			print "ERROR: Enable arp ignore (eth0) failed. rc=" + str(rc) + " " + output
			return
		
		# Enable arp announce
		rc, output = self.ssh.execute_command("echo \"2\" > /proc/sys/net/ipv4/conf/all/arp_announce")
		if rc != True:
			print "ERROR: Enable arp announce (all) failed. rc=" + str(rc) + " " + output
			return
		rc, output = self.ssh.execute_command("echo \"2\" > /proc/sys/net/ipv4/conf/eth0/arp_announce")
		if rc != True:
			print "ERROR: Enable arp announce (eth0) failed. rc=" + str(rc) + " " + output
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
		rc, output = self.ssh.execute_command(cmd = "echo \"0\" >/proc/sys/net/ipv4/conf/eth0/arp_ignore")
		if rc != True:
			print "ERROR: Disable arp ignore (eth0) failed. rc=" + str(rc) + " " + output
			return
		
		# Disable arp announce
		rc, output = self.ssh.execute_command("echo \"0\" > /proc/sys/net/ipv4/conf/all/arp_announce")
		if rc != True:
			print "ERROR: Disable arp announce (all) failed. rc=" + str(rc) + " " + output
			return
		rc, output = self.ssh.execute_command("echo \"0\" > /proc/sys/net/ipv4/conf/eth0/arp_announce")
		if rc != True:
			print "ERROR: Disable arp announce (eth0) failed. rc=" + str(rc) + " " + output
			return


	def set_index_html(self, str):
		cmd = "echo " + str + " >/var/www/html/index.html"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: setting index.html failed. rc=" + str(rc) + " " + output
			return


	def delete_index_html(self):
		cmd = "rm -f /var/www/html/index.html"
		rc, output = self.ssh.execute_command(cmd)
		if rc != True:
			print "ERROR: setting index.html failed. rc=" + str(rc) + " " + output
			return
