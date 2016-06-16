#!/usr/bin/env python

import os
import sys
import pexpect
from pexpect import pxssh
import random

sys.path.append('/auto/nps_release/EZdk/EZdk-2.0a-patch-1.0.0/tools/EZcpPyLib/lib')
from ezpy_cp import EZpyCP
 
import time
import struct
import socket
import inspect
import logging

STRUCT_ID_NW_INTERFACES                = 0
STRUCT_ID_NW_LAG                       = 1
STRUCT_ID_ALVS_CONN_CLASSIFICATION     = 2
STRUCT_ID_ALVS_CONN_INFO               = 3
STRUCT_ID_ALVS_SERVICE_CLASSIFICATION  = 4
STRUCT_ID_ALVS_SERVICE_INFO            = 5
STRUCT_ID_ALVS_SCHED_INFO              = 6
STRUCT_ID_ALVS_SERVER_INFO             = 7
STRUCT_ID_NW_FIB                       = 8
STRUCT_ID_NW_ARP                       = 9

class real_server:
    def __init__(self, management_ip, data_ip, username, password):
        self.username = username
        self.password = password
        self.management_ip = management_ip
        self.data_ip = data_ip
        self.ssh_object = pxssh.pxssh()
        
    def connect(self):
        
        self.ssh_object.login(self.management_ip, self.username, self.password)
        
        cmd = "ifconfig lo:0 " + self.data_ip + " netmask 255.255.255.255"
        
        self.ssh_object.sendline(cmd)
        self.ssh_object.prompt()
        
    def close_server(self):
        cmd = "ifconfig lo:0 down"
        
    def catch_received_packets(self, tcpdump_params):
        cmd = 'tcpdump -G 10 -w /tmp/dump.pcap -n -i eth0:0 ' + tcpdump_params + ' dst ' + self.data_ip + ' & sleep 3s && pkill -HUP -f tcpdump'
        print cmd
        self.ssh_object.sendline(cmd)
        self.ssh_object.prompt()
       
class service:
    
    def __init__(self, ezbox, virtual_ip, port, schedule_algorithm = 'source_hash'):
        self.virtual_ip = virtual_ip
        self.port = port
        self.ezbox = ezbox
        self.servers = []
        if schedule_algorithm == 'source_hash':
            self.schedule_algorithm = 'sh'
        else:
            print 'This Schedule algorithm is not supported'
            exit(1)
        
        self.ezbox.execute_command_on_host("ipvsadm -A -t %s:%s -s %s"%(self.schedule_algorithm, self.virtual_ip, self.port))
        
    def add_server(self, new_server, weight = '1'):
        self.servers.append(new_server)
        result, output = self.ezbox.execute_command_on_host("ipvsadm -a -t %s:%s -r %s:%s -w %s"%(self.virtual_ip, self.port, new_server.data_ip, self.port, weight))        
        if result == False:
            print "Error while removing server from service"
            exit(1)
            
    def remove_server(self,server_to_delete):
        self.servers.remove(server_to_delete)
        result, output = self.ezbox.execute_command_on_host("ipvsadm -d -t %s:%s -r %s:%s"%(self.virtual_ip, self.port, server_to_delete.data_ip, self.port))
        if result == False:
            print "Error while removing server from service"
            exit(1)
        
    def modify_server(self, server_to_modify):
        
        if server_to_modify not in self.servers:
            print "Error, Server is not exist on Service"
            exit(1)
             
        result, output = self.ezbox.execute_command_on_host("ipvsadm -e -t %s:%s -r %s:%s"%(self.virtual_ip, self.port, server_to_delete.data_ip, self.port))
        if result == False:
            print "Error while removing Server from Service"
            exit(1)
        
class arp_entry:
     
    def __init__(self, ip_address, mac_address, interface=None, flags=None, mask=None, type=None):
        self.ip_address = ip_address
        self.mac_address = mac_address
        self.interface = interface
        self.flags = flags
        self.mask = mask
        self.type = type
         
class ezbox_host:
     
    def __init__(self, management_ip, username, password, data_ip=None, nps_ip=None, cp_app_bin="alvs_daemon", dp_app_bin="alvs_dp"):
        self.management_ip = management_ip
        self.data_ip = data_ip
        self.username = username
        self.password = password
        self.ssh_object = pxssh.pxssh()
        self.run_app_ssh = pxssh.pxssh()
        self.syslog_ssh = pxssh.pxssh()
        self.arp_entries = []
        self.cp_arp_entries = []
        self.cp_arp_dict = {}
        self.linux_arp_dict = {}
        self.process_id = None
        self.cp_app_bin = cp_app_bin
        self.dp_app_bin = dp_app_bin
        self.nps_ip = nps_ip
        
     
    def reset_ezbox(self, ezbox_name):
        print "Reset EZbox: " + ezbox_name
        cmd = "/.autodirect/LIT/SCRIPTS/rreboot " + ezbox_name
        os.system(cmd)
        print "Finished reset"
        self.wait_for_connectivity()
