#!/usr/bin/env python

# Built-in modules
import sys
import os

# Local modules
from reg2_wrapper.test_wrapper.standalone_wrapper import StandaloneWrapper

class install_wrapper(StandaloneWrapper):

    def get_prog_path(self):
        prog = "python2.7 install.py"
        return prog

    def configure_parser(self):
        super(install_wrapper, self).configure_parser()
        self.add_cmd_argument('-f',  help='Installation file name ')
        self.add_cmd_argument('-p',  help='Installation project name ')
        self.add_cmd_argument('-c',  help='number of CPUs')
        self.add_test_attribute_argument('--topo_file', 'topo_file', separator=' ') 

if __name__ == "__main__":
    wrapper = install_wrapper("install")
    wrapper.execute(sys.argv[1:])
