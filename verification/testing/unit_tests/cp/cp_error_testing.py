#!/usr/bin/env python

import sys
sys.path.append("../")
import random 
import time
import sys

import logging

log_file = "cp_error_testing.log"

FORMAT = 'Level Num:%(levelno)s %(filename)s %(funcName)s line:%(lineno)d   %(message)s'
logging.basicConfig(format=FORMAT, filename=log_file, level=logging.DEBUG)
logging.info("\n\nStart logging\nCurrent date & time " + time.strftime("%c") + "\n")


host1_ip = '10.7.103.30'
host2_ip = '10.7.101.92'

host_ip = host2_ip

app_bin = "/tmp/cp_app"

ezbox = ezbox_host(hostname=host_ip, username='root', password='ezchip')
ezbox.connect()
# ezbox.run_cp_app(app_bin)

scenarios = [1]

# Add the maximum amount of arp entries in the control plane
if 1 in scenarios:
    
    sys.stdout.write("Checking CP ARP table error case, adding en entry when the controlne arp table is full - ")
    max_arp_cp_entries = 80000
    
    unused_ip_adress = ezbox.get_unused_ip_on_subnet("255.255.248.0")
                     
    temp_ip_address = '10.10.10.10'
    for i in range (0,max_arp_cp_entries):
        print "inserting cp entry number %d"%i
#         temp_ip_address = unused_ip_adress[i]
        temp_ip_address = add2ip(temp_ip_address, 1)
        key = hex(ip2int(temp_ip_address))
        key = key[2:]
        key = '0'*(8-len(key)) + key
        key = list(key)
        key = ' '.join(key)
        result = '0 0 0 0 0 0 0 0 0 0 0 0 f f f f' 
        ezapi_entry = { 'key' : key,  'result' : result,  'rank' : 0,  'ranges' : 0,  'range_params' : [{ 'min_value' : 0,  'max_value' : 0}, { 'min_value' : 0,  'max_value' : 0}, { 'min_value' : 0,  'max_value' : 0}, { 'min_value' : 0,  'max_value' : 0}, { 'min_value' : 0,  'max_value' : 0}, { 'min_value' : 0,  'max_value' : 0}, { 'min_value' : 0,  'max_value' : 0}, { 'min_value' : 0,  'max_value' : 0}]}
        ezapi_entry_aprams = { 'enforce' : False,  'immediate' : False}
        ezbox.cpe.cp.struct.add_entry(struct_id = STRUCT_ID_NW_ARP,  channel_map = '01000000',  entry = ezapi_entry,  entry_params = ezapi_entry_aprams )
#         result = ezbox.add_arp_entry(temp_ip_address, "a:1:2:3:4:5")    
    
    # add an arp entry in linux
    unused_ip_adress = ezbox.get_unused_ip_on_subnet("255.255.248.0")
    result = ezbox.add_arp_entry(unused_ip_adress[0], "a:1:2:3:4:5")
    if result == False:
        print "Error - add entry finished with error exit code"
        exit(1)

    ezbox.get_cp_arp_table()
    
    print unused_ip_adress[0]
    
    result = ezbox.check_cp_entry_exist(unused_ip_adress[0], "a:1:2:3:4:5", True)
    
    print result
    
    print "Passed"


if 2 in scenarios:
    # trying to delete an entry that is not exist on cp
    ezbox.clean_arp_table()
    
    unused_ip_adress = ezbox.get_unused_ip_on_subnet("255.255.248.0")
    
    print unused_ip_adress[1]
    
    result = ezbox.add_arp_entry(unused_ip_adress[1], "a:1:2:3:4:5")
    if result == False:
        print "Error - add entry finished with error exit code"
        exit(1)
    key = hex(ip2int(unused_ip_adress[1]))
    key = key[2:]
    key = '0'*(8-len(key)) + key
    key = list(key)
    key = ' '.join(key)
    result = '0 0 0 0 0 0 0 0 0 0 0 0 f f f f' 
    ezapi_entry = { 'key' : key,  'result' : result,  'rank' : 0,  'ranges' : 0,  'range_params' : [{ 'min_value' : 0,  'max_value' : 0}, { 'min_value' : 0,  'max_value' : 0}, { 'min_value' : 0,  'max_value' : 0}, { 'min_value' : 0,  'max_value' : 0}, { 'min_value' : 0,  'max_value' : 0}, { 'min_value' : 0,  'max_value' : 0}, { 'min_value' : 0,  'max_value' : 0}, { 'min_value' : 0,  'max_value' : 0}]}
    ezapi_entry_aprams = { 'enforce' : False,  'immediate' : False}
    
    ezbox.get_cp_arp_table()
    print ezbox.cp_arp_dict
    ezbox.cpe.cp.struct.delete_entry(struct_id = STRUCT_ID_NW_ARP,  channel_map = '01000000',  entry = ezapi_entry,  entry_params = ezapi_entry_aprams )
    
    result = ezbox.check_cp_entry_exist(unused_ip_adress[1], "a:1:2:3:4:5", update_arp_entries=True)
    if result == True:
        print "Error while trying to delete arp entry with agt"
        exit(1)

    print ezbox.cp_arp_dict
     
    ezbox.delete_arp_entry(unused_ip_adress[1])
 
    ezbox.get_cp_arp_table()
    print ezbox.cp_arp_dict    
    
    #todo - check for an warning/notice message on syslog 
    
print "Passed"




















  