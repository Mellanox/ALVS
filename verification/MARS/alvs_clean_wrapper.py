#!/usr/bin/env python

# Built-in modules
import sys
import os

# Local modules
from reg2_wrapper.test_wrapper.standalone_wrapper import StandaloneWrapper

class alvs_clean_wrapper(StandaloneWrapper):

    def get_prog_path(self):
        prog = "alvs_clean.py"
        return prog

    def configure_parser(self):
        super(alvs_clean_wrapper, self).configure_parser()
        self.add_test_attribute_argument('--topo_file', 'topo_file', separator=' ')

if __name__ == "__main__":
    alvs_wrapper = alvs_clean_wrapper("alvs_clean")
    alvs_wrapper.execute(sys.argv[1:])
