#!/usr/bin/env python
from test_infra import *
import random 
import time
import sys
import logging

host1_ip = '10.7.103.30'
host2_ip = '10.7.101.92'

host_ip = host2_ip

app_bin = "/tmp/alvs_daemon"

log_file = "alvs_unsupported_features_test.log"
FORMAT = 'Level Num:%(levelno)s %(filename)s %(funcName)s line:%(lineno)d   %(message)s'
logging.basicConfig(format=FORMAT, filename=log_file, filemode='w+', level=logging.DEBUG)
print "dd"
logging.info("\n\nStart logging\nCurrent date & time " + time.strftime("%c") + "\n")
print "dd1"

ezbox = ezbox_host(hostname=host_ip, username='root', password='ezchip')
ezbox.connect()

# add ipv6 arp entry
cmd = 'ip -6 neigh add lladdr a:a:a:a:a:a dev eth2 fe80::a539:c48e:4c21:2f18'

ezbox.run_cp_app(cmd)

result = ezbox.compare_arp_tables(update_arp_entries=True)
if result == False:
    print "Error - arp table from linux and control plane is not equal"
    exit(1)
    