#         time.sleep(40)
        
    def wait_for_connectivity(self):
        os.system("../wait_for_connection.sh " + self.management_ip)
        
    def clean(self):
        self.terminate_cp_app()
        self.logout()
        
    def connect(self):
        print "Connecting to host: " + self.management_ip + ", username: " + self.username + " password: " + self.password
        
        #connecting to host 
        self.ssh_object.login(self.management_ip, self.username, self.password)
        self.run_app_ssh.login(self.management_ip, self.username, self.password)
        
        # connect to syslog
        self.syslog_ssh.login(self.management_ip, self.username, self.password)
        self.syslog_ssh.sendline('tail -f /var/log/syslog | grep ALVS_DAEMON')
        
        
        # connect to AGT
        self.cpe = EZpyCP(self.management_ip, 1234)
        
        # retrieve local mac address
        result, self.mac_address, pid = self.execute_command_on_host("cat /sys/class/net/eth0/address")
        if result == False:
            print "Error while retreive local address"
            print self.mac_address
            exit(1)
        
        self.mac_address = self.mac_address.strip('\r')
        
        if self.data_ip != None:
            self.execute_command_on_host("ifconfig eth0:0 %s netmask 255.255.255.0"%self.data_ip)
        
        
        print "Connected"
         
    def logout(self):
        self.ssh_object.logout()
        
    def send_packet_to_ezbox(self, pcap_file):
        
        cmd = 'sudo tcpreplay --intf1=ens6 ' + pcap_file
        print cmd
        os.system(cmd)
        
    def execute_command_on_host(self, cmd):
        
        temp_ssh_object = pxssh.pxssh()
        temp_ssh_object.login(self.management_ip, self.username, self.password)
        time.sleep(1)
        self.ssh_object.sendline(cmd)
        time.sleep(1)
        self.ssh_object.prompt()
        time.sleep(1)
        output = self.ssh_object.before
#         print "output is:"
#         print "output "+ output
        output = output.split('\n')
        output = output[1]
        output = output.strip('\r')
        
        # get exit code
        self.ssh_object.sendline("echo $?")
        self.ssh_object.prompt()               
        exit_code = self.ssh_object.before 
        exit_code = exit_code.strip('\r')
        exit_code = exit_code.split('\n')
        exit_code = exit_code[1]
 
        # get pid of process
        self.ssh_object.sendline("echo $!")
        self.ssh_object.prompt()         
        pid = self.ssh_object.before
        pid = pid.strip('\r')
        pid = pid.split('\n')
        pid = pid[1]
        pid = pid.strip('\r')
        
        temp_ssh_object.logout()
        del temp_ssh_object
        
        try:
            exit_code = int(exit_code)
            if exit_code != 0:
                print "terminate cp app was failed"
                print "Exit Code " + exit_code
                print "Error message: " + output
                return [False,output,pid]
        except:
            exit_code = None
            pass

        return [True,output,pid]
       
    def copy_cp_bin_to_host(self,cp_bin=None):
        print "Copy cp bin file to host"
        if cp_bin == None:
            cp_bin = self.cp_app_bin
        cmd = "sshpass -p ezchip ssh root@%s rm -f /tmp/%s"%(self.management_ip,cp_bin)
        os.system(cmd)
        bin_path = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))+'/../../bin'
        cmd = "sshpass -p %s scp %s/%s %s@%s:/tmp/%s"%(self.password, bin_path, cp_bin, self.username, self.management_ip, cp_bin)
        os.system(cmd)
        
    def reset_chip(self):
        print "Reset The Chip"
        logging.log(logging.INFO,"Reset The Chip")
        
        self.run_app_ssh.sendline("nps_power -r por")
        self.run_app_ssh.prompt()
        time.sleep(3)
        self.run_app_ssh.sendline("~/nl_if_setup.sh")
        self.run_app_ssh.prompt()
        time.sleep(1)
        
    def copy_and_run_dp_bin(self, nps_ip=None, dp_bin=None):
        if nps_ip == None:
            nps_ip = self.nps_ip
        if dp_bin == None:
            dp_bin = self.dp_app_bin
            
        print "Copy and run dp app on chip"
        logging.log(logging.INFO,"Copy and run dp app on chip")
        bin_path = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))+'/../../bin'
        script_path = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
        os.system("sudo %s/run_dp.sh %s/%s %s"%(script_path, bin_path, dp_bin, nps_ip) )
        
    
    def check_syslog_output(self, message):
        while (True):
            print "Looking for syslog message " + message
            
            index = self.syslog_ssh.expect_exact([message, pexpect.EOF, pexpect.TIMEOUT])
            if index == 0:
                print 'Found...'
                logging.log(logging.INFO, self.syslog_ssh.before + self.syslog_ssh.match + self.syslog_ssh.after)
                break
            elif index == 1:
                print 'EOF...'
                print self.syslog_ssh.before + self.syslog_ssh.match
                return False
                break
            elif index == 2: 
                print 'TIMEOUT...'
                print self.syslog_ssh.before
        return True
        
         
    def run_cp_app(self, app_bin=None, agt_enable = True):
        if app_bin==None:
            app_bin=self.cp_app_bin
        if self.verify_cp_app_is_running(app_bin) == False:
            print "run cp app"
            
            if agt_enable == True:
                self.run_app_ssh.sendline("/tmp/" + app_bin + " --agt_enabled")
                
            else:
                self.run_app_ssh.sendline("/tmp/" + app_bin)
                
            self.run_app_ssh.prompt()
            self.run_app_ssh.prompt()
            time.sleep(5)
            output = self.run_app_ssh.before
            logging.log(logging.DEBUG, output)
        
