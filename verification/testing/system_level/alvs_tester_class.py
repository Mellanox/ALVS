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
from alvs_infra import *
from tester import Tester

from multiprocessing import Process

#===============================================================================
# Test Globals
#===============================================================================

class ALVS_Tester(Tester):
    
    __metaclass__  = abc.ABCMeta
    
    def __init__(self):
    	super(ALVS_Tester, self).__init__()
    	self.test_resources = {'server_list': [],
                               'ezbox': None,
                               'client_list': []} 
        
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
    
    def clean_all_players(self):
        clean_players(self.test_resources, True, self.config['stop_ezbox'])
    
    def start_test(self):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        
        self.config = fill_default_config(generic_main())
        
        self.user_init(self.config['setup_num'])
        
        init_players(self.test_resources, self.config)
        
        self.run_user_test()
        
        log_dir = collect_logs(self.test_resources)
        
        self.gen_rc = general_checker(self.test_resources)
        
        self.user_rc = self.run_user_checker(log_dir)
        
        self.print_test_result()
        

    