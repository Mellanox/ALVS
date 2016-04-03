#!/usr/bin/env python

# Built-in modules
import sys
import os

# Local modules
from reg2_wrapper.test_wrapper.standalone_wrapper import StandaloneWrapper

class LoadBalancerWrapper(StandaloneWrapper):

    def get_prog_path(self):
        prog = "echo"
        return prog

    def configure_parser(self):
        super(LoadBalancerWrapper, self).configure_parser()
        self.add_cmd_argument('--param', value_only=True, help='Test type')

if __name__ == "__main__":
    load_balancer_wrapper = LoadBalancerWrapper("LoadBalancerWrapper")
    load_balancer_wrapper.execute(sys.argv[1:])
