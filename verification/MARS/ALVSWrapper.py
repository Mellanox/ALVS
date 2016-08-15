#!/usr/bin/env python

# Built-in modules
import sys
import os

# Local modules
from reg2_wrapper.test_wrapper.standalone_wrapper import StandaloneWrapper

class ALVSWrapper(StandaloneWrapper):

    def get_prog_path(self):
        prog = "test_runner.py"
        return prog

    def configure_parser(self):
        super(ALVSWrapper, self).configure_parser()
        self.add_cmd_argument('-t',  help='Test to run')
        self.add_test_attribute_argument('--topo_file', 'topo_file', separator=' ') 

if __name__ == "__main__":
    alvs_wrapper = ALVSWrapper("ALVS")
    alvs_wrapper.execute(sys.argv[1:])
