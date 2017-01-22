#!/usr/bin/env python

import sys
import os
import getpass
from optparse import OptionParser
from pexpect import pxssh
import traceback
import logging


parentdir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ALVSdir = os.path.dirname(parentdir)
mars_server = pxssh.pxssh()

def get_def_caller():
    return 'manual_' + getpass.getuser()

def create_command(options):
    if options.project_name != 'alvs':
        test_file = options.project_name + '.setup'
    else:
        if options.regression_type == 'unit':
            test_file = 'alvs_unit.setup'
        else:
            test_file = 'alvs_sys.setup'
    
    cmd = 'python2.7 '\
          '/.autodirect/MARS/production/mlnx_autotest/tools/mars_cli/mini_regression.py '\
          '--cmd start '\
          '--setup solution_setup' + options.setup_num + \
          ' --conf ' + test_file + \
          ' --meinfo_regression_type=' + options.caller + '_' + options.project_name + \
          ' --meinfo_tests_src_path='
    
    if options.path:
        cmd += options.path
    else:
        cmd += ALVSdir[12:]
    
    if options.tags:
        cmd += ' --enable_tags "('
        for tag in options.tags:
            cmd += tag + ','
        cmd = cmd[:len(cmd)-1] + ')"'
    
    if options.file_name:
        cmd += ' --meinfo_file_name='
        cmd += options.file_name
        
    if options.cpu_count:
        cmd += ' --meinfo_cpu_count='
        cmd += options.cpu_count
    return cmd
    
def run_regression(cmd):
    global mars_server
    mars_host_name = 'mtl-stm-76'
    if not mars_server.login (mars_host_name,'root','3tango'):
        err_msg = "SSH session: ERROR - Failed to login to " + mars_host_name
        raise RuntimeError(err_msg)
    else:
        print "SSH session: Successful login to " + mars_host_name
        print "Executing:"
        print cmd
        mars_server.sendline (cmd)
        mars_server.prompt(timeout=36000)         # match the prompt
        if '*Session RC: 1' in mars_server.before:
            err_msg = "Regression Failed"
            raise RuntimeError(err_msg)
        
        
    
################################################################################
# Function: Main
################################################################################
def main():
    
    usage = "usage: %prog [-s, -p, -t, -f ,-c]"
    
    parser = OptionParser(usage=usage, version="%prog 1.0")
    cpus_choices=['32','64','128','256','512','1024','2048','4096']
    parser.add_option("-s", "--setup_num", dest="setup_num",
                      help="Setup number. range (1..7)					(Mandatory parameter)")
    parser.add_option("--type", dest="regression_type", default = 'system',
                      help="Set type to system or unit 						(default is system)")
    parser.add_option("-p", "--path", dest="path", 
                      help="Set path to ALVS directory 						(default is your current directory)")
    parser.add_option("--project", dest="project_name", default = 'alvs',
                      help="Set type of project alvs/ddp..                  (default is alvs)")
    parser.add_option("--caller", dest="caller", default = get_def_caller(),
                      help="regression caller (manual / push)")
    parser.add_option("-t", "--tags", action="append", dest="tags", default = [],
                      help="Filter by tags								(Optional parameter. Few tags:  -t tagA -t tagB)")
    parser.add_option("-f", "--file_name", dest="file_name",
                      help="Installation file name  (.deb)")
    parser.add_option("-c", "--cpu_count", dest="cpu_count",choices=cpus_choices,
                      help="number of CPUs, must be power of 2 between 32-4096")
    (options, args) = parser.parse_args()
    
    if not options.setup_num:
        err_msg = "ERROR: setup_num was not given"
        raise RuntimeError(err_msg)
    
    cmd = create_command(options)
    
    run_regression(cmd)
    

try:
    rc = 0
    main()
except Exception as error:
    logging.exception(error)
    rc = 1
finally:
    print (mars_server.before)
    mars_server.logout()
    exit(rc)
