#!/usr/bin/env python

import sys
sys.path.append("verification/testing")
from test_infra import * 
import random

args = read_test_arg(sys.argv)    

log_file = "host_to_network_test.log"
if 'log_file' in args:
    log_file = args['log_file']
init_logging(log_file)

ezbox = ezbox_host(args['setup_num'])

if args['hard_reset']:
    ezbox.reset_ezbox()

# init ALVS daemon
ezbox.connect()
ezbox.terminate_cp_app()
ezbox.reset_chip()
ezbox.flush_ipvs()
ezbox.copy_cp_bin('bin/alvs_daemon')
ezbox.run_cp()
ezbox.copy_dp_bin('bin/alvs_dp')
ezbox.wait_for_cp_app()
ezbox.run_dp()

ip_list = get_setup_list(args['setup_num'])
ezbox.flush_ipvs()    

print "Flush all arp entries"
ezbox.clean_arp_table()

# create client
client_object = client(management_ip=ip_list[0]['hostname'], data_ip=ip_list[0]['ip'])

print "Checking ping from host to one of the VMs"
result,output = ezbox.execute_command_on_host('ping ' + client_object.data_ip + ' & sleep 10s && pkill -HUP -f ping')
print output
    
time.sleep(11)

print "Reading all the arp entries"
arp_entries = ezbox.get_all_arp_entries() 

print "The Arp Table:"
print arp_entries

print "\nChecking if the VM arp entry exist on arp table"
if client_object.data_ip not in arp_entries:
    print "ERROR, arp entry is not exist, ping from host to VM was failed\n"
    exit(1)
    
print "Arp entry exist\n"
print "Test Passed"
