#!/usr/bin/env python

#import urllib2
import sys
import os
from optparse import OptionParser
from pexpect import pxssh


parentdir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ALVSdir = os.path.dirname(parentdir)

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
    parser.add_option("-t", "--tags", action="append", dest="tags", default = [],
                      help="Filter by tags								(Optional parameter. Few tags:  -t tagA -t tagB)")
    parser.add_option("-f", "--file_name", dest="file_name",
                      help="Installation file name  (.deb)")
    parser.add_option("-c", "--cpu_count", dest="cpu_count",choices=cpus_choices,
                      help="number of CPUs, must be power of 2 between 32-4096")
    (options, args) = parser.parse_args()
    
    # validate options
    if not options.setup_num:
        print 'ERROR: setup_num was not given'
        exit(1)
    
    if options.regression_type == 'unit':
    	test_file = 'alvs_unit.setup'
    else:
    	test_file = 'alvs_sys.setup'
    	
    command_line = 'python2.7 '\
                   '/.autodirect/MARS/production/mlnx_autotest/tools/mars_cli/mini_regression.py '\
                   '--cmd start '\
                   '--setup solution_setup' + options.setup_num + \
                   ' --conf ' + test_file + \
                   ' --meinfo_tests_src_path='
                   
                   
    if options.path:
        command_line += options.path
    else:
        command_line += ALVSdir[12:]
    
    
    if options.tags:
        command_line += ' --enable_tags "('
        for tag in options.tags:
            command_line += tag + ','
        command_line = command_line[:len(command_line)-1] + ')"'
    
    if options.file_name:
        command_line += ' --meinfo_file_name='
        command_line += options.file_name
        
    if options.cpu_count:
        command_line += ' --meinfo_cpu_count='
        command_line += options.cpu_count
    
    rc = 1
    s = pxssh.pxssh()
    if not s.login ('mtl-stm-76','root','3tango'):
        print "SSH session: ERROR - Failed to login to mtl-stm-76."
        print str(s)
    else:
        print "SSH session: Successful login to mtl-stm-76"
        print "Executing:"
        print command_line
        s.sendline (command_line)
        s.prompt(timeout=36000)         # match the prompt
        if '*Session RC: 0' in s.before:
            rc = 0
        print (s.before)     # print everything before the prompt.
        s.logout()
    
    return rc


rc = main()
exit(rc)
