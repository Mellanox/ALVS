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
sys.path.append('/.autodirect/swgwork/roysr/sandbox/ALVS/verification/testing/DDP/ddp_players_factory.py')


from trex_infra_utils import *
from DDP_infra import *
from ddp_players_factory import *

#===============================================================================
# Classes
#===============================================================================
class TrexDdpPlayers(TrexPlayers):

    def __init__(self):
        super(TrexDdpPlayers, self).__init__()
        self.ddp_players = None 
        self.mask = 16
        
    def get_players(self, setup_num, client_count, server_count,trex_hosts_count):
        super(TrexDdpPlayers, self).get_players(setup_num, client_count, server_count, trex_hosts_count)        
        self.ddp_players =  DDP_Players_Factory.generic_init_ddp(setup_num)
        
    def init_players(self, test_config={}):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        DDP_Players_Factory.init_players(self.ddp_players, test_config)
        self.clean_test_fib_entries()
            
    def clean_players(self, test_config={}):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        super(TrexDdpPlayers, self).clean_players()
        DDP_Players_Factory.clean_players(self.ddp_players, True, test_config['stop_ezbox'])
            
    def clean_test_fib_entries(self):
        ezbox = self.ddp_players['ezbox']
        for trex in self.trex_hosts:
            for port_info in trex.trex_info['ports_info']:
                ezbox.delete_fib_entry(port_info['subnet'], self.mask) 

#===============================================================================   