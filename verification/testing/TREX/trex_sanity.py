#!/usr/bin/env python


#===============================================================================
# imports
#===============================================================================

# system  
import cmd
from collections import namedtuple
import logging
import os
import sys
import inspect
from multiprocessing import Process

# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(1,parentdir) 
from trex_infra_utils import *
from trex_tester import TrexTester
#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

class TrexSanity(TrexTester):

    def user_init(self):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        setup_num = self.config['setup_num']
        self.test_resources = get_trex_test_resources(setup_num, client_count = 1000, 
                                                      server_count = 10, service_count = 10)
        
        for i,s in enumerate(self.test_resources['server_list']):
            s.vip = self.test_resources['vip_list'][i%len(self.test_resources['vip_list'])]
            
    def user_create_yaml(self):
        trex = self.test_resources['trex']
        trex.params.add_trex_pcap(TrexPcap(name = 'avl/short_http_flow.pcap'))
        trex.params.generate_trex_yaml()
    
    def run_user_test(self):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        port = '80'
        ezbox = self.test_resources['ezbox']
        trex = self.test_resources['trex']
        
        for service in self.test_resources['vip_list']:
            ezbox.add_service(service, port, sched_alg_opt='-b sh-port')

        for i,server in enumerate(self.test_resources['server_list']):
            if i < (trex.params.server_count / 2):
                ezbox.add_arp_static_entry(server.ip, trex.trex_info['port1_server'])
            else:
                ezbox.add_arp_static_entry(server.ip, trex.trex_info['port2_server'])
            
            ezbox.add_server(server.vip, port, server.ip, port,server.weight)
                 
        self.run_test()
        
        print 'End user test'
    
current_test = TrexSanity()
current_test.main()
