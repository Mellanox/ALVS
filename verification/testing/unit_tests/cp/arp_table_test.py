#!/usr/bin/env python


#===============================================================================
# imports
#===============================================================================

# system  
import sys
import random
import time
import os
import inspect

# pythons modules 
# local
my_currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
my_parentdir = os.path.dirname(my_currentdir)
my_grandparentdir =  os.path.dirname(my_parentdir)
sys.path.append(my_grandparentdir)
sys.path.append(my_parentdir)
from common_infra import *
from alvs_infra import *
from unit_tester import Unit_Tester

server_count   = 0
client_count   = 0
service_count  = 0


class arp_table_test(Unit_Tester):
    
    
    def user_init(self, setup_num):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        self.test_resources = generic_init(setup_num, service_count, server_count, client_count)
        return dict
        

    def change_config(self, config):
        pass
    

    
    def run_user_test(self):
        try:
            linux_table={}
    
            linux_arp_entries_dict={}
            ezbox =self.test_resources['ezbox']
            print "cleaning arp tables"
            ezbox.clean_arp_table()
            
            
            #if update_arp_entries == True or self.arp_entries == []:
           
            #if update_arp_entries == True or self.cp_arp_entries == []:
            
            # Check the control plane arp table after initialization, all entries before loading the app should be inside cp arp table after loading
            print "Scenarios 1 - Checking CP ARP table after loading the cp app - "
            server_list = self.test_resources['server_list']
            client_list = self.test_resources['client_list']
            vip_list = self.test_resources['vip_list']
            cp_entries_list=[]
                
            linux_table = ezbox.get_arp_table()    
            alvs_arp_entries_dict = ezbox.get_all_arp_entries()
            result = ezbox.compare_arp_tables(linux_table,alvs_arp_entries_dict)
            if result == False:
                print "\nTest Failed - comparing cp arp table to the linux arp table failed, compare was made after initialize"
                exit(1)
            print "Passed"
            
            
            
            # checking cp arp table after adding a static entry
            print "Scenario 2 - Checking CP ARP table after adding a static ARP entry - "
            ip_address = ezbox.get_unused_ip_on_subnet() 
        
            result = ezbox.add_arp_entry(ip_address[0],"AA:BB:CC:DD:00:11")
            if result[0] == False:
                print result[1]
                print "\nTest Failed - adding a static entry failed" 
                exit(1)
            time.sleep(5)
            result = ezbox.check_cp_entry_exist(ip_address[0], "AA:BB:CC:DD:00:11")
            if result == False:
                print "\nTest Failed - adding a static entry failed"
                exit(1)
                
            print "Passed"
              
            
            
            
            # checking cp arp table after removing a static entry 
            print "Scenario 3 - Checking CP ARP table after removing a static ARP entry - " 
            
            ezbox.delete_arp_entry(ip_address[0])
            time.sleep(2)
            result = ezbox.check_cp_entry_exist(ip_address[0], "AA:BB:CC:DD:00:11")
            if result == True:
                print "\nTest Failed - removing a static entry failed"
                exit(1)
            
            print "Passed"
            
            
            
            # checking cp arp table after adding the max num of entries(should be 4k but ther is a known bug)
            print "Scenario 4 - Checking CP ARP table after adding the maximum number of ARP entries - "
            max_entries = 50
            temp_mac_address = "AA:BB:CC:DD:00:00"
              
            ip_address = ezbox.get_unused_ip_on_subnet() 
             
            max_entries = max_entries - len(ezbox.get_arp_table())
            
             
            for i in range(0, max_entries):
                temp_ip_address = ip_address[i]
                temp_mac_address = add2mac(temp_mac_address,1)
                result = ezbox.add_arp_entry(temp_ip_address, temp_mac_address)
                if result[0] == False:
                    print result[1]
                    print "\nTest Failed - adding a static entry failed" 
                    exit(1)
            
            
            
            linux_table = ezbox.get_arp_table()    
            alvs_arp_entries_dict = ezbox.get_all_arp_entries()
            result = ezbox.compare_arp_tables(linux_table,alvs_arp_entries_dict)
                  
            if result == False:
                print "\nTest Failed - adding a static entry failed"
                sys.exit(1)
            print "Passed"
          
          
            
         
            # checking cp arp table after adding one more entry than the maximum - known bug
            """"
            print "Scenario 5 - Checking CP ARP table after adding more than the maximum ARP entries - " 
            ip_address = self.get_unused_ip_on_subnet()
            result = ezbox.add_arp_entry(ip_address[0], "11:BB:CC:DD:00:00")
            if result == True:
                print "\nTest Failed - add entry should be raise an error"
                exit(1)
             
            result = ezbox.check_cp_entry_exist(ip_address[0], "11:BB:CC:DD:00:00")
            if result == True:
                print "\nTest Failed - removing a static entry failed"
                exit(1)
            print "Passed"
            """
           
            # checking cp arp table after deleting all entries 
            print "Scenario 6 - Checking CP ARP table after deleting all entries - "
            ezbox.clean_arp_table()
            time.sleep(10)
            linux_table = ezbox.get_arp_table()    
            alvs_arp_entries_dict = ezbox.get_all_arp_entries()
            result = ezbox.compare_arp_tables(linux_table,alvs_arp_entries_dict)
            if result == False:
                print "\nTest Failed - clear all entries failed"
                exit(1)
            print "Passed"
              
            # checking cp arp table after deleting an entry that not exist 
            print "Scenario 7 - Checking CP ARP table after deleting an entry that not exist - "
            linux_table = ezbox.get_arp_table()    
            ip_address = ezbox.get_unused_ip_on_subnet()
            result=ezbox.delete_arp_entry(ip_address[0]) 
            if result[0] is True:
                print result[1] + " Deleted but not exists"
                exit(1)
            alvs_arp_entries_dict = ezbox.get_all_arp_entries()
            result = ezbox.compare_arp_tables(linux_table,alvs_arp_entries_dict)
            if result == False:
                print "\nTest Failed - removing a static entry failed"
                exit(1)
            print "Passed"    
          
            # checking cp arp table after editing an entry 
            print "Scenario 8 - Checking CP ARP table after editing an arp entry - " 
            ip_address = ezbox.get_unused_ip_on_subnet() 
            temp_ip_address = ip_address[0]
            result = ezbox.add_arp_entry(temp_ip_address, "01:02:03:04:05:06")
            if result[0] == False:
                print "\nTest Failed - adding a static entry failed1"
                exit(1)
            result = ezbox.add_arp_entry(temp_ip_address, "01:02:03:04:05:07")
            if result[0] == False:
                print "\nTest Failed - adding a static entry faile2"
                exit(1)
            
            
            time.sleep(5)
            
            result = ezbox.check_cp_entry_exist(temp_ip_address, "01:02:03:04:05:07")
            if result == False:
                print "\nTest Failed - editing a static entry failed3"
                exit(1)
            print "Passed" 
            
            
            
                
            # checking cp arp table after adding and removing arp entries in a loop
            print "Scenario 9 - Checking CP ARP table after adding and removing entries in a loop - " 
            mac_address = "AA:02:03:04:05:07"
            num_of_entries_in_system = 0
            for i in range(5):
        
                ip_address = ezbox.get_unused_ip_on_subnet()
                     
                random_number = random.randint(1,20)
    
                for j in range(random_number):
                    temp_ip_address = ip_address[j]
                    mac_address = add2mac(mac_address, 1)
                    result = ezbox.add_arp_entry(temp_ip_address, mac_address)
                    if result[0] == False:
                        print "\nTest Failed - adding a static entry failed"
                        exit(1)
                         
                time.sleep(1)
                 
                
                linux_table = ezbox.get_arp_table()    
                alvs_arp_entries_dict = ezbox.get_all_arp_entries()
                result = ezbox.compare_arp_tables(linux_table,alvs_arp_entries_dict)
                if result == False:
                    print "\nTest Failed - removing a static entry failed"
                    exit(1)
                     
                num_of_entries_in_system += random_number
                 
                time.sleep(1)
         
                random_number = random.randint(1,random_number)
              
                 
                for j in range(random_number):
                    temp_ip_address = ip_address[j]
                    ezbox.delete_arp_entry(temp_ip_address)
         
                time.sleep(1)
                
                linux_table = ezbox.get_arp_table()    
                alvs_arp_entries_dict = ezbox.get_all_arp_entries()
                result = ezbox.compare_arp_tables(linux_table,alvs_arp_entries_dict)
                if result == False:
                    print "\nTest Failed - removing a static entry failed"
                    exit(1)
                
         
            print "Passed"
        finally:
            print "cleaning arp tables"
            self.test_resources['ezbox'].clean_arp_table()

        
    
current_test = arp_table_test()
current_test.main()
    
