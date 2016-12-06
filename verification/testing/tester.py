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

from multiprocessing import Process

#===============================================================================
# Test Globals
#===============================================================================

class Tester():
    
    __metaclass__  = abc.ABCMeta
    
    def __init__(self):
        self.test_resources = {'server_list': [],
                               'ezbox': None,
                               'client_list': []}   
        
        # define default configuration 
        self.config = {'setup_num'       : None,  # supply by user
                       'modify_run_cpus' : True,  # in case modify_run_cpus is false, use_4k_cpus ignored
                       'num_of_cpus': "32",
                       'install_package' : True,  # in case install_package is true, copy_binaries ignored
                       'install_file'    : None,
                       'copy_binaries'   : True,
                       'use_director'    : True,
                       'stats'           : False,
                       'start_ezbox'     : False,
                       'stop_ezbox'      : False}
        
        self.gen_rc = False
        self.user_rc = False
        self.test_rc = 1
    
    @abc.abstractmethod
    def run_user_test(self):
        """Run user specific test - must be implemented"""
        pass
    
    @abc.abstractmethod
    def user_init(self, setup_num):
        """User initialization - must be implemented"""
        pass
       
    @abc.abstractmethod
    def start_test(self, setup_num):
        """User initialization - must be implemented"""
        pass
       
    @abc.abstractmethod
    def get_test_rc(self):
        """Get test result - must be implemented"""
        pass
    
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
            
            self.handle_term_signals()
            
            self.start_test()
        
        except KeyboardInterrupt:
            print "The test has been terminated, Good Bye"
        
        except Exception as error:
            logging.exception(error)
    
        finally:
            clean_players(self.test_resources, True, self.config['stop_ezbox'])
            exit(self.get_test_rc())
            

    