#             os.system("sshpass -p ezchip scp /swgwork/tomeri/ALVS/bin/" + app_bin + " root@" + host_ip + ":/tmp/ ")
#             cp_app_ssh.sendline("/tmp/" + app_name + " --agt_enabled")
#             cp_app_ssh.prompt()
             
            print 'Waiting for NPS initialization to complete...'
            while (True):
#                 index = self.syslog_ssh.expect_exact(['alvs_db_manager_poll...','Shut down ALVS daemon',pexpect.EOF, pexpect.TIMEOUT])
                index = self.syslog_ssh.expect_exact(['alvs_db_manager_poll...',pexpect.EOF, pexpect.TIMEOUT])
                if index == 0:
                    print 'Completed...'
                    logging.log(logging.INFO, self.syslog_ssh.before + self.syslog_ssh.match + self.syslog_ssh.after)
                    break
#                 elif index == 1:
#                     print 'Shut down...'
#                     print self.syslog_ssh.before + self.syslog_ssh.match
#                     self.run_app_ssh.sendline("cat /tmp/alvs_daemon.log")
#                     self.run_app_ssh.prompt()
#                     print self.run_app_ssh.before
#                     exit(1)
#                     break
                elif index == 1:
                    print 'EOF...'
                    print self.syslog_ssh.before + self.syslog_ssh.match
                    exit(1)
                    break
                elif index == 2:
                    print 'TIMEOUT...'
                    print self.syslog_ssh.before
        return self.verify_cp_app_is_running(app_bin)
         
    def verify_cp_app_is_running(self,app_name=None):
        if app_name==None:
            app_name = self.cp_app_bin
        logging.log(logging.INFO,"Check if cp app is running")
        if self.process_id != None:
            app_name = self.process_id
        cmd = "pidof " + app_name

        logging.log(logging.DEBUG,"command: %s"%cmd)
        self.ssh_object.sendline(cmd)         
        self.ssh_object.prompt()
        output = self.ssh_object.before
        output = output.split('\n')
        if output[1] == '':
            print "CP app is not running"
            logging.log(logging.INFO,"CP app is not running")
            return False
        else:
            print "CP app is running"
            logging.log(logging.INFO,"CP app is running")
            return True
            
    def terminate_cp_app(self,app_name=None):
        if app_name==None:
            app_name = self.cp_app_bin
        
        if self.verify_cp_app_is_running(app_name) == False:
            return True

        print "Terminate current cp app" 
            
        cmd = "pidof " + app_name
#         cmd = "ps ax | grep '%s'"%app_name
        logging.log(logging.DEBUG,"command: %s"%cmd)
        self.run_app_ssh.sendline(cmd)          # run a command
        self.run_app_ssh.prompt()               # match the prompt
        output = self.run_app_ssh.before        # print everything before the prompt.
        logging.log(logging.DEBUG, output)
        output = output.split('\n')
