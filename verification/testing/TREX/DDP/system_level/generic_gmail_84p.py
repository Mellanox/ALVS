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
grandir = os.path.dirname(parentdir)
sys.path.append(parentdir) 
sys.path.append(grandir) 
from trex_infra_utils import *
from trex_ddp_tester import  TrexDdpTester

class genricGmail(TrexDdpTester):

    def user_init(self):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        setup_num = self.config['setup_num']
        self.test_resources.get_players(setup_num, client_count = 1000, server_count = 1000, trex_hosts_count = 1)
        
        #Set Run parameters
        for trex in self.test_resources.trex_hosts:
            run_params = trex.params.run_params
            run_params.set_multiplier(1)
            run_params.set_duration(30)
            run_params.set_cores(1)
            
    def user_create_yaml(self):
        for trex in self.test_resources.trex_hosts:
            trex.params.add_trex_pcap(TrexPcap(name = '/swgwork/roysr/trex/trex_17.04/pcaps_for_application_mix/generic_gmail_84P_994B.pcap', cps = 1))
            trex.params.generate_trex_yaml()
            
    def user_set_l7_rules(self):
        remote_host = self.test_resources.ddp_players['remote_host']
        remote_host.synca_commands.disable_l7_features()
    
current_test = genricGmail()
current_test.main()
