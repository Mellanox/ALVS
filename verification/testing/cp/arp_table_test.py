#!/usr/bin/env python

import sys
sys.path.append("../")
from test_infra import *

args = read_test_arg(sys.argv)    

log_file = "arp_table_test.log"
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
ezbox.run_cp_app()
ezbox.copy_and_run_dp_bin()
    
if 1 in scenarios_to_run:
    # Check the control plane arp table after initialization, all entries before loading the app should be inside cp arp table after loading
    sys.stdout.write("Scenarios 1 - Checking CP ARP table after loading the cp app - ") 
    ip_address = ezbox.get_unused_ip_on_subnet()
    mac_address = 'AA:AA:AA:AA:AA:AA'
 
    ezbox.terminate_cp_app()
    ezbox.reset_chip()
     
    time.sleep(5)
    
    result = ezbox.add_arp_entry(ip_address[1], mac_address)
    if result == False:
        print "\nTest Failed - adding a static entry failed"
        exit(1)
      
    ezbox.get_arp_table()
      
    ezbox.run_cp_app()
    
    time.sleep(3)
    ezbox.get_cp_arp_table()
      
    result = ezbox.compare_arp_tables(update_arp_entries=False)
    if result == False:
        print
        print "\nTest Failed - comparing cp arp table to the linux arp table failed, compare was made after initialize"
        exit(1)
    print "Passed"
  
if 2 in scenarios_to_run:
    # checking cp arp table after adding a static entry
    sys.stdout.write("Scenario 2 - Checking CP ARP table after adding a static ARP entry - ")
    ip_address = ezbox.get_unused_ip_on_subnet() 
    result = ezbox.add_arp_entry(ip_address[0], "AA:BB:CC:DD:00:11")
    if result == False:
        print "\nTest Failed - adding a static entry failed"
        exit(1)
        
    result = ezbox.check_cp_entry_exist(ip_address[0], "AA:BB:CC:DD:00:11", update_arp_entries=True)
    if result == False:
        print "\nTest Failed - adding a static entry failed"
        exit(1)
    print "Passed"
   
if 3 in scenarios_to_run:
    # checking cp arp table after removing a static entry 
    sys.stdout.write("Scenario 3 - Checking CP ARP table after removing a static ARP entry - ") 
    ezbox.delete_arp_entry(ip_address[0])
    result = ezbox.check_cp_entry_exist(ip_address[0], "aa:bb:cc:dd:00:00", update_arp_entries=True)
    if result == True:
        print "\nTest Failed - removing a static entry failed"
        exit(1)
    print "Passed"
   
if 4 in scenarios_to_run:
    # checking cp arp table after adding the max num of entries
    sys.stdout.write("Scenario 4 - Checking CP ARP table after adding the maximum number of ARP entries - ") 
    max_entries = 1024
    temp_mac_address = "AA:BB:CC:DD:00:00"
      
    linux_arp = ezbox.get_arp_table() 
    ip_address = ezbox.get_unused_ip_on_subnet() 
     
    max_entries = max_entries - len(ezbox.arp_entries)
     
    for i in range(0, max_entries):
        temp_ip_address = ip_address[i]
        temp_mac_address = add2mac(temp_mac_address,1)
        result = ezbox.add_arp_entry(temp_ip_address, temp_mac_address)
        if result == False:
            print "\nTest Failed - adding a static entry failed"
            exit(1)
           
    result = ezbox.compare_arp_tables(update_arp_entries=True)
          
    if result == False:
        print "\nTest Failed - adding a static entry failed"
        sys.exit(1)
    print "Passed"
  
  