#         output = output[1].split()
        pid = output[1]
         
        cmd = "kill " + pid
        logging.log(logging.DEBUG,"command: %s"%cmd)
        self.run_app_ssh.sendline(cmd)          # run a command
        self.run_app_ssh.prompt()               # match the prompt
        output = self.run_app_ssh.before        # print everything before the prompt.        
        logging.log(logging.DEBUG, output)
#         self.ssh_object.sendline("echo $?")
#         self.ssh_object.prompt()               # match the prompt
#         exit_code = self.ssh_object.before        # print everything before the prompt.
#         print "exit code: " + exit_code
#         exit_code = exit_code.split('\n')
#         exit_code = exit_code[1]
#         print exit_code
#         if int(exit_code) != 0:
#             print "terminate cp app was failed"
#             print "Exit Code " + exit_code
#             print "Error message: " + output
#             return False
  
        time.sleep(5)
        if self.verify_cp_app_is_running(app_name) == False:
            logging.log(logging.INFO,"cp app was terminated succesfully")
            return True
        else:
            print "error while terminating cp app"
            logging.log(logging.ERROR,"error while terminating cp app")
            exit(1)
            return False
         
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
         
        cmd ="ip neigh change " + ip_address + " dev eth0 nud " + new_state
         
        if check_if_state_changed == True:
            cmd = cmd + ";ip neigh show nud " + new_state + " | grep " + ip_address
 
        if old_state != None:
            if check_if_state_changed == True:
                cmd = "ip neigh change " + ip_address + " dev eth0 nud " + old_state + ";ip neigh show " + ip_address + " nud " + old_state + ";" + cmd
            else:
                cmd = "ip neigh change " + ip_address + " dev eth0 nud " + old_state + ";" + cmd
           
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
        num_entries = (self.cpe.cp.struct.get_num_entries( struct_id , channel_id = 0 )).result['num_entries']['number_of_entries']
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
         
    def get_unused_ip_on_subnet(self,netmask='255.255.248.0'):
        self.get_arp_table()
        int_ip_address=ip2int(self.management_ip)
        int_netmask=ip2int(netmask)
        first_ip = int2ip(int_ip_address & int_netmask)
        temp_ip = first_ip
         
        new_list = []
         
        while True: 
            temp_ip = add2ip(temp_ip,1)
            if self.management_ip == temp_ip:
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
        logging.log(logging.DEBUG,output)
        output = output.split('\n')
        output = output[1]
        self.ssh_object.sendline("echo $?")
        self.ssh_object.prompt()               # match the prompt
        exit_code = self.ssh_object.before        # print everything before the prompt.
        logging.log(logging.DEBUG,exit_code)
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
        logging.log(logging.INFO, "clean arp table")
        cmd = 'ip -s -s neigh flush all'
        self.ssh_object.sendline(cmd)          # run a command
        self.ssh_object.prompt()               # match the prompt
        output = self.ssh_object.before        # print everything before the prompt.
        logging.log(logging.DEBUG, output)
         
        cmd = 'ip -s -s neigh flush nud perm'
        self.ssh_object.sendline(cmd)          # run a command
        self.ssh_object.prompt()               # match the prompt
        output = self.ssh_object.before        # print everything before the prompt.
        logging.log(logging.DEBUG, output)
        
    def collect_log(self):
        print "Saving Logs"
        #syslog
        #alvs_daemon log
        #/var/log/alvs_ezcp_log
        
