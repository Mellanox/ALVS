#!/usr/bin/env python
#===============================================================================
# imports
#===============================================================================
# system  
import os
import sys
import inspect

# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.append(parentdir) 
from trex_infra_utils import *
from trex_ddp_tester import  TrexDdpTester

#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

class TrexDdpSanity(TrexDdpTester):

    def user_init(self):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        
        setup_num = self.config['setup_num']
        self.test_resources.get_players(setup_num, client_count = 1, server_count = 1, trex_hosts_count = 1)
        
        #Set Run parameters
        for trex in self.test_resources.trex_hosts:
            run_params = trex.params.run_params
            run_params.set_multiplier(100)
            run_params.set_duration(30)
            run_params.set_cores(1)
        
    def user_create_yaml(self):
        for trex in self.test_resources.trex_hosts:
            trex.params.add_trex_pcap(TrexPcap(name = 'avl/http_browsing.pcap', cps = 1))
            trex.params.generate_trex_yaml()
    
current_test = TrexDdpSanity()
current_test.main()
