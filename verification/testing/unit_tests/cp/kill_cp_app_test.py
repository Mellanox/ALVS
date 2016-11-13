#!/usr/bin/env python

import sys
sys.path.append("../")
from test_infra import *

args = read_test_arg(sys.argv)    

log_file = "kill_cp_app_test.log"
if 'log_file' in args:
    log_file = args['log_file']
init_logging(log_file)

scenarios_to_run = args['scenarios']

ezbox = ezbox_host(management_ip=args['host_ip'], username='root', password='ezchip',  nps_ip=args['nps_ip'], cp_app_bin=args['cp_bin'], dp_app_bin=args['dp_bin'])

if args['hard_reset']:
    ezbox.reset_ezbox(args['ezbox'])

ezbox.connect()
ezbox.terminate_cp_app()
ezbox.reset_chip()
ezbox.copy_cp_bin_to_host()

wait_time_log = []
for i in range(50):
    ezbox.run_cp_app()
    wait_time_log.append(random.random()*2)
    time.sleep(wait_time_log[-1])
    ezbox.terminate_cp_app()
    ezbox.reset_chip()
     
    # todo - check in syslog that program is without errors
 
 
print "the log of the delay time is: " + str(wait_time_log)