#     def add_service(self, service):
#         self.execute_command_on_host("ipvsadm -A -t %s:%s -s sh"%(virtual_ip_address,port))
#         
#         for server_entry in servers:
#             self.execute_command_on_host("ipvsadm -a -t %s:%s -r %s:%s -w 1"%(virtual_ip_address,port,server_entry,port))
#          
#     def remove_service(self, virtual_ip_address, real_ip_addresses):
#         # NOT IMPLEMENTED
#         print virtual_ip_address
#         print real_ip_addresses
        
    def catch_received_packets_on_host(self, tcpdump_params = ''):
        cmd = 'tcpdump -G 10 -w /tmp/dump.pcap -n -i eth0:0 ' + tcpdump_params + ' dst ' + self.data_ip + ' & sleep 3s && pkill -HUP -f tcpdump'
        print cmd
        self.ssh_object.sendline(cmd)
        self.ssh_object.prompt()

    def get_num_of_services(self):
        return self.cpe.cp.struct.get_num_entries(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION, channel_id = 0).result['num_entries']['number_of_entries']

    def get_num_of_connections(self):
        return self.cpe.cp.struct.get_num_entries(STRUCT_ID_ALVS_CONN_CLASSIFICATION, channel_id = 0).result['num_entries']['number_of_entries']
    
    def get_service(self, vip, port, protocol):
        class_res = str(self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION, 0, {'key' : "%08x%04x%04x" % (vip, port, protocol)}).result["params"]["entry"]["result"]).split(' ')
        if (int(class_res[0], 16) >> 4) != 3:
            return None
        info_res = str(self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_SERVICE_INFO, 0, {'key' : class_res[2]}).result["params"]["entry"]["result"]).split(' ')
        if (int(info_res[0], 16) >> 4) != 3:
            print 'This should not happen!'
            return None
        
        sched_info = []
        for ind in range(256):
            sched_index = int(class_res[2], 16) * 256 + ind
            sched_res = str(self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_SCHED_INFO, 0, {'key' : "%04x" % sched_index}).result["params"]["entry"]["result"]).split(' ')
            if (int(sched_res[0], 16) >> 4) == 3:
                sched_info[ind] = int(''.join(sched_res[4:8]), 16)
        
        service = {'sched_alg' : int(info_res[0], 16) & 0xf,
                   'server_count' : int(''.join(res[2:4]), 16),
                   'stats_base' : int(''.join(res[8:12]), 16),
                   'flags' : int(''.join(res[12:16]), 16),
                   'sched_info' : sched_info
                   }
        
        return service
    
    def get_connection(self, vip, vport, cip, cport, protocol):
        class_res = str(self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_CONN_CLASSIFICATION, 0, {'key' : "%08x%08x%04x%04x%04x" % (cip, vip, cport, vport, protocol)}).result["params"]["entry"]["result"]).split(' ')
        if (int(class_res[0], 16) >> 4) != 3:
            return None
        info_res = str(self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_CONN_INFO, 0, {'key' : ''.join(class_res[4:8])}).result["params"]["entry"]["result"]).split(' ')
        if (int(info_res[0], 16) >> 4) != 3:
            print 'This should not happen!'
            return None
        
        conn = {'sync_bit' : (int(info_res[0], 16) >> 3) & 0x1,
                'aging_bit' : (int(info_res[0], 16) >> 2) & 0x1,
                'delete_bit' : (int(info_res[0], 16) >> 1) & 0x1,
                'reset_bit' : int(info_res[0], 16) & 0x1,
                'age_iteration' : int(info_res[1], 16),
                'server_index' : int(''.join(info_res[2:4]), 16),
                'state' : int(info_res[5], 16),
                'stats_base' : int(''.join(info_res[8:12]), 16),
                'flags' : int(''.join(info_res[28:32]), 16)
                }
        
        return conn

    def get_interface(self, lid):
        res = str(self.cpe.cp.struct.lookup(STRUCT_ID_NW_INTERFACES, 0, {'key' : "%02x" % lid}).result["params"]["entry"]["result"]).split(' ')
        interface = {'oper_status' : (int(info_res[0], 16) >> 3) & 0x1,
                     'path_type' : (int(info_res[0], 16) >> 1) & 0x3,
                     'is_vlan' : int(info_res[0], 16) & 0x1,
                     'lag_id' : int(info_res[1], 16),
                     'default_vlan' : int(''.join(info_res[2:4]), 16),
                     'mac_address' : int(''.join(info_res[4:10]), 16),
                     'output_channel' : int(info_res[10], 16),
                     'stats_base' : int(''.join(info_res[12:16]), 16),
                     }
        
        return interface

    def get_all_interfaces(self):
        iterator_params_dict = (self.cpe.cp.struct.iterator_create(STRUCT_ID_NW_INTERFACES, { 'channel_id': 0 })).result['iterator_params']
        num_entries = (self.cpe.cp.struct.get_num_entries(STRUCT_ID_NW_INTERFACES, channel_id = 0)).result['num_entries']['number_of_entries']

        interfaces = []
        for i in range(0, num_entries):
            iterator_params_dict = self.cpe.cp.struct.iterator_get_next(STRUCT_ID_NW_INTERFACES, iterator_params_dict).result['iterator_params']
             
            key = str(iterator_params_dict['entry']['key'])
            lid = int(key, 16)
             
            result = str(iterator_params_dict['entry']['result']).split(' ')
            interface = {'oper_status' : (int(result[0], 16) >> 3) & 0x1,
                         'path_type' : (int(result[0], 16) >> 1) & 0x3,
                         'is_vlan' : int(result[0], 16) & 0x1,
                         'lag_id' : int(result[1], 16),
                         'default_vlan' : int(''.join(result[2:4]), 16),
                         'mac_address' : int(''.join(result[4:10]), 16),
                         'output_channel' : int(result[10], 16),
                         'stats_base' : int(''.join(result[12:16]), 16),
                         'key' : {'lid' : lid}
                         }

            interfaces.append(interface)
            
        self.cpe.cp.struct.iterator_delete(STRUCT_ID_NW_INTERFACES, iterator_params_dict)
        return interfaces
    
    def get_all_services(self):
        iterator_params_dict = (self.cpe.cp.struct.iterator_create(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION, { 'channel_id': 0 })).result['iterator_params']
        num_entries = (self.cpe.cp.struct.get_num_entries(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION, channel_id = 0)).result['num_entries']['number_of_entries']

        services = []
        for i in range(0, num_entries):
            iterator_params_dict = self.cpe.cp.struct.iterator_get_next(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION, iterator_params_dict).result['iterator_params']
             
            key = str(iterator_params_dict['entry']['key']).split(' ')
            vip = int(''.join(key[0:4]), 16)
            port = int(''.join(key[4:6]), 16)
            protocol = int(''.join(key[6:8]), 16)
             
            class_res = str(iterator_params_dict['entry']['result']).split(' ')
            info_res = str(self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_SERVICE_INFO, 0, {'key' : class_res[2]}).result["params"]["entry"]["result"]).split(' ')
            if (int(info_res[0], 16) >> 4) != 3:
                print 'This should not happen!'
                return {}            
            
            sched_info = []
            for ind in range(256):
                sched_index = int(class_res[2], 16) * 256 + ind
                sched_res = str(self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_SCHED_INFO, 0, {'key' : "%04x" % sched_index}).result["params"]["entry"]["result"]).split(' ')
                if (int(sched_res[0], 16) >> 4) == 3:
                    sched_info[ind] = int(''.join(sched_res[4:8]), 16)
            
            service = {'sched_alg' : int(info_res[0], 16) & 0xf,
                       'server_count' : int(''.join(res[2:4]), 16),
                       'stats_base' : int(''.join(res[8:12]), 16),
                       'flags' : int(''.join(res[12:16]), 16),
                       'sched_info' : sched_info,
                       'key' : {'vip' : vip, 'port' : port, 'protocol' : protocol}
                       }
            
            services.append(service)
            
        self.cpe.cp.struct.iterator_delete(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION, iterator_params_dict)
        return services


    def get_all_connections(self):
        iterator_params_dict = (self.cpe.cp.struct.iterator_create(STRUCT_ID_ALVS_CONN_CLASSIFICATION, { 'channel_id': 0 })).result['iterator_params']
        num_entries = (self.cpe.cp.struct.get_num_entries(STRUCT_ID_ALVS_CONN_CLASSIFICATION, channel_id = 0)).result['num_entries']['number_of_entries']

        conns = []
        for i in range(0, num_entries):
            iterator_params_dict = self.cpe.cp.struct.iterator_get_next(STRUCT_ID_ALVS_CONN_CLASSIFICATION, iterator_params_dict).result['iterator_params']
             
            key = str(iterator_params_dict['entry']['key']).split(' ')
            cip = int(''.join(key[0:4]), 16)
            vip = int(''.join(key[4:8]), 16)
            cport = int(''.join(key[8:10]), 16)
            vport = int(''.join(key[10:12]), 16)
            protocol = int(''.join(key[12:14]), 16)
             
            class_res = str(iterator_params_dict['entry']['result']).split(' ')
            info_res = str(self.cpe.cp.struct.lookup(STRUCT_ID_ALVS_CONN_INFO, 0, {'key' : class_res[2]}).result["params"]["entry"]["result"]).split(' ')
            if (int(info_res[0], 16) >> 4) != 3:
                print 'This should not happen!'
                return {}            
            
            conn = {'sync_bit' : (int(info_res[0], 16) >> 3) & 0x1,
                    'aging_bit' : (int(info_res[0], 16) >> 2) & 0x1,
                    'delete_bit' : (int(info_res[0], 16) >> 1) & 0x1,
                    'reset_bit' : int(info_res[0], 16) & 0x1,
                    'age_iteration' : int(info_res[1], 16),
                    'server_index' : int(''.join(info_res[2:4]), 16),
                    'state' : int(info_res[5], 16),
                    'stats_base' : int(''.join(info_res[8:12]), 16),
                    'flags' : int(''.join(info_res[28:32]), 16),
                    'key' : {'vip' : vip, 'vport' : vport, 'cip' : cip, 'cport' : cport, 'protocol' : protocol}
                    }
            
            conns.append(conn)
            
        self.cpe.cp.struct.iterator_delete(STRUCT_ID_ALVS_CONN_CLASSIFICATION, iterator_params_dict)
        return conns


