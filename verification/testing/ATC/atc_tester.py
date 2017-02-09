#===============================================================================
# imports
#===============================================================================
# system  
import cmd
import os
import inspect
import sys
import traceback
import re
import abc
# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir) 


from atc_infra import *
from tester import *
from atc_players_factory import *
from multiprocessing import Process

class atc_tester(Tester):
    
    __metaclass__  = abc.ABCMeta
    
    def __init__(self):
    	super(atc_tester, self).__init__()
    	self.test_resources = {'host_list': [],
                               'ezbox': None
                               } 
    
    def get_test_rc(self):
        #zero means test passed
        return self.test_rc
    
    def clean_all_players(self):
        ATC_Players_Factory.clean_players(self.test_resources, True, self.config['stop_ezbox'])
    
    def start_test(self):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        
        self.config = ATC_Players_Factory.fill_default_config(ATC_Players_Factory.generic_main())
        
        self.user_init(self.config['setup_num'])
        
        ATC_Players_Factory.init_players(self.test_resources, self.config)
        
        self.run_user_test()
        
        print "Test Passed"
        
        self.test_rc = 0
      
