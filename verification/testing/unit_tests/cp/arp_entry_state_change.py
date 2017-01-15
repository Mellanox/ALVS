    #!/usr/bin/env python


#===============================================================================
# imports
#===============================================================================

# system  
import sys
import os
import inspect
from __builtin__ import enumerate

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

class arp_entry_states(Unit_Tester):

    def user_init(self, setup_num):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        self.test_resources = generic_init(setup_num, service_count, server_count, client_count)
        return dict
    
    def change_config(self, config):
        return
    
    #===============================================================================
    # main function
    #===============================================================================
    
    
    
    def local_compare(self):
        ezbox = self.test_resources['ezbox']
        print "comparing arp tables"
        for i in range(2):
            linux_table = ezbox.get_arp_table()        
            alvs_arp_entries_dict = ezbox.get_all_arp_entries()
            result = ezbox.compare_arp_tables(linux_table,alvs_arp_entries_dict)
            if result==True:
                break;
            elif i==1:
               print "Tables are not the same"
               exit(1)    
    
    
    
    def run_user_test(self):
        
        try:
            ezbox = self.test_resources['ezbox']
            
            state_list = ['permanent', 'delay', 'reachable', 'stale', 'probe', 'failed', 'noarp', 'none', 'incomplete']
    
            invalid_state_list = ['incomplete', 'failed', 'noarp', 'none']
            valid_state_with_timeout = ['delay', 'probe']
            
            
            print "cleaning arp tables"
            ezbox.clean_arp_table()
            
            ip_address = ezbox.get_unused_ip_on_subnet()
            temp_ip_address = ip_address[0]
            i = 0
            mac_address = "01:02:03:04:05:08"
            
            
            
            
            
            for old_state in state_list:
                   
                if old_state in ['delay', 'none', 'probe']:
                    continue
              
                for new_state in state_list:
               
                    if new_state in ['stale'] and old_state not in ['delay', 'probe', 'stale', 'reachable']:
                        continue
                    if old_state == 'stale':
                        continue
                    if old_state == 'reachable' and new_state == 'stale':
                        continue
                    if old_state in ['incomplete', 'failed', 'none']: 
                        if new_state not in ['incomplete', 'failed', 'none']:
                            continue
                      
                    # todo - this one related to bug on none state, need to check also none states
                    if new_state == 'none' or old_state == 'none':
                        continue
                      
                    for j in range(10):
                        temp_ip_address = ip_address[i]
                        i = i + 1
                        mac_address = add2mac(mac_address, 1)
                          
                        result = ezbox.add_arp_entry(temp_ip_address, mac_address)
                        if result[0] == False:
                            print "Test Failed - add entry failed"
                            exit(1)
                                   
                        time.sleep(1)
                          
                        print "Checking change state from %s to %s state" % (old_state, new_state) 
                          
                        result = ezbox.change_arp_entry_state(temp_ip_address, old_state=old_state, new_state=new_state, check_if_state_changed=True)
                        if result[0] == True:
                            break
                        
                          
                        print result[1] ,"Trying one more time"
                          
                    if result[0] == False:
                        print "error while changing states"
                        exit(1)
                     
                    self.local_compare()
                
            
            print "Checking Change state from reachable to stale"
            
            
            
            
            for new_state in state_list:
                
                if new_state in ['none', 'delay', 'probe']:
                    continue
                
    
                for j in range(10):
                    temp_ip_address = ip_address[i]
                    i = i + 1
                    mac_address = add2mac(mac_address, 1)
                        
                    result = ezbox.add_arp_entry(temp_ip_address, mac_address)
                    if result[0] == False:
                         print "Test Failed - add entry failed"
                         exit(1)
                        
                    time.sleep(1)
                        
                    result = ezbox.change_arp_entry_state(temp_ip_address, new_state='reachable', check_if_state_changed=True)
                    if result[0] == True:
                         break
                        
                    print "unable to move to reachable state, trying to change states again"
                        
                if result[0] == False:
                    print "error while changing states"
                    exit(1)
                        
                time.sleep(61)           
                self.local_compare()
                print "Checking change state from reachable to %s state" % (new_state)
                result = ezbox.change_arp_entry_state(temp_ip_address, new_state=new_state, check_if_state_changed=True)
                if result == False:
                    print "error while changing entry state"
                    exit(1) 
                time.sleep(1)
                
                self.local_compare()
                    
        finally:
            print "cleaning arp tables"
            self.test_resources['ezbox'].clean_arp_table()            
                    
                    
        
            
current_test = arp_entry_states()
current_test.main()