class tcp_packet:
    def __init__(self, mac_da, mac_sa, ip_src, ip_dst, tcp_source_port, tcp_reset_flag = False, tcp_fin_flag = False, tcp_sync_flag = False, packet_length = 64):
        self.mac_sa = mac_sa
        self.mac_da = mac_da
        self.ip_src = ip_src
        self.ip_dst = ip_dst
        self.tcp_source_port = tcp_source_port
        self.tcp_reset_flag = tcp_reset_flag
        self.tcp_fin_flag = tcp_fin_flag
        self.tcp_sync_flag = tcp_sync_flag
        self.packet_length = packet_length
        self.packet = ''
    
    def generate_packet(self):
        l2_header = self.mac_da + ' ' + self.mac_sa + ' ' + '08 00'
        data = '45 00 00 2e 00 00 40 00 40 06 00 00 ' + self.ip_src + ' ' + self.ip_dst  
        data = data.split()
        data = map(lambda x: int(x,16), data)
        ip_checksum = checksum(data)
        ip_checksum = '%02x'%ip_checksum
        ip_header = '45 00 00 2e 00 00 40 00 40 06 ' + ip_checksum[0:2] + ' ' + ip_checksum[2:4] + ' ' + self.ip_src + ' ' + self.ip_dst
        
        flag = 0
        if self.tcp_fin_flag:
            flag += 1
        if self.tcp_sync_flag:
            flag += 2    
        if self.tcp_reset_flag:
            flag += 4
        flag = '%02x'%flag
        data = self.tcp_source_port + ' 00 00 00 00 00 00 00 00 00 00 50 ' + flag + ' FF FC 00 00 00 00'
        data = data.split()
        data = map(lambda x: int(x,16), data)
        tcp_checksum = checksum(data)
        tcp_checksum = '%02x'%tcp_checksum
        tcp_header = self.tcp_source_port + ' 00 00 00 00 00 00 00 00 00 00 50 ' + flag + ' FF FC ' + tcp_checksum[0:2] + ' ' + tcp_checksum[2:4] + ' 00 00'
        
        packet = l2_header + ' ' + ip_header + ' ' + tcp_header
        temp_length = len(packet.split())
        zero_padding_length = self.packet_length - temp_length
        for i in range(zero_padding_length):
            packet = packet + ' 00'
            
        self.packet = packet[:]
        return self.packet
              
