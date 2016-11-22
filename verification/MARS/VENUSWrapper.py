#!/usr/bin/env python

# Built-in modules
import sys
import os

# Local modules
from reg2_wrapper.test_wrapper.standalone_wrapper import StandaloneWrapper

class VENUSWrapper(StandaloneWrapper):

    def get_prog_path(self):
        prog = "venus_runner.py"
        return prog

    def configure_parser(self):
        super(VENUSWrapper, self).configure_parser()
        self.add_cmd_argument('-t', help='case to run')
        self.add_cmd_argument('-u', help='unit test')
        self.add_cmd_argument('-a', help='application name')
        self.add_test_attribute_argument('--topo_file', 'topo_file', separator=' ') 

if __name__ == "__main__":
    venus_wrapper = VENUSWrapper("VENUS")
    venus_wrapper.execute(sys.argv[1:])
