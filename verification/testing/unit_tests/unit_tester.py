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


from alvs_infra import *
from tester import *
from alvs_players_factory import *
from multiprocessing import Process
from alvs_players_factory import *

#===============================================================================
# Test Globals
#===============================================================================

class Unit_Tester(Tester):
    
    __metaclass__  = abc.ABCMeta
    
    def __init__(self):
    	super(Unit_Tester, self).__init__()
    	self.test_resources = {'server_list': [],
                               'ezbox': None,
                               'client_list': []}
        
    @abc.abstractmethod
    def change_config(self, config):
        """change configurantion staticly - must be implemented"""
        pass
    
    def get_test_rc(self):
        #zero means test passed
        return self.test_rc
    
    def clean_all_players(self):
        ALVS_Players_Factory.clean_players(self.test_resources, True, self.config['stop_ezbox'])
    
    def start_test(self):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        
        self.config = ALVS_Players_Factory.fill_default_config(ALVS_Players_Factory.generic_main())
        
        self.change_config(self.config)
        
        self.user_init(self.config['setup_num'])
        
        ALVS_Players_Factory.init_players(self.test_resources, self.config)
        
        self.run_user_test()
        
        print "Test Passed"
        
        self.test_rc = 0
        
        
            

    