class client:
     
    def __init__(self,management_ip, username, password, data_ip=None):
    
        self.management_ip = management_ip
        self.data_ip = data_ip
        self.username = username
        self.password = password
        self.ssh_object = pxssh.pxssh()
        self.mac_address = None
         
    def connect(self):
        print "Connecting to host: " + self.ip_address + ", username: " + self.username + " password: " + self.password
        
        # ssh connecting to virtual machine 
        self.ssh_object.login(self.ip_address, self.username, self.password)
        
        # retrieve local mac address
        result, self.mac_address = self.execute_command("cat /sys/class/net/ens6/address")
        if result == False:
            print "Error while retreive local address"
            print self.mac_address
            exit(1)
        self.mac_address = self.mac_address.strip('\r')

        print "Connected"
         
    def logout(self):
        self.ssh_object.logout()
        
    def execute_command(self, cmd):
        self.ssh_object.sendline(cmd)
        self.ssh_object.prompt()
        output = self.ssh_object.before
        output = output.split('\n')
        output = output[1]
        
        self.ssh_object.sendline("echo $?")
        self.ssh_object.prompt()               
        exit_code = self.ssh_object.before 
        exit_code = exit_code.split('\n')
        exit_code = exit_code[1]
 
        if int(exit_code) != 0:
            print "terminate cp app was failed"
            print "Exit Code " + exit_code
            print "Error message: " + output
            return [False,output]
        
        return [True,output]
        
    def send_packet_to_nps(self, packet_string):
        os.system('rm -f pcap_files/tmp.pcap')
        string_to_pcap_file(packet_string, 'pcap_files/tmp.pcap')
        print "Send packet to NPS" 
        cmd = "tcpreplay --intf1=ens6 /swgwork/tomeri/sandbox/ALVS/verification/testing/dp/pcap_files/tmp.pcap" 
        result, output = self.execute_command(cmd)
        if result == False:
            print "Error while sending a packet to NPS"
            print output
            exit(1)
        
        
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

