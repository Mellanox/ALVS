#!/usr/bin/env python

#===============================================================================
# imports
#===============================================================================
# system  
import os
import inspect
import sys
import abc
from netaddr import *
from multiprocessing import Process, Queue 
import time
# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.append(parentdir) 
sys.path.append('/.autodirect/swgwork/roysr/sandbox/ALVS/verification/testing/DDP/ddp_players_factory.py')

from trex_infra_utils import *
from trex_ddp_utils import *
from trex_tester import TrexTester
from ddp_players_factory import *

#===============================================================================
# Test Globals
#===============================================================================
class TrexDdpTester(TrexTester):
    
    __metaclass__  = abc.ABCMeta
    
    def __init__(self):        
        self.config = {'setup_num'       : None,
                       'modify_run_cpus' : True,
                       'use_4k_cpus'     : False,
                       'install_package' : True,
                       'install_file'    : None,
                       'copy_binaries'   : True,
                       'use_director'    : False,
                       'start_ezbox'     : False,
                       'stop_ezbox'      : False}
        
        self.test_resources = TrexDdpPlayers()
        self.test_result = TrexTestResult()
        
    @abc.abstractmethod
    def user_init(self):
        """User initialization - must be implemented"""
        pass
    
    @abc.abstractmethod
    def user_create_yaml(self):
        """yaml creation - must be implemented"""
        pass
       
    def user_set_l7_rules(self):
		"""set l7 rules- optional function"""
		pass
    
    def clean_all_players(self):
        self.test_resources.clean_players(self.config)
    
    def run_test(self):
        process_list = []
        trex_test_result_list = []
        #Use a synchronized queue class to share a variable between threads
        trex_test_result_queue = Queue()
        
        for trex in self.test_resources.trex_hosts:
            process_list.append(Process(target=trex.run_trex_test, args=(trex_test_result_queue,)))
                        
        for p in process_list:
            p.start()    
                            
        for p in process_list:
            trex_test_result_list.append(trex_test_result_queue.get())  
                      
        for p in process_list:
            p.join()   

        self.test_result.set_result(trex_test_result_list) 
        
    def set_up_route_counfig(self):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        ezbox = self.test_resources.ddp_players['ezbox']
        mask = self.test_resources.mask
        
        for trex in self.test_resources.trex_hosts:
            for port_info in trex.trex_info['ports_info']:
                ezbox.add_fib_entry(port_info['subnet'], mask, port_info['gw'])  
                ezbox.add_arp_static_entry(port_info['gw'], port_info['mac'])  

    def general_trex_checker(self):
        self.test_result.general_checker()
        
    def get_test_rc(self):
        return self.test_result.get_rc()
    
    def start_test(self):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        self.config = DDP_Players_Factory.fill_default_config(DDP_Players_Factory.generic_main())
        self.user_init() 
        self.test_resources.init_players(self.config)           
        self.user_create_yaml() 
        self.user_set_l7_rules()
        self.set_up_route_counfig()           
        self.run_test()       
        self.general_trex_checker()
