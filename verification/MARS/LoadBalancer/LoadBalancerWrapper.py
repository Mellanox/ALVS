#!/usr/bin/env python

# Built-in modules
import sys
import os

# Local modules
from reg2_wrapper.test_wrapper.standalone_wrapper import StandaloneWrapper

class LoadBalancerWrapper(StandaloneWrapper):

    def get_prog_path(self):
        prog = "ClientWrapper.py"
        return prog

    def configure_parser(self):
        super(LoadBalancerWrapper, self).configure_parser()
        self.add_cmd_argument('-i',  help='IP of the HTTP server')
        self.add_cmd_argument('-r',  help='Number of HTTP requests', default=1)
        self.add_cmd_argument('--s1', help='Expected response from server 1', default=0)
        self.add_cmd_argument('--s2', help='Expected response from server 2', default=0)
        self.add_cmd_argument('--s3', help='Expected response from server 3', default=0)
        self.add_cmd_argument('--s4', help='Expected response from server 4', default=0)
        self.add_cmd_argument('--s5', help='Expected response from server 5', default=0)
        self.add_cmd_argument('--s6', help='Expected response from server 6', default=0)
        

if __name__ == "__main__":
    load_balancer_wrapper = LoadBalancerWrapper("LoadBalancerWrapper")
    load_balancer_wrapper.execute(sys.argv[1:])
