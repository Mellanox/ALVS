#!/usr/bin/env python

from test_infra import *
import random 
import time
import sys
import logging

log_file = "alvs_unsupported_features_test.log"
FORMAT = 'Level Num:%(levelno)s %(filename)s %(funcName)s line:%(lineno)d   %(message)s'
logging.basicConfig(format=FORMAT, filename=log_file, filemode='w+', level=logging.DEBUG)
print "dd"
logging.info("\n\nStart logging\nCurrent date & time " + time.strftime("%c") + "\n")
print "dd1"

host1_ip = '10.7.103.30'
host2_ip = '10.7.101.92'

host_ip = host2_ip

app_bin = "/tmp/alvs_daemon"

ezbox = ezbox_host(hostname=host_ip, username='root', password='ezchip')
ezbox.connect()
# ezbox.run_cp_app(app_bin)

# ezbox.terminate_cp_app(app_bin)

scenarios = [1]