if 5 in scenarios_to_run:
    ip_address = ezbox.get_unused_ip_on_subnet()
     
    # checking cp arp table after adding one more entry than the maximum 
    sys.stdout.write("Scenario 5 - Checking CP ARP table after adding more than the maximum ARP entries - ") 
    result = ezbox.add_arp_entry(ip_address[0], "11:BB:CC:DD:00:00")
    if result == True:
        print "\nTest Failed - add entry should be raise an error"
        exit(1)
     
    result = ezbox.check_cp_entry_exist(ip_address[0], "11:BB:CC:DD:00:00", update_arp_entries=True)
    if result == True:
        print "\nTest Failed - removing a static entry failed"
        sys.exit(1)
    print "Passed"
    # TODO add checking to the return value of linux??
      
      
if 6 in scenarios_to_run:
    # checking cp arp table after deleting all entries 
    sys.stdout.write("Scenario 6 - Checking CP ARP table after deleting all entries - ")
    ip_address = ezbox.get_unused_ip_on_subnet()
#     result = ezbox.add_arp_entry(ip_address[0], "11:BB:CC:DD:00:00")
#     if result == False:
#         print "\nTest Failed - add entry should be raise an error"
#         exit(1)
    ezbox.clean_arp_table()
    time.sleep(10)
    result = ezbox.compare_arp_tables(update_arp_entries=True)
    if result == False:
        print "\nTest Failed - clear all entries failed"
        sys.exit(1)
    print "Passed"
 
if 7 in scenarios_to_run:
    # checking cp arp table after deleting an entry that not exist 
    sys.stdout.write("Scenario 7 - Checking CP ARP table after deleting an entry that not exist - ")
    ezbox.get_arp_table()
    ip_address = ezbox.get_unused_ip_on_subnet()
    ezbox.delete_arp_entry(ip_address[0]) # TODO need to check the exit code, need to return an error
    ezbox.get_cp_arp_table()
    result = ezbox.compare_arp_tables(update_arp_entries=False)
    if result == False:
        print "\nTest Failed - removing a static entry failed"
        sys.exit(1)
    print "Passed"
  
  
  
  
if 8 in scenarios_to_run:
    # checking cp arp table after editing an entry 
    sys.stdout.write("Scenario 8 - Checking CP ARP table after editing an arp entry - ") 
    ip_address = ezbox.get_unused_ip_on_subnet() 
    temp_ip_address = ip_address[0]
    result = ezbox.add_arp_entry(temp_ip_address, "01:02:03:04:05:06")
    if result == False:
        print "\nTest Failed - adding a static entry failed"
        exit(1)
    result = ezbox.add_arp_entry(temp_ip_address, "01:02:03:04:05:07")
    if result == False:
        print "\nTest Failed - adding a static entry failed"
        exit(1)
         
    result = ezbox.check_cp_entry_exist(temp_ip_address, "01:02:03:04:05:07", update_arp_entries=True)
    if result == False:
        print "\nTest Failed - editing a static entry failed"
        sys.exit(1)
    print "Passed"
     
 
if 9 in scenarios_to_run:
    # checking cp arp table after adding and removing arp entries in a loop
    sys.stdout.write("Scenario 9 - Checking CP ARP table after adding and removing entries in a loop - ") 
    mac_address = "AA:02:03:04:05:07"
    num_of_entries_in_system = 0
    for i in range(5):
     
        ip_address = ezbox.get_unused_ip_on_subnet()
             
        random_number = random.randint(1,100)
 
        logging.log(logging.DEBUG,"Add %d arp entries"%random_number)
#         print "random_number is " + str(random_number)
        for j in range(random_number):
            temp_ip_address = ip_address[j]
            mac_address = add2mac(mac_address, 1)
            result = ezbox.add_arp_entry(temp_ip_address, mac_address)
            if result == False:
                print "\nTest Failed - adding a static entry failed"
                exit(1)
                 
        time.sleep(1)
         
        logging.log(logging.DEBUG,"Comparing linux table to CP table")
         
        result = ezbox.compare_arp_tables(update_arp_entries=True)
        if result == False:
            print "\nTest Failed - removing a static entry failed"
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
            print "\nTest Failed - removing a static entry failed"
            sys.exit(1)
        
 
    print "Passed"



        

