#!/usr/bin/env python

import os
import sys
import pexpect
from pexpect import pxssh
import random

sys.path.append('EZdk/tools/EZcpPyLib/lib')
from ezpy_cp import EZpyCP
 
import time
import struct
import socket
import inspect
import logging

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
    

    
    
    
    
    
