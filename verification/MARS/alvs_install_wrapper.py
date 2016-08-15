#!/usr/bin/env python

# Built-in modules
import sys
import os

# Local modules
from reg2_wrapper.test_wrapper.standalone_wrapper import StandaloneWrapper

class alvs_install_wrapper(StandaloneWrapper):

    def get_prog_path(self):
        prog = "python2.7 alvs_install.py"
        return prog

    def configure_parser(self):
        super(alvs_install_wrapper, self).configure_parser()
        self.add_cmd_argument('-f',  help='Installation file name ')
        self.add_cmd_argument('-c',  help='use 4k cpus true / false')
        self.add_test_attribute_argument('--topo_file', 'topo_file', separator=' ') 

if __name__ == "__main__":
    alvs_wrapper = alvs_install_wrapper("alvs_install")
    alvs_wrapper.execute(sys.argv[1:])
