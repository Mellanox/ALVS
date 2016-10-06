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
                       'use_4k_cpus'     : False,
                       'install_package' : True,  # in case install_package is true, copy_binaries ignored
                       'install_file'    : None,
                       'copy_binaries'   : True,
                       'use_director'    : True,
                       'start_ezbox'     : False,
                       'stop_ezbox'      : False}
        
        self.gen_rc = False
        self.user_rc = False
        
    @abc.abstractmethod
    def user_init(self, setup_num):
        """User initialization - must be implemented"""
        pass
    
    @abc.abstractmethod
    def run_user_test(self):
        """Run user specific test - must be implemented"""
        pass
    
    @abc.abstractmethod
    def run_user_checker(self, log_dir):
        """Run user specific checker - must be implemented"""
        pass 
    
    def get_test_rc(self):
        if self.gen_rc and self.user_rc:
            return 0
        else:
            return 1
        
    def print_test_result(self):
        if self.gen_rc and self.user_rc:
            print 'Test passed !!!'
        else:
            print 'Test failed !!!'
            
    def deep_copy_server_list(self):
        """Refresh ssh objects before deep copying,
           in order to avoid data corruption"""
        for s in self.test_resources['server_list']:
            if s.ssh.connection_established:
                s.ssh.recreate_ssh_object()
        return copy.deepcopy(self.test_resources['server_list'])
    
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
            
            self.config = fill_default_config(generic_main())
            
            self.user_init(self.config['setup_num'])
            
            init_players(self.test_resources, self.config)
            
            self.run_user_test()
            
            log_dir = collect_logs(self.test_resources)
            
            self.gen_rc = general_checker(self.test_resources)
            
            self.user_rc = self.run_user_checker(log_dir)
            
            self.print_test_result()
        
        except KeyboardInterrupt:
            print "The test has been terminated, Good Bye"
        
        except Exception as error:
            logging.exception(error)
            
        finally:
            clean_players(self.test_resources, True, self.config['stop_ezbox'])
            exit(self.get_test_rc())
            

    