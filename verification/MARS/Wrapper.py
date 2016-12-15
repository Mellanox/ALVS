#!/usr/bin/env python

# Built-in modules
import sys
import os

# Local modules
from reg2_wrapper.test_wrapper.standalone_wrapper import StandaloneWrapper
parentdir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

class Wrapper(StandaloneWrapper):

    def get_prog_path(self):
        prog = "test_runner.py"
        return prog

    def configure_parser(self):
        super(Wrapper, self).configure_parser()
        self.add_cmd_argument('-t',  help='Test to run')
        self.add_cmd_argument('--env',  help='test environment', alias='-e')
        self.add_test_attribute_argument('--topo_file', 'topo_file', separator=' ') 

if __name__ == "__main__":
    wrapper = Wrapper("test_runner")
    wrapper.execute(sys.argv[1:])
