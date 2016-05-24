#!/usr/bin/env python

from test_infra import *
import random 
import time
import sys

import logging

log_file = "arp_table_test.log"

FORMAT = 'Level Num:%(levelno)s %(filename)s %(funcName)s line:%(lineno)d   %(message)s'
logging.basicConfig(format=FORMAT, filename=log_file, level=logging.DEBUG)
logging.info("\n\nStart logging\nCurrent date & time " + time.strftime("%c") + "\n")

scenarios = [1,2,3,6,7,8,9]
scenarios = [4]

host1_ip = '10.7.103.30'
host2_ip = '10.7.101.92'

subnet_mask = '255.255.248.0'

host_ip = host2_ip
host_mac = '00:1A:4A:DE:78:3B'

app_bin = "/tmp/alvs_daemon"

ezbox = ezbox_host(hostname=host_ip, username='root', password='ezchip')
ezbox.connect()

if 1 in scenarios:
    # Check the control plane arp table after initialization, all entries before loading the app should be inside cp arp table after loading
    ip_address = ezbox.get_unused_ip_on_subnet(subnet_mask)
    mac_address = 'AA:AA:AA:AA:AA:AA'
    # add 30 static entries
    for i in range(1):
        result = ezbox.add_arp_entry(ip_address[i], mac_address)
        if result == False:
            print "Test Failed - adding a static entry failed"
            exit(1)
    #     if int(a) != 0:
    #         print "Exit Code "
    #         print "Add an arp entry on linux failed"
    #         exit(1)  
        mac_address = add2mac(mac_address,1)
         
     
     
    ezbox.get_arp_table()
     
    ezbox.run_cp_app(app_bin)
     
    ezbox.get_cp_arp_table()
       
     
    sys.stdout.write("Checking CP ARP table after loading the cp app - ") 
    result = ezbox.compare_arp_tables(update_arp_entries=False)
    if result == False:
        print
        print "Test Failed - comparing cp arp table to the linux arp table failed, compare was made after initialize"
        exit(1)
    print "Passed"
 
if 2 in scenarios:
    # checking cp arp table after adding a static entry
    sys.stdout.write("Checking CP ARP table after adding a static ARP entry - ")
    ip_address = ezbox.get_unused_ip_on_subnet(subnet_mask) 
    result = ezbox.add_arp_entry(ip_address[0], "AA:BB:CC:DD:00:00")
    if result == False:
        print "Test Failed - adding a static entry failed"
        exit(1)
        
    result = ezbox.check_cp_entry_exist(ip_address[0], "AA:BB:CC:DD:00:00", update_arp_entries=True)
    if result == False:
        print "Test Failed - adding a static entry failed"
        exit(1)
    print "Passed"
  
if 3 in scenarios:
    # checking cp arp table after removing a static entry 
    sys.stdout.write("Checking CP ARP table after removing a static ARP entry - ") 
    ezbox.delete_arp_entry(ip_address[0])
    result = ezbox.check_cp_entry_exist(ip_address[0], "aa:bb:cc:dd:00:00", update_arp_entries=True)
    if result == True:
        print "Test Failed - removing a static entry failed"
        exit(1)
    print "Passed"
  
if 4 in scenarios:
    # checking cp arp table after adding the max num of entries
    sys.stdout.write("Checking CP ARP table after adding the maximum number of ARP entries - ") 
    max_entries = 1024
    temp_mac_address = "AA:BB:CC:DD:00:00"
     
    linux_arp = ezbox.get_arp_table() 
    ip_address = ezbox.get_unused_ip_on_subnet(subnet_mask) 
    
    max_entries = max_entries - len(ezbox.arp_entries)
    
    print "max entries is: " + str(max_entries)
    
    for i in range(0, max_entries):
        temp_ip_address = ip_address[i]
        temp_mac_address = add2mac(temp_mac_address,1)
        result = ezbox.add_arp_entry(temp_ip_address, temp_mac_address)
        if result == False:
            print "Test Failed - adding a static entry failed"
            exit(1)
          
    result = ezbox.compare_arp_tables(update_arp_entries=True)
         
    if result == False:
        print "Test Failed - adding a static entry failed"
        sys.exit(1)
    print "Passed"
 
 
