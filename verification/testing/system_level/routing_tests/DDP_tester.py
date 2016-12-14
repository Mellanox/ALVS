# system  
import cmd
import os
import inspect
import sys
import traceback
import abc
import signal
# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir) 


from DDP_infra import *
from tester import *

from multiprocessing import Process

#===============================================================================
# Test Globals
#===============================================================================

class DDP_Tester(Tester):
    __metaclass__  = abc.ABCMeta
    
    @abc.abstractmethod
    def change_config(self, config):
        """change configurantion staticly - must be implemented"""
        pass
    
    def get_test_rc(self):
        #zero means test passed
        return self.test_rc
    
    def clean_all_players(self):
    	#the DDP_tester clean_players
        clean_players(self.test_resources)
    
    def start_test(self):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        
        self.config = fill_default_config(generic_main())
        
        self.change_config(self.config)
        
        self.user_init(self.config['setup_num'])
        
        init_players(self.test_resources, self.config)
        
        self.run_user_test()
        
        print "Test Passed"
        
        self.test_rc = 0
        