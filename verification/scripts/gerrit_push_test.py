#!/usr/bin/env python
import os
import time
import subprocess
import sys
from twisted.python._release import Project

dir =  '/.autodirect/sw_regression/nps_sw/MARS/MARS_conf/setups/ALVS_gen'
sys.path.insert(0,dir) 
from common import *


def parse_command():
    if len(sys.argv)==1:
        print "No args given. Script wont run regression.."
        return "no_regression_run"
    else:
        branch=sys.argv[1]
        print 
        print "========== Args:  =========="
        print "branch    =    %s" %branch
        print "========= END Args ========="
        print
        return branch


def copy_project():
    workspace_dir = '/.autodirect/swgwork/nps_solutions/workspace/mars_push_regression/ALVS'
    print 'copy all to: ' + workspace_dir
    os.system('rm -rf %s'%workspace_dir)
    os.system('mkdir %s'%workspace_dir)
    os.system('cp -r * %s'%workspace_dir)


def exec_regression(branch):
    if branch == "no_regression_run":
        return
    
    if branch == 'master':      # only alvs push regression
        run_regression('alvs')
    elif branch == 'dev/ddp':   #running alvs and ddp push regression
        rc = run_regression('alvs')
        if rc != 0:
            return
        #run_regression('ddp')    #need to build tests
    else:
        print'ERROR: undefined name of branch'
        exit(1)


def run_regression(project):
    print 
    print "Running Regression"
    print "=================="
    setup_num,setup_name = get_setup_num(project,'push')
    cmd = "python2.7 ./verification/MARS/MARS_regression_run.py -s %s -t basic -c 32" %setup_num
    cmd = cmd + " -p /swgwork/nps_solutions/workspace/mars_push_regression/ALVS --project %s" %project
    regression=subprocess.Popen(cmd,shell=True)
    regression.wait()
    process_list.append(["Regression_%s %s"%project,regression.returncode])
    return regression.returncode


def summary(process_list):
    print "########### Summary : ###########"
    exit_code=0
    ret_code = 1
    name = 0
    for process in process_list:
        if process[ret_code] == 0:
            print process[name] + ' Passed'
        else:
            print process[name] + ' Failed'
            exit_code=1
    print "###########           ###########"   
    return exit_code

################################################################################
# Script start point
################################################################################
try:
    exit_code = 1
    
    process_list = []
    
    branch=parse_command()
    
    subprocess.check_call("verification/scripts/make_all.sh all deb", shell=True)
    
    coverity=subprocess.Popen(["verification/scripts/coverity.sh"])
    
    coding_style=subprocess.Popen(["verification/scripts/coding_style.sh"])
    
    copy_project()
    exec_regression(branch)
    
    coverity.wait()
    coding_style.wait()
    
    process_list.append(["Coverity",coverity.returncode])
    process_list.append(["Coding Style",coding_style.returncode])
    exit_code = summary(process_list)
except Exception as error:
    logging.exception(error)
    exit_code = 1
finally:
    exit(exit_code)
