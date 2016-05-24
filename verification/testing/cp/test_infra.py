#!/usr/bin/env python

import os
import sys
import pexpect
from pexpect import pxssh
import cmd
from salt.modules.status import pid
# from _ast import Num
# from arp_entry_state_change import ip_address
sys.path.append('/auto/nps_release/EZdk/EZdk-2.0a-patch-1.0.0/tools/EZcpPyLib/lib')
from ezpy_cp import EZpyCP

import time
import struct
import socket

import logging

STRUCT_ID_NW_ARP = 6

class real_server:
    def __init__(self, ip_server, virtual_ip_server):
        self.ip_server = ip_server
        self.virtual_ip_server = virtual_ip_server
        
    def create_http_server(self):
        cmd = "ifconfig lo:0 " + str(self.virtual_ip_address) + " netmask 255.255.255.255"
    
        
class arp_entry:
    
    def __init__(self, ip_address, mac_address, interface=None, flags=None, mask=None, type=None):
        self.ip_address = ip_address
        self.mac_address = mac_address
        self.interface = interface
        self.flags = flags
        self.mask = mask
        self.type = type
        
class ezbox_host:
    
    def __init__(self, hostname, username, password):
        self.ip_address = hostname
        self.username = username
        self.password = password
        self.ssh_object = pxssh.pxssh()
        self.arp_entries = []
        self.cp_arp_entries = []
        self.cp_arp_dict = {}
        self.linux_arp_dict = {}
        
    def connect(self):
        print "Connecting to host: " + self.ip_address + ", username: " + self.username + " password: " + self.password
        self.ssh_object.login(self.ip_address, self.username, self.password)
        self.cpe = EZpyCP(self.ip_address, 1234)
        print "Connected"
        
    def logout(self):
        self.ssh_object.logout()
        
    def run_cp_app(self, app_bin):
        self.ssh_object.sendline(app_bin)
        self.ssh_object.prompt()
        time.sleep(2)
        
    def get_arp_table(self):
        # clear all the entries in this setup
        del self.arp_entries[:]
        self.arp_entries = []
        del self.linux_arp_dict
        self.linux_arp_dict = {}
        
        self.ssh_object.sendline('arp -n')     # run a command
        self.ssh_object.prompt()               # match the prompt
        output = self.ssh_object.before        # print everything before the prompt.
        arp_entries = output.split('\n')
        # remove unnecessary first lines
        del arp_entries[0]
        del arp_entries[0]


        
        for entry in arp_entries:
            if entry == '':
                break
            if "incomplete" in entry: #not valid arp entry
                continue
            entry_items = entry.split()
            entry_items[2] = entry_items[2].upper()
            temp_entry = arp_entry(ip_address = entry_items[0], 
                                   type = entry_items[1], 
                                   mac_address = entry_items[2], 
                                   interface = entry_items[-1])
            
            self.arp_entries.append(temp_entry)
            self.linux_arp_dict[entry_items[0]] = temp_entry
        
        logging.log(logging.DEBUG,"num of entries in linux arp table: %d"%int(len(self.arp_entries)))
     
        logging.log(logging.DEBUG,"the linux arp table entries:")
        for entry in self.arp_entries:
            logging.log(logging.DEBUG,str(self.arp_entries.index(entry)) + ". ip_address: " + entry.ip_address + " mac address: " + entry.mac_address)
            
        return self.arp_entries
    
    def change_arp_entry_state(self, ip_address, new_state, check_if_state_changed = False, old_state = None):
        
        cmd ="ip neigh change " + ip_address + " dev eth2 nud " + new_state
        
        if check_if_state_changed == True:
            cmd = cmd + ";ip neigh show nud " + new_state + " | grep " + ip_address

        if old_state != None:
            if check_if_state_changed == True:
                cmd = "ip neigh change " + ip_address + " dev eth2 nud " + old_state + ";ip neigh show " + ip_address + " nud " + old_state + ";" + cmd
            else:
                cmd = "ip neigh change " + ip_address + " dev eth2 nud " + old_state + ";" + cmd
          
        logging.log(logging.INFO,"changing state of arp entry ip: " + ip_address + ", to state: "+ new_state)
        logging.log(logging.DEBUG,cmd)
        self.ssh_object.sendline(cmd)          # run a command
        self.ssh_object.prompt()               # match the prompt
        output = self.ssh_object.before        # print everything before the prompt.
        
        logging.log(logging.DEBUG,"output of chaging states:\n" + output)
        
        output = output.split('\n')
        
        if old_state != None and check_if_state_changed == True:
            if len(output) < 4:
                logging.log(logging.DEBUG,"Error didnt succeed to change entry state")
                return False
                
        output = output[1]
        self.ssh_object.sendline("echo $?")
        self.ssh_object.prompt()               # match the prompt
        exit_code = self.ssh_object.before     # print everything before the prompt.
        exit_code = exit_code.split('\n')
        exit_code = exit_code[1]
 
        if int(exit_code) != 0:
            logging.log(logging.DEBUG,"Change state of an arp entry failed")
            logging.log(logging.DEBUG,"Exit Code " + exit_code)
            logging.log(logging.DEBUG,"Error message: " + output)
            return False
             
        return True
        
    def verify_arp_state(self, ip_address, entry_state):
        logging.log(logging.INFO, "verify %s entry is on %s state"%(ip_address, entry_state))
         
        cmd ="ip neigh show nud " + entry_state + " | grep " + ip_address
        logging.log(logging.DEBUG, "command is: " + cmd)
        self.ssh_object.sendline(cmd)          # run a command
        self.ssh_object.prompt()               # match the prompt
        output = self.ssh_object.before        # print everything before the prompt.
        logging.log(logging.DEBUG, "output is:\n" + output)
        output = output.split('\n')
        output = output[1]

        self.ssh_object.sendline("echo $?")
        self.ssh_object.prompt()               # match the prompt
        exit_code = self.ssh_object.before     # print everything before the prompt.
        exit_code = exit_code.split('\n')
        exit_code = exit_code[1]
            
        if int(exit_code) > 0:
            return  False
        else:
            return True
    
    def terminate_cp_app(self,app_name='/tmp/alvs_daemon'):
        cmd = "ps ax | grep '%s'"%app_name
        logging.log(logging.DEBUG,"command: %s"%cmd)
        self.ssh_object.sendline(cmd)          # run a command
        self.ssh_object.prompt()               # match the prompt
        output = self.ssh_object.before        # print everything before the prompt.
        output = output.split('\n')
        output = output[1].split()
        pid = output[0]
        
        cmd = "kill " + pid
        logging.log(logging.DEBUG,"command: %s"%cmd)
        self.ssh_object.sendline(cmd)          # run a command
        self.ssh_object.prompt()               # match the prompt
        output = self.ssh_object.before        # print everything before the prompt.        
        
        self.ssh_object.sendline("echo $?")
        self.ssh_object.prompt()               # match the prompt
        exit_code = self.ssh_object.before        # print everything before the prompt.
        exit_code = exit_code.split('\n')
        exit_code = exit_code[1]

        if int(exit_code) != 0:
            print "terminate cp app was failed"
            print "Exit Code " + exit_code
            print "Error message: " + output
            return False
 
        logging.log(logging.INFO,"cp app was terminated succesfully")
        
        return True
        
    def get_cp_arp_table(self):
        
        logging.log(logging.DEBUG,"Reading the control plane arp table")
        # clear all the cp arp entries in this setup
        del self.cp_arp_entries[:]
        self.cp_arp_entries = []
        del self.cp_arp_dict
        self.cp_arp_dict = {}
        iterator_params_dict = { 'channel_id': 0 }
        struct_id = STRUCT_ID_NW_ARP
        
        iterator_params_dict = (self.cpe.cp.struct.iterator_create(struct_id, iterator_params_dict)).result['iterator_params']
        num_entries = (self.cpe.cp.struct.get_num_entries( struct_id = 6 , channel_id = 0 )).result['num_entries']['number_of_entries']
        logging.log(logging.DEBUG,"Num of entries are " + str(num_entries))
        for i in range(0,num_entries):
            iterator_params_dict = self.cpe.cp.struct.iterator_get_next( struct_id, iterator_params_dict ).result['iterator_params'] 
            key = iterator_params_dict['entry']['key']
            ip_address = key.replace(" ", "")
            ip_address = int(ip_address,16)
            ip_address = int2ip(ip_address)
            
            result = iterator_params_dict['entry']['result']
            result = result.replace(" ","")
            result = result[4:]
            mac_address = int(result,16)
            mac_address = int2mac(mac_address)
            mac_address = str(mac_address).upper()
            temp_entry = arp_entry(ip_address = ip_address, 
                                   type = None, 
                                   mac_address = mac_address, 
                                   interface = None)
            self.cp_arp_entries.append(temp_entry)
            self.cp_arp_dict[ip_address] = temp_entry
            
        self.cpe.cp.struct.iterator_delete( struct_id , iterator_params_dict )

        logging.log(logging.DEBUG,"the control plane arp table entries:")
        for entry in self.cp_arp_entries:
            logging.log(logging.DEBUG,str(self.cp_arp_entries.index(entry)) + ". ip_address: " + entry.ip_address + " mac address: " + entry.mac_address)        
        return self.cp_arp_entries
        
    def check_cp_entry_exist(self, ip_address, mac_address, update_arp_entries=True):
        mac_address = str(mac_address).upper()
        logging.log(logging.INFO,"Check if entry exist on control plane - ip address: " + ip_address + ", mac address: " + mac_address)
        if update_arp_entries == True or self.cp_arp_entries == []:
            self.get_cp_arp_table()
            self.get_arp_table()
            
        logging.log(logging.DEBUG,"looking for ip address %s on cp arp table"%ip_address)
        if ip_address in self.cp_arp_dict:
            logging.log(logging.DEBUG,"ip address exist on control plane arp entries")
            logging.log(logging.DEBUG,"looking for ip address %s on linux arp table"%ip_address)
            if ip_address in self.linux_arp_dict:
                logging.log(logging.DEBUG,"ip address exist on linux arp entries")
                logging.log(logging.DEBUG,"comparing cp mac address %s to linux mac address %s"%(self.cp_arp_dict[ip_address].mac_address,self.linux_arp_dict[ip_address].mac_address))
                if self.cp_arp_dict[ip_address].mac_address == self.linux_arp_dict[ip_address].mac_address:
                    logging.log(logging.DEBUG,"the entry exist on linux table and control plane table")
                    return True
        
        logging.log(logging.INFO,"Didn't found match")
        return False
    
