#!/usr/bin/env python
import os
import fcntl
import inspect
import time
import re
import os
import pwd
import sys
import datetime
from optparse import OptionParser


file_path_to_allocation_files ='/.autodirect/sw_regression/nps_sw/MARS/MARS_conf/setups/ALVS_gen/setup_allocation'
file_path_of_curr_setup = None
fd_of_curr_setup = None


def get_username():
    return pwd.getpwuid(os.getuid())[ 0 ]

def open_file(file_path, permission):
    try:
        fd = open(file_path, permission)
    except:
        print '****** Error openning file %s !!!' % file_path
        exit(1)
    return fd
    
def delete_file_content(file_path):
    fd = open_file(file_path, 'w')
    fd.close()

def lock_file(fd):
    try:
        fcntl.flock(fd,fcntl.LOCK_EX | fcntl.LOCK_NB)
    except IOError:
        return False

    return True


def lock_specific_setup(line):
    global file_path_of_curr_setup
    global fd_of_curr_setup
    
    file_path_of_curr_setup = file_path_to_allocation_files + '/setup' + line.strip() + '.txt'
    fd_of_curr_setup = open_file(file_path_of_curr_setup, 'a+')
    is_locked = lock_file(fd_of_curr_setup)
    
    if is_locked:
        print 'You have locked setup number: ' + line.strip()
        fd_of_curr_setup.write(get_username())
        fd_of_curr_setup.write('    Timestamp: {0}\n'.format(datetime.datetime.now()))
        return True
    fd_of_curr_setup.close()
    
    return False

################################################################################
# Function: Main
################################################################################
def main():
    global file_path_of_curr_setup
    global fd_of_curr_setup
    
    usage = "usage: %prog [-hold, -s, -r, -f, -p, -t]"
    parser = OptionParser(usage=usage, version="%prog 1.0")

    parser.add_option("--hold", dest="hold", default="False",
                      help="Hold setup until released by user")
    parser.add_option("-s", "--setup_num", dest="setup_num",
                      help="Optional, provide the setup number you wish to allocate")
    parser.add_option("-r", "--run_regression", dest="run_regression", default="False",
                      help="Run regression and hold setup until end of regression (default=False)")
    parser.add_option("-f", "--file_name", dest="file_name",
                      help="Installation file name  (.deb). in case run_regression is true")
    parser.add_option("-p", "--path", dest="path",
                      help="Set path to ALVS directory (default is your current directory). in case run_regression is true")
    parser.add_option("-t", "--tag", action="append", dest="tag", default = [],
                      help="Filter tests by tags (Optional parameter. Few tags:  -t tagA -t tagB ). in case run_regressin is true")
    (options, args) = parser.parse_args()


    fd_of_all_setups = open_file(file_path_to_allocation_files + '/setups.txt', 'r')

    is_locked = False
    still_hold = False
    #####   catch setup
    #catch specific setup
    if options.setup_num:
        for line in fd_of_all_setups:
            if line[0] != '#' and line.strip() == options.setup_num:
                is_locked = lock_specific_setup(line)
                if is_locked == False:        #someone use this setup
                    fd_of_curr_setup = open_file(file_path_of_curr_setup, 'r')
                    curr_setup_inf = fd_of_curr_setup.read()
                    user_name = curr_setup_inf.split(" ")[0]
                    if get_username() == user_name:
                        print "You already locked this setup"
                        still_hold = True
                        is_locked = True
                    else:
                        print  "setup " + options.setup_num + " is locked. used by " + user_name\
                            + " since " + curr_setup_inf.partition(' ')[2]
                        fd_of_curr_setup.close()
                        fd_of_all_setups.close()
                        exit(0)
                break
        if is_locked == False:
            print "This setup is unavailable"
            fd_of_all_setups.close()
            exit(0)
    #catch first free setup
    else:
        try:
            while is_locked == False:
                for line in fd_of_all_setups:
                    if line[0] !='#':
                        is_locked = lock_specific_setup(line)
                    
                        if is_locked:
                            break
            
                if is_locked == False:
                    print 'All setups are now in use, trying again in 10 seconds.....'
                    time.sleep(10)
                    fd_of_all_setups.seek(0)
        except KeyboardInterrupt:
            print 
            print "Exit"
            exit(0)
            
            
    if still_hold == False:
        fd_of_curr_setup.flush()
    
    try:    # Ctrl+C options
        #####   run regression
        if options.run_regression.lower() == "true":
            path = ''
            tags = ''
            file_name = ''
            currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
            VERdir = os.path.dirname(currentdir)
            if options.path:
                path = ' -p '+options.path
            if options.tag:
                for tag in options.tag:
                    tags += ' -t '+tag
            if options.file_name:
                file_name += ' -f' + options.file_name
            setup_num = line[:-1]
            os.system(VERdir + '/MARS/MARS_regression_run.py -s '+ setup_num + path + tags + file_name)
        #####   hold setup
        if options.hold.lower() == "true" and still_hold == False:
            raw_input("Press Enter to release setup...")    
    
    except KeyboardInterrupt:
        print 
    
    if still_hold == False:
        fd_of_curr_setup.close()
        delete_file_content(file_path_of_curr_setup)
        print "Setup Released"
        fd_of_all_setups.close()

main()
      