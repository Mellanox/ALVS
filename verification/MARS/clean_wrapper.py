#!/usr/bin/env python

# Built-in modules
import sys
import os

# Local modules
from reg2_wrapper.test_wrapper.standalone_wrapper import StandaloneWrapper

class clean_wrapper(StandaloneWrapper):

    def get_prog_path(self):
        prog = "clean.py"
        return prog

    def configure_parser(self):
        super(clean_wrapper, self).configure_parser()
        self.add_test_attribute_argument('--topo_file', 'topo_file', separator=' ')

if __name__ == "__main__":
    wrapper = clean_wrapper("clean")
    wrapper.execute(sys.argv[1:])
