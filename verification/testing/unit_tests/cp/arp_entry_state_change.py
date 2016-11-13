#!/usr/bin/env python

###############################################################################################################################
########################### this test is checking all the permutation of arp entry state change ################################
###############################################################################################################################

import sys
sys.path.append("../")
from test_infra import *

args = read_test_arg(sys.argv)

log_file = "arp_entry_state_change.log"
if 'log_file' in args:
    log_file = args['log_file']
    
init_logging(log_file)
    
ezbox = ezbox_host(management_ip=args['host_ip'], username='root', password='ezchip',  nps_ip=args['nps_ip'], cp_app_bin=args['cp_bin'], dp_app_bin=args['dp_bin'])

if args['hard_reset']:
    ezbox.reset_ezbox(args['ezbox'])

ezbox.connect()
ezbox.terminate_cp_app()
ezbox.reset_chip()
ezbox.copy_cp_bin_to_host()
ezbox.run_cp_app()

state_list = ['permanent', 'delay', 'reachable', 'stale','probe', 'failed', 'noarp', 'none', 'incomplete']

invalid_state_list = ['incomplete', 'failed', 'noarp', 'none']
valid_state_with_timeout = ['delay', 'probe']

ip_address = ezbox.get_unused_ip_on_subnet()
temp_ip_address = ip_address[0]
i=0
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
            if new_state not in ['incomplete','failed','none']:
                continue
          
        # todo - this one related to bug on none state, need to check also none states
        if new_state == 'none' or old_state == 'none':
            continue
          
        for j in range(10):
            temp_ip_address = ip_address[i]
            i=i+1
            mac_address = add2mac(mac_address, 1)
              
            result = ezbox.add_arp_entry(temp_ip_address, mac_address)
            if result == False:
                print "Test Failed - add entry failed"
                exit(1)
                       
            time.sleep(1)
              
            print "Checking change state from %s to %s state"%(old_state,new_state) 
              
            result = ezbox.change_arp_entry_state(temp_ip_address, old_state=old_state, new_state=new_state, check_if_state_changed=True)
            if result == True:
                break
              
            print "Trying one more time"
              
        if result == False:
            print "error while changing states"
            exit(1)
              
        result = ezbox.check_cp_entry_exist(temp_ip_address, mac_address, update_arp_entries=True)
        print "entry still exist? %s"%result
          
        if new_state in ['delay','probe']:
            if ezbox.verify_arp_state(temp_ip_address, "failed") == True:
                print "Entry state changed to failed"
                # entry should be deleted
                if result == True:
                    print "Test Failed - change state to entry failed, invalid state entry wasnt deleted from cp"
                    exit(1)
                      
            else:
                # entry shouldnt be deleted, entry is probably on stale state
                if result == False:
                    print "Test Failed - change state to entry failed, a valid entry was deleted in cp"
                    exit(1)            
              
            continue
              
        if new_state in invalid_state_list:
            # entry state is invalid, entry is not suppose to be in control plane
            if result == True:
                print "Test Failed - change state to entry failed, invalid state entry wasnt deleted from cp"
                exit(1)
            else:
                print "Entry not exist on control plane"
        else:
            # entry state is valid, entry is suppose to be in control plane
            if result == False:
                print "Test Failed - change state to entry failed, a valid entry was deleted in cp"
                exit(1)  
            else:
                print "Entry still exist on control plane"            

print "Checking Change state from reachable to stale"

for new_state in state_list:
    
    if new_state in ['none','delay','probe']:
        continue
    
    for num_of_tries in range(10): 
        for j in range(10):
            temp_ip_address = ip_address[i]
            i=i+1
            mac_address = add2mac(mac_address, 1)
            
            result = ezbox.add_arp_entry(temp_ip_address, mac_address)
            if result == False:
                print "Test Failed - add entry failed"
                exit(1)
            
            time.sleep(1)
            
            result = ezbox.change_arp_entry_state(temp_ip_address, new_state='reachable', check_if_state_changed=True)
            if result == True:
                break
            
            print "unable to move to reachable state, trying to change states again"
            
        if result == False:
            print "error while changing states"
            exit(1)
            
        time.sleep(61)
        if ezbox.verify_arp_state(temp_ip_address, "stale") == True:
            break
        
        print "error, trying to change states again"
        
    result = ezbox.check_cp_entry_exist(temp_ip_address, mac_address, update_arp_entries=True)
    if result == False:
        print "Test Failed - change state to entry failed, a valid entry was deleted in cp"
        exit(1)
        
    
    print "Checking Change state from stale to %s"%new_state
    
    if ezbox.verify_arp_state(temp_ip_address, "stale") == False:
        print "error while changing entry state"
        exit(1)
    
    result = ezbox.change_arp_entry_state(temp_ip_address, new_state=new_state, check_if_state_changed=True)
    if result == False:
        print "error while changing entry state"
        exit(1) 

    time.sleep(1)
    
    result = ezbox.check_cp_entry_exist(temp_ip_address, mac_address, update_arp_entries=True)
    
    if new_state in invalid_state_list:
        # entry state is invalid, entry is not suppose to be in control plane
        if result == True:
            print "Test Failed - change state to entry failed, invalid state entry wasnt deleted from cp"
            exit(1)
        else:
            print "Entry not exist on control plane"
        
    else:
        # entry state is valid, entry is suppose to be in control plane
        if result == False:
            print "Test Failed - change state to entry failed, a valid entry was deleted in cp"
            exit(1)
        else:
            print "Entry still exist on control plane"  
     
    
print "Test Passed"