if 5 in scenarios:
    ip_address = ezbox.get_unused_ip_on_subnet(subnet_mask)
    
    # checking cp arp table after adding one more entry than the maximum 
    sys.stdout.write("Checking CP ARP table after adding more than the maximum ARP entries - ") 
    result = ezbox.add_arp_entry(ip_address[0], "11:BB:CC:DD:00:00")
    if result == True:
        print "Test Failed - add entry should be raise an error"
        exit(1)
    
    result = ezbox.check_cp_entry_exist(ip_address[0], "11:BB:CC:DD:00:00", update_arp_entries=True)
    if result == True:
        print "Test Failed - removing a static entry failed"
        sys.exit(1)
    print "Passed"
    # TODO add checking to the return value of linux??
     
     
if 6 in scenarios:
    # checking cp arp table after deleting all entries 
    sys.stdout.write("Checking CP ARP table after deleting all entries - ")
    ip_address = ezbox.get_unused_ip_on_subnet(subnet_mask)
    result = ezbox.add_arp_entry(ip_address[0], "11:BB:CC:DD:00:00")
    if result == False:
        print "Test Failed - add entry should be raise an error"
        exit(1)
    ezbox.clean_arp_table()
    result = ezbox.compare_arp_tables(update_arp_entries=True)
    if result == False:
        print "Test Failed - clear all entries failed"
        sys.exit(1)
    print "Passed"

if 7 in scenarios:
    # checking cp arp table after deleting an entry that not exist 
    sys.stdout.write("Checking CP ARP table after deleting an entry that not exist - ")
    ezbox.get_arp_table()
    ezbox.delete_arp_entry("111.111.111.111") # TODO need to check the exit code, need to return an error
    ezbox.get_cp_arp_table()
    result = ezbox.compare_arp_tables(update_arp_entries=False)
    if result == False:
        print "Test Failed - removing a static entry failed"
        sys.exit(1)
    print "Passed"
 
 
 
 
if 8 in scenarios:
    # checking cp arp table after editing an entry 
    sys.stdout.write("Checking CP ARP table after editing an arp entry - ") 
    ip_address = ezbox.get_unused_ip_on_subnet(subnet_mask) 
    temp_ip_address = ip_address[0]
    result = ezbox.add_arp_entry(temp_ip_address, "01:02:03:04:05:06")
    if result == False:
        print "Test Failed - adding a static entry failed"
        exit(1)
    result = ezbox.add_arp_entry(temp_ip_address, "01:02:03:04:05:07")
    if result == False:
        print "Test Failed - adding a static entry failed"
        exit(1)
        
    result = ezbox.check_cp_entry_exist(temp_ip_address, "01:02:03:04:05:07", update_arp_entries=True)
    if result == False:
        print "Test Failed - editing a static entry failed"
        sys.exit(1)
    print "Passed"
    

if 9 in scenarios:
    # checking cp arp table after adding and removing arp entries in a loop
    sys.stdout.write("Checking CP ARP table after adding and removing entries in a loop - ") 
    mac_address = "AA:02:03:04:05:07"
    num_of_entries_in_system = 0
    for i in range(10):
    
        ip_address = ezbox.get_unused_ip_on_subnet(subnet_mask)
            
        random_number = random.randint(1,100)

        logging.log(logging.DEBUG,"Add %d arp entries"%random_number)

        for j in range(random_number):
            temp_ip_address = ip_address[j]
            mac_address = add2mac(mac_address, 1)
            result = ezbox.add_arp_entry(temp_ip_address, mac_address)
            if result == False:
                print "Test Failed - adding a static entry failed"
                exit(1)
                
        time.sleep(1)
        
        logging.log(logging.DEBUG,"Comparing linux table to CP table")
        
        result = ezbox.compare_arp_tables(update_arp_entries=True)
        if result == False:
            print "Test Failed - removing a static entry failed"
            sys.exit(1)
            
        num_of_entries_in_system += random_number
        
        time.sleep(1)

        random_number = random.randint(1,random_number)
        logging.log(logging.DEBUG,"Delete %d arp entries"%random_number)
        
        for j in range(random_number):
            temp_ip_address = ip_address[j]
            ezbox.delete_arp_entry(temp_ip_address)

        time.sleep(1)
        logging.log(logging.DEBUG,"Comparing linux table to CP table")
        result = ezbox.compare_arp_tables(update_arp_entries=True)
        if result == False:
            print "Test Failed - removing a static entry failed"
            sys.exit(1)
       

    print "Passed"


# sys.exit(0)

        