#         for cp_entry in self.cp_arp_entries:
#             logging.log(logging.DEBUG,"Comparing ip %s to %s and mac %s to %s",cp_entry.ip_address, ip_address, cp_entry.mac_address, mac_address)
#             if cp_entry.ip_address == ip_address and cp_entry.mac_address == mac_address:
#                 logging.log(logging.INFO,"Found match")
#                 return True
#             
#         return False
        
    def get_unused_ip_on_subnet(self,netmask):
        self.get_arp_table()
        int_ip_address=ip2int(self.ip_address)
        int_netmask=ip2int(netmask)
        first_ip = int2ip(int_ip_address & int_netmask)
        temp_ip = first_ip
        
        new_list = []
        
        while True: 
            temp_ip = add2ip(temp_ip,1)
            if self.ip_address == temp_ip:
                continue
            
            found=False
            for entry in self.arp_entries:
                if entry.ip_address == temp_ip:
                    found=True
                    continue
            if (ip2int(first_ip) & int_netmask) < (ip2int(temp_ip) & int_netmask):
                break
            
            if found == False:
                new_list.append(temp_ip)
                
        return new_list    
           
    def compare_arp_tables(self, update_arp_entries=True):
        
        logging.log(logging.INFO,"Comparing the linux arp table and the control plane arp table")
        
        # update 
        if update_arp_entries == True or self.arp_entries == []:
            logging.log(logging.INFO,"reading the linux arp table")
            self.get_arp_table()
        if update_arp_entries == True or self.cp_arp_entries == []:
            logging.log(logging.INFO,"reading the control plane arp table")
            self.get_cp_arp_table()
            
        linux_arp_entries = self.arp_entries[:]
        cp_arp_entries = self.cp_arp_entries[:]
        
        logging.log(logging.DEBUG,"linux arp table has " + str(len(linux_arp_entries)) + " entries")
        logging.log(logging.DEBUG,"control plane arp table has " + str(len(cp_arp_entries)) + " entries")
        if len(linux_arp_entries) != len(cp_arp_entries):
            print "Error - num of entries is not equal"
            print "linux has " + str(len(linux_arp_entries)) + " arp entries"
            print "control plane has " + str(len(cp_arp_entries)) + " arp entries"
            return False
            
        for cp_entry in self.cp_arp_entries:
            for linux_entry in self.arp_entries:
                if cp_entry.ip_address == linux_entry.ip_address:
                    if cp_entry.mac_address == linux_entry.mac_address:
                        logging.log(logging.DEBUG,"found a match:")
                        logging.log(logging.DEBUG,"linux_entry.ip_address = " + linux_entry.ip_address)
                        logging.log(logging.DEBUG,"cp_entry.mac_address = " + cp_entry.mac_address)
                        logging.log(logging.DEBUG,"linux_entry.mac_address = " + linux_entry.mac_address)
                        linux_arp_entries.remove(linux_entry)
                        cp_arp_entries.remove(cp_entry)
                        continue
                
        if cp_arp_entries == [] and linux_arp_entries == []:
            return True
        else:
            logging.log(logging.INFO,"didnt found a match to all entries\nthese are the entries that didnt had a match")
            for cp_entry in cp_arp_entries:
                logging.log(logging.DEBUG,"\ncp_entry.ip_address = " + cp_entry.ip_address) 
        
            for entry in linux_arp_entries:
                logging.log(logging.DEBUG,"\nlinux_entry.ip_address = " + entry.ip_address)
                 
            return False
            
        
    def add_arp_entry(self, ip_address, mac_address):
        mac_address = str(mac_address).upper()

        cmd = "arp -s " + ip_address + " " + mac_address
        logging.log(logging.INFO,"Adding an arp entry ip: " + ip_address + ", mac address: "+ mac_address)
        logging.log(logging.DEBUG,cmd)
        self.ssh_object.sendline(cmd)          # run a command
        self.ssh_object.prompt()               # match the prompt
        output = self.ssh_object.before        # print everything before the prompt.
        output = output.split('\n')
        output = output[1]
        self.ssh_object.sendline("echo $?")
        self.ssh_object.prompt()               # match the prompt
        exit_code = self.ssh_object.before        # print everything before the prompt.
        exit_code = exit_code.split('\n')
        exit_code = exit_code[1]

        if int(exit_code) != 0:
            print "Add an arp entry on linux failed"
            print "Exit Code " + exit_code
            print "Error message: " + output
            return False
 
        logging.log(logging.INFO,"Entry was added")
        
        return True
    
    def delete_arp_entry(self, ip_address):
        cmd = "arp -d " + ip_address
        self.ssh_object.sendline(cmd)          # run a command
        self.ssh_object.prompt()               # match the prompt
        output = self.ssh_object.before        # print everything before the prompt.
        
        # check exit code of the last command
        self.ssh_object.sendline("echo $?")
        self.ssh_object.prompt()               # match the prompt
        exit_code = self.ssh_object.before        # print everything before the prompt.
        exit_code = exit_code.split('\n')
        exit_code = exit_code[1]

        if int(exit_code) != 0:
            print "Delete an arp entry on linux failed"
            print "Exit Code " + exit_code
            print "Error message: " + output
            return False
 
        logging.log(logging.INFO,"Entry was deleted")
        
        return True
    
    def clean_arp_table(self):
        cmd = 'ip -s -s neigh flush all'
        self.ssh_object.sendline(cmd)          # run a command
        self.ssh_object.prompt()               # match the prompt
        output = self.ssh_object.before        # print everything before the prompt.
        print output
        
        cmd = 'ip -s -s neigh flush nud perm'
        self.ssh_object.sendline(cmd)          # run a command
        self.ssh_object.prompt()               # match the prompt
        output = self.ssh_object.before        # print everything before the prompt.
        print output
                
    def add_service(self, virtual_ip_address, real_ip_addresses):
        print virtual_ip_address
        print real_ip_addresses
        
    def remove_service(self, virtual_ip_address, real_ip_addresses):
        print virtual_ip_address
        print real_ip_addresses
              
   
   
def ip2int(addr):                                                               
    return struct.unpack("!I", socket.inet_aton(addr))[0]                       


def int2ip(addr):                                                               
    return socket.inet_ntoa(struct.pack("!I", addr)) 

def mac2int(addr):
    temp = addr.replace(':','')
    return int(temp,16)

def int2mac(addr):
    mac = hex(addr)
    mac = mac[2:]
    mac = '0'*(12-len(mac)) + mac
    temp = mac[0:2] + ":" + mac[2:4] + ":" + mac[4:6] + ":" + mac[6:8] + ":" + mac[8:10] + ":" + mac[10:12]
    return temp

def add2ip(addr,num):
    temp = ip2int(addr)
    temp = temp + num
    return int2ip(temp)

def add2mac(addr,num):
    temp = mac2int(addr)
    temp = temp + num
    return int2mac(temp)

    
    
# ezbox = setup(hostname='10.7.103.30', username='root', password='ezchip')
# ezbox.connect()
# r = ezbox.get_cp_arp_table()
# r1 = ezbox.get_arp_table()


        