def init_logging(log_file): 
    if "log/" not in log_file:
        log_file = "log/" + log_file
        
    if not os.path.exists("log"):
        os.makedirs("log")
        
    FORMAT = 'Level Num:%(levelno)s %(filename)s %(funcName)s line:%(lineno)d   %(message)s'
    logging.basicConfig(format=FORMAT, filename=log_file, filemode='w', level=logging.DEBUG)
    logging.info("\n\nStart logging\nCurrent date & time " + time.strftime("%c") + "\n")

def read_test_arg(args):
    
    scenarios = range(1,100)
    dp_bin = 'alvs_dp'
    cp_bin = 'alvs_daemon'
    nps_ip = None
    host_ip = None
    hard_reset = False
    log_file = 'test_log.log'
    
    if '-host_ip' in args:
        if '-ezbox' in args:
            print "dont use ezbox parameter with host_ip parameter"
            exit(1)
        host_ip_index = args.index('-host_ip') + 1
        host_ip = args[host_ip_index]
        print "Host IP is: " + host_ip   
    
    if '-nps_ip' in args:
        if '-ezbox' in args:
            print "dont use ezbox parameter with nps_ip parameter"
            exit(1)
        nps_ip_index = args.index('-nps_ip') + 1
        nps_ip = args[nps_ip_index]
        print "NPS IP is: " + nps_ip
        
    if '-cp_bin' in args:
        cp_bin_index = args.index('-cp_bin') + 1
        cp_bin = args[cp_bin_index]
        print "CP Bin file is: " + cp_bin
      
    if '-dp_bin' in args:
        dp_bin_index = args.index('-dp_bin') + 1
        dp_bin = args[dp_bin_index]
        print "DP Bin file is: " + dp_bin
     
    if '-log_file' in args:
        log_file_index = args.index('-log_file') + 1
        log_file = args[log_file_index]
        print "Log file is: " + log_file
     
    if '-scenarios' in args:
        scenarios_index = args.index('-scenarios') + 1
        scenarios = args[scenarios_index]
        scenarios = scenarios.split(',')
        scenarios = map(int,scenarios)
        print "Scenarios to run: " + str(scenarios)
        
    if '-ezbox' in args:
        ezbox_index = args.index('-ezbox') + 1
        ezbox = args[ezbox_index]
        host_ip = os.popen("host " + ezbox + "-host")
        host_ip = host_ip.read()
        if 'not found' in host_ip:
            print "error - wrong ezbox name"
            exit(1)
        host_ip = host_ip.split()
        host_ip = host_ip[-1]
        
        nps_ip = os.popen("host " + ezbox + "-chip")
        nps_ip = nps_ip.read()
        if 'not found' in nps_ip:
            print "error - wrong ezbox name"
            exit(1)
        nps_ip = nps_ip.split()
        nps_ip = nps_ip[-1]
        
        print "ezbox setup: " + str(ezbox)
        print "host_ip: " + host_ip
        print "nps_ip: " + nps_ip
    else:
        print "Please specify the ezbox name that you want to use"
        exit(1)
            
    if '-reset' in args:
        hard_reset = True
        
    if '-help' in args:
        print "-host_ip - the ip address of the host (x.x.x.x)"
        print "-nps_ip - the ip address of the nps (x.x.x.x)"
        print "-ezbox - the name of the ezbox (ezbox name) - this is instead of the host ip/nps ip"
        print "-cp_bin - the cp bin file"
        print "-dp_bin - the dp bin file"
        print "-log_file - the log file"
        print "Examples to parameters:"
        print "<python script> -ezbox ezbox55 -cp_bin alvs_daemon -dp_bin alvs_dp -log_file test_log.log -scenarios 1,4,5,6"
        print "<python script> -ezbox ezbox29 -cp_bin alvs_daemon -dp_bin alvs_dp -log_file test_log.log -scenarios 2,3"
        exit()
        
    return {'log_file':log_file, 'host_ip':host_ip, 'nps_ip':nps_ip ,'cp_bin':cp_bin, 'dp_bin':dp_bin, 'scenarios':scenarios, 'ezbox':ezbox, 'hard_reset':hard_reset}       

def carry_around_add(a, b):
    c = a + b
    return (c & 0xffff) + (c >> 16)

def checksum(msg):
    s = 0
    for i in range(0, len(msg), 2):
        w = msg[i+1] + (msg[i] << 8)
        s = carry_around_add(s, w)
    return ~s & 0xffff

def string_to_pcap_file(packet_string, output_pcap_file):
    cmd = "echo 0000    " + packet_string + " >> tmp.txt"
    os.system(cmd)
    cmd = "text2pcap " + "tmp.txt " + output_pcap_file 
    os.system(cmd)
    os.system("rm tmp.txt")
    

    
    
    
    
    
