#!/usr/bin/env python

#===============================================================================
# imports
#===============================================================================
# system  
import os
import sys
from netaddr import *
import inspect

# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
grandparentdir =  os.path.dirname(parentdir)
sys.path.append(parentdir)
sys.path.append(grandparentdir)
sys.path.append("/.autodirect/swgwork/roysr/sandbox/ALVS/verification/testing/DDP/")

from trex_infra_utils import *
from DDP_infra import *

#===============================================================================
# Classes
#===============================================================================
class TrexDdpPlayers(TrexPlayers):

    def __init__(self):
        super(TrexDdpPlayers, self).__init__()
        self.ezbox = None
        self.remote_host = None
        
    def get_players(self, setup_num, client_count, server_count,trex_hosts_count):
        super(TrexDdpPlayers, self).get_players(setup_num, client_count, server_count, trex_hosts_count)        
        self.remote_host = self.get_remote_host(setup_num)
        self.ezbox = DDP_ezbox(setup_num, self.remote_host)
        
    def get_remote_host(self, setup_num):
        remote_host_info = get_remote_host(setup_num)
        return Remote_Host(ip        = remote_host_info['all_ips'],
                           hostname  = remote_host_info['hostname'],
                           all_eths  = remote_host_info['all_eths'])

    def init_players(self, test_config={}):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        # init ezbox in another proccess
        if test_config['start_ezbox']:
            ezbox_init_proccess = Process(target=self.ezbox.common_ezbox_bringup,
                                      args=(test_config,))
            ezbox_init_proccess.start()
        
        # connect Ezbox (proccess work on ezbox copy and not on ezbox object
        self.ezbox.connect()
        self.remote_host.connect()
        
        # Wait for EZbox proccess to finish
        if test_config['start_ezbox']:
            ezbox_init_proccess.join()
        # Wait for all proccess to finish
            if ezbox_init_proccess.exitcode:
                print "ezbox_init_proccess failed. exit code " + str(ezbox_init_proccess.exitcode)
                exit(ezbox_init_proccess.exitcode)
        
        time.sleep(6)
            
    def clean_players(self, use_director = False, stop_ezbox = False):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        super(TrexDdpPlayers, self).clean_players(use_director = False, stop_ezbox = False)
        if self.ezbox:
            self.ezbox.ssh_object.recreate_ssh_object()
            self.ezbox.run_app_ssh.recreate_ssh_object()
            self.ezbox.syslog_ssh.recreate_ssh_object()
            self.ezbox.clean_arp_table()
            clean_ezbox(self.ezbox, stop_ezbox)

#===============================================================================   