#!/usr/bin/env python

#===============================================================================
# imports
#===============================================================================
# system  
import cmd
from collections import namedtuple
import logging
from optparse import OptionParser
import os
import inspect
import sys
import traceback
import re
from time import gmtime, strftime
from os import listdir
from os.path import isfile, join
import copy
import abc
import signal
from netaddr import *
# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir) 

from common_infra import *
from server_infra import *
from client_infra import *
from test_infra import *
from e2e_infra import *
from trex_infra_utils import *
from multiprocessing import Process

#===============================================================================
# Test Globals
#===============================================================================
class TrexTester():
    
    __metaclass__  = abc.ABCMeta
    
    def __init__(self):
        self.test_resources = {'ezbox': None,
                               'trex': None}   
        
        # define default configuration 
        self.config = {'setup_num'       : None,  # supply by user
                       'modify_run_cpus' : True,  # in case modify_run_cpus is false, use_4k_cpus ignored
                       'use_4k_cpus'     : False,
                       'install_package' : True,  # in case install_package is true, copy_binaries ignored
                       'install_file'    : None,
                       'copy_binaries'   : True,
                       'use_director'    : False,
                       'start_ezbox'     : False,
                       'stop_ezbox'      : False}
       
        self.test_result = TrexTestResult()
        
    @abc.abstractmethod
    def user_init(self):
        """User initialization - must be implemented"""
        pass
    
    @abc.abstractmethod
    def user_create_yaml(self):
        """yaml creation - must be implemented"""
        pass
    
    @abc.abstractmethod
    def run_user_test(self):
        """Run user specific test - must be implemented"""
        pass
    
    #@abc.abstractmethod
    #def run_user_checker(self, log_dir):
        """Run user specific checker - must be implemented"""
    #    pass 
    
    def gen_init(self):
        self.handle_term_signals()  
        self.config = self.get_config()  
           
    def get_config(self):
        config = fill_default_config(generic_main())
        config['use_director'] = False
        return config
    
    def run_test(self):
        ezbox = self.test_resources['ezbox']
        trex = self.test_resources['trex']
        trex_result = trex.run_trex_test()
        ez_stats = get_ezbox_stats(ezbox)        
        self.test_result.set_result(trex_result,ez_stats, self.test_resources)

    def general_trex_checker(self):
        self.test_result.general_checker()
    
    def signal_handler(self, signum, frame):
        #Ignore termination signals while executing clean up
        signal.signal(signal.SIGTERM,signal.SIG_IGN)
        signal.signal(signal.SIGINT,signal.SIG_IGN)
        raise KeyboardInterrupt
    
    def handle_term_signals(self):
        signal.signal(signal.SIGTERM, self.signal_handler)
        signal.signal(signal.SIGINT, self.signal_handler)
    
    def main(self):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        
        try:
            
            self.gen_init()
            
            self.user_init()
            
            self.user_create_yaml()
            
            init_trex_players(self.test_resources, self.config)
            
            self.run_user_test()
            
            self.general_trex_checker()
        
        except KeyboardInterrupt:
            print "The test has been terminated, Good Bye"
        
        except Exception as error:
            logging.exception(error)
            
        finally:
            clean_trex_players(self.test_resources, True, self.config['stop_ezbox'])
            exit(self.test_result.get_rc())
            

    