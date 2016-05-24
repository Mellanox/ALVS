#!/usr/bin/env python

from test_infra import *
import random 
import time
import sys

from ez_general.ezpy_exceptions import EZpyFailedToConnectSocket

host1_ip = '10.7.103.30'
host2_ip = '10.7.101.92'

host_ip = host2_ip

app_bin = "/tmp/alvs_daemon"

ezbox = ezbox_host(hostname=host_ip, username='root', password='ezchip')
ezbox.connect()

print
try:
    ezbox.cpe.cp.struct.get_hash_params(STRUCT_ID_NW_ARP)
except EZpyFailedToConnectSocket as e:
    print "Connection to AGT Failed"
    print "Test Passed"
    pass
else:
    print "AGT port is Enabled, Test Failed"
    exit(1)
    
