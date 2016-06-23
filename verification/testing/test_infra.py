#!/usr/bin/env python

import os
import sys
import pexpect
from pexpect import pxssh
import random

sys.path.append('EZdk/tools/EZcpPyLib/lib')
from ezpy_cp import EZpyCP
 
from common_infra import *

import time
import struct
import socket
 
import logging


class real_server:
    def __init__(self, management_ip, data_ip, username='root', password='3tango', eth = 'ens6'):
        self.data_ip = data_ip
        self.management_ip = management_ip
        self.password = password
        self.username = username
        self.ssh_object = SshConnect(hostname=management_ip, username=username, password=password)
        self.eth = eth
        self.connect()
        
    def disable_arp(self):
        # Disable IP forwarding
        rc, output = self.ssh_object.execute_command("echo \"0\" > /proc/sys/net/ipv4/ip_forward")
        if rc != True:
            print "ERROR: Disable IP forwarding failed. rc=" + str(rc) + " " + output
            return

        # Enable arp ignore
        rc, output = self.ssh_object.execute_command("echo \"1\" > /proc/sys/net/ipv4/conf/all/arp_ignore")
        if rc != True:
            print "ERROR: Enable IP forwarding failed. rc=" + str(rc) + " " + output
            return
        cmd = "echo \"1\" >/proc/sys/net/ipv4/conf/" + self.eth + "/arp_ignore"
        rc, output = self.ssh_object.execute_command(cmd)
        if rc != True:
            print "ERROR: Enable arp ignore ("+ self.eth + ") failed. rc=" + str(rc) + " " + output
            return
        
        # Enable arp announce
        rc, output = self.ssh_object.execute_command("echo \"2\" > /proc/sys/net/ipv4/conf/all/arp_announce")
        if rc != True:
            print "ERROR: Enable arp announce (all) failed. rc=" + str(rc) + " " + output
            return
        cmd = "echo \"2\" > /proc/sys/net/ipv4/conf/" + self.eth + "/arp_announce"
        rc, output = self.ssh_object.execute_command(cmd)
        if rc != True:
            print "ERROR: Enable arp announce ("+ self.eth + ") failed. rc=" + str(rc) + " " + output
            return
           
    def enable_arp(self):
        # Enable IP forwarding
        rc, output = self.ssh.execute_command("echo \"1\" >/proc/sys/net/ipv4/ip_forward")
        if rc != True:
            print "ERROR: Enable IP forwarding failed. rc=" + str(rc) + " " + output
            return

        # Disable arp ignore
        rc, output = self.ssh.execute_command("echo \"0\" > /proc/sys/net/ipv4/conf/all/arp_ignore")
        if rc != True:
            print "ERROR: Disable IP forwarding failed. rc=" + str(rc) + " " + output
            return
        cmd = "echo \"0\" >/proc/sys/net/ipv4/conf/" + self.eth + "/arp_ignore"
        rc, output = self.ssh.execute_command(cmd)
        if rc != True:
            print "ERROR: Disable arp ignore (" + self.eth +") failed. rc=" + str(rc) + " " + output
            return
        
        # Disable arp announce
        rc, output = self.ssh.execute_command("echo \"0\" > /proc/sys/net/ipv4/conf/all/arp_announce")
        if rc != True:
            print "ERROR: Disable arp announce (all) failed. rc=" + str(rc) + " " + output
            return
        cmd = "echo \"0\" > /proc/sys/net/ipv4/conf/" + self.eth + "/arp_announce"
        rc, output = self.ssh.execute_command(cmd)
        if rc != True:
            print "ERROR: Disable arp announce (" + self.eth + ") failed. rc=" + str(rc) + " " + output
            return
        
    def connect(self):
        
        self.ssh_object.connect()
        
        # retrieve local mac address
        result, self.mac_address = self.ssh_object.execute_command("cat /sys/class/net/ens6/address")
        if result == False:
            print "Error while retreive local address"
            print self.mac_address
            exit(1)
        
        self.mac_address = self.mac_address[0:17]
        
        self.disable_arp()
        
    def close_server(self):
        cmd = "ifconfig lo:0 down"
        result, output = self.ssh_object.execute_command(cmd)
        if result != True:
            print "ERROR: close server failed" + str(result) + " " + output
            return
        self.enable_arp()
        
    def capture_packets_from_service(self, service, tcpdump_params=''):

        self.dump_pcap_file = '/tmp/server_dump.pcap'
        self.ssh_object.execute_command("rm -f " + self.dump_pcap_file)
#         cmd = 'nohup tcpdump -w ' + self.dump_pcap_file + ' -n -i ens6 ' + tcpdump_params + ' dst ' + service.virtual_ip + ' > /dev/null 2>&1 & sleep 3s && pkill -HUP -f tcpdump &'
#         cmd = 'tcpdump -w ' + self.dump_pcap_file + ' -n -i ens6 ' + tcpdump_params + ' dst ' + service.virtual_ip + ' > /dev/null 2>&1 & sleep 3s && pkill -HUP -f tcpdump'
        cmd = 'pkill -HUP -f tcpdump; tcpdump -w ' + self.dump_pcap_file + ' -n -i ens6 ether host ' + self.mac_address + ' and dst ' + service.virtual_ip + ' &'

        logging.log(logging.DEBUG,"running on server command:\n"+cmd)
        self.ssh_object.ssh_object.sendline(cmd)
        self.ssh_object.ssh_object.prompt()
                
    def stop_capture(self):
        cmd = 'pkill -HUP -f tcpdump'
        self.ssh_object.ssh_object.sendline(cmd)
        self.ssh_object.ssh_object.prompt()
        output = self.ssh_object.ssh_object.before
        logging.log(logging.DEBUG,"output from tcpdump:\n"+output)
        
        # send a dummy command to clear all unnecessary outputs
        self.ssh_object.ssh_object.sendline("echo $?")
        self.ssh_object.ssh_object.prompt()
        
        num_of_packets_received = check_packets_on_pcap(self.dump_pcap_file, self.ssh_object.ssh_object)
        
        return num_of_packets_received
    
#         cmd = "tcpdump -r %s | wc -l "%self.dump_pcap_file
#         logging.log(logging.DEBUG,"executing: "+cmd)
#         self.ssh_object.ssh_object.sendline(cmd)
#         self.ssh_object.ssh_object.prompt()
#         output = self.ssh_object.ssh_object.before
#         logging.log(logging.DEBUG,"output: "+output)
#         
#         try:
#             num_of_packets_received = int(output.split('\n')[2])
#             return num_of_packets_received
#         except:
#             print "ERROR, cannot get num of packets that was received"
#             print "output: " + output
#             return -1
            
        
    def compare_received_packets_to_pcap_file(self, pcap_file, pcap_file_on_server=None):
#         local_dir = os.getcwd()
        if pcap_file_on_server == None:
            pcap_file_on_server = self.dump_pcap_file
            
        # copy the pcap file to compare
        cmd = "sshpass -p %s scp ../../../bin/%s %s@%s:/tmp/%s"%(self.password, pcap_file, self.username, self.management_ip, pcap_file)
        os.system(cmd)
        
        #copy the checker python script
        checker_script_name = '../compare_pcap_files.py'
        cmd = "sshpass -p %s scp ../../../bin/%s %s@%s:/tmp/%s"%(self.password, checker_script_name, self.username, self.management_ip, checker_script_name)
        os.system(cmd)
        
        cmd = '/tmp/compare_pcap_files.py ' + pcap_file_on_server + ' ' + pcap_file 
        result, self.mac_address = self.ssh_object.execute_command(cmd)
        print result
#         if result == False:
#             print "Error while retreive local address"
#             print self.mac_address
#             exit(1)
        
#         cmd = local_dir + '/../'
                   
class service:
    
    services_dictionary = {}
    services_counter = 0
    
    def __init__(self, ezbox, virtual_ip, port, schedule_algorithm = 'source_hash'):
        self.virtual_ip = virtual_ip
        temp_virtual_ip = virtual_ip.split('.')
        self.virtual_ip_hex_display = '%02x %02x %02x %02x'%(int(temp_virtual_ip[0]), int(temp_virtual_ip[1]), int(temp_virtual_ip[2]), int(temp_virtual_ip[3]))
        self.port = port
        self.ezbox = ezbox
        self.servers = []
        if schedule_algorithm == 'source_hash':
            self.schedule_algorithm = 'sh'
        elif schedule_algorithm == 'source_hash_with_source_port':
            self.schedule_algorithm = 'sh -b sh-port'
        else:
            print 'This Schedule algorithm is not supported'
            exit(1)
        result,output = self.ezbox.execute_command_on_host("ipvsadm -A -t %s:%s -s %s"%(self.virtual_ip, self.port, self.schedule_algorithm))
        if result == False:
            print "ERROR, Failed to add service"
            print output
            exit(1)
            
#         ezbox.config_vips([self.virtual_ip])
        cmd = "ifconfig " + ezbox.setup['interface'] + ":" + str(self.services_counter+1)+ " "  + virtual_ip + " netmask 255.255.0.0"
        result,output = self.ezbox.execute_command_on_host(cmd)
        if result == False:
            print "ERROR, Failed to add service"
            print output
            exit(1)
        
        # service number is been use when we create the interface/port on the ezbox host (eth0:service_num)
        self.service_number = service.services_counter
        service.services_counter += 1
        
        # add an entry in services dictionary, will use it later on the server director on ezbox
        service.services_dictionary[self.virtual_ip] = []
        
    def add_server(self, new_server, weight = '1'):
        self.servers.append(new_server)
        cmd = "ipvsadm -a -t %s:%s -r %s:%s -w %s"%(self.virtual_ip, self.port, new_server.data_ip, self.port, weight)

        result, output = self.ezbox.execute_command_on_host(cmd)        
        if result == False:
            print "Error while removing server from service"
            print output
            exit(1)
            
        # create arp static entry , workaround for a problem
        result, output = self.ezbox.execute_command_on_host("arp -s %s %s"%(new_server.data_ip, new_server.mac_address))
        if result == False:
            print "Error while adding arp entry"
            print output
            exit(1)
         
        cmd = "ifconfig lo:0 " + self.virtual_ip + " netmask 255.255.255.255"
        result, output = new_server.ssh_object.execute_command(cmd)
        if result != True:
            print "ERROR: setting loopback port failed" + str(result) + " " + output
            return
        
        # add the server to the services dictionary, will use it later on the director initialize
        service.services_dictionary[self.virtual_ip].append(new_server.data_ip)
    
    def remove_server(self,server_to_delete):
        self.servers.remove(server_to_delete)
        result, output = self.ezbox.execute_command_on_host("ipvsadm -d -t %s:%s -r %s:%s"%(self.virtual_ip, self.port, server_to_delete.data_ip, self.port))
        if result == False:
            print "Error while removing server from service"
            print output
            exit(1)
    
    def modify_server(self, server_to_modify):
        
        if server_to_modify not in self.servers:
            print "Error, Server is not exist on Service"
            exit(1)
             
        result, output = self.ezbox.execute_command_on_host("ipvsadm -e -t %s:%s -r %s:%s"%(self.virtual_ip, self.port, server_to_modify.data_ip, self.port))
        if result == False:
            print "Error while removing Server from Service"
            exit(1)
        
    def remove_service(self):
        result,output = self.ezbox.execute_command_on_host("ipvsadm -D -t %s:%s"%(self.virtual_ip, self.port))
        if result == False:
            print "ERROR, Failed to remove service"
            print output
            exit(1)
            
        cmd = "ifdown " + self.ezbox.setup['interface'] + ":" + str(self.service_number+1)+ " "  + self.virtual_ip 
        result,output = self.ezbox.execute_command_on_host(cmd)
        if result == False:
            print "ERROR, Failed to add service"
            print output
            exit(1)
            
        # remove from the global service dictionary
        del service.services_dictionary[self.virtual_ip]
           
class arp_entry:
     
    def __init__(self, ip_address, mac_address, interface=None, flags=None, mask=None, type=None):
        self.ip_address = ip_address
        self.mac_address = mac_address
        self.interface = interface
        self.flags = flags
        self.mask = mask
        self.type = type
         

class tcp_packet:
    
    packets_counter = 0
    
    def __init__(self, mac_da, mac_sa, ip_src, ip_dst, tcp_source_port, tcp_dst_port, tcp_reset_flag = False, tcp_fin_flag = False, tcp_sync_flag = False, packet_length = 64):
        self.mac_sa = mac_sa
        self.mac_da = mac_da
        
        self.ip_src = ip_src
        # change the format of the ip to "xx xx xx xx" instead of "xxx.xxx.xxx.xxx"
        if '.' in self.ip_src:
            ip_src = ip_src.split('.')
            self.ip_src = '%02x %02x %02x %02x'%(int(ip_src[0]), int(ip_src[1]), int(ip_src[2]), int(ip_src[3]))
        
        self.ip_dst = ip_dst
        self.tcp_source_port = tcp_source_port
        self.tcp_dst_port = tcp_dst_port
        self.tcp_reset_flag = tcp_reset_flag
        self.tcp_fin_flag = tcp_fin_flag
        self.tcp_sync_flag = tcp_sync_flag
        self.packet_length = packet_length
        self.packet = ''
        self.pcap_file_name = 'verification/testing/dp/pcap_files/packet' + str(self.packets_counter) + '.pcap'
        tcp_packet.packets_counter += 1
    
    def generate_packet(self):
        logging.log(logging.DEBUG, "generating the packet, packet size is %d"%self.packet_length)
        l2_header = self.mac_da + ' ' + self.mac_sa + ' ' + '08 00'
        data = '45 00 00 2e 00 00 40 00 40 06 00 00 ' + self.ip_src + ' ' + self.ip_dst  
        data = data.split()
        data = map(lambda x: int(x,16), data)
        ip_checksum = checksum(data)
        ip_checksum = '%04x'%ip_checksum
        ip_header = '45 00 00 2e 00 00 40 00 40 06 ' + ip_checksum[0:2] + ' ' + ip_checksum[2:4] + ' ' + self.ip_src + ' ' + self.ip_dst
        
        flag = 0
        if self.tcp_fin_flag:
            flag += 1
        if self.tcp_sync_flag:
            flag += 2    
        if self.tcp_reset_flag:
            flag += 4
        flag = '%02x'%flag

        data = self.tcp_source_port + ' ' + self.tcp_dst_port + ' 00 00 00 00 00 00 00 00 50 ' + flag + ' FF FC 00 00 00 00'
        data = data.split()
        data = map(lambda x: int(x,16), data)
        tcp_checksum = checksum(data)
        tcp_checksum = '%04x'%tcp_checksum
        tcp_header = self.tcp_source_port + ' ' + self.tcp_dst_port + ' 00 00 00 00 00 00 00 00 50 ' + flag + ' FF FC ' + tcp_checksum[0:2] + ' ' + tcp_checksum[2:4] + ' 00 00'
        
        packet = l2_header + ' ' + ip_header + ' ' + tcp_header
        temp_length = len(packet.split())
        zero_padding_length = self.packet_length - temp_length
        for i in range(zero_padding_length):
            packet = packet + ' 00'
            
        self.packet = packet[:]
        
        string_to_pcap_file(self.packet, self.pcap_file_name)
        
        return self.packet
              
class client:
     
    def __init__(self,management_ip, data_ip, username='root', password='3tango'):
    
        self.management_ip = management_ip
        self.data_ip = data_ip
        client_ip = self.data_ip.split('.')
        self.hex_display_to_ip = '%02x %02x %02x %02x'%(int(client_ip[0]), int(client_ip[1]), int(client_ip[2]), int(client_ip[3]))
        self.username = username
        self.password = password
        
        self.ssh_object = pxssh.pxssh()
        self.mac_address = None
        
        self.connect()
         
    def connect(self):
        print "Connecting to host: " + self.management_ip + ", username: " + self.username + " password: " + self.password
        
        # ssh connecting to virtual machine 
        self.ssh_object.login(self.management_ip, self.username, self.password)
        
        # retrieve local mac address
        result, self.mac_address = self.execute_command("cat /sys/class/net/ens6/address")
        if result == False:
            print "Error while retreive local address"
            print self.mac_address
            exit(1)
        self.mac_address = self.mac_address.strip('\r')
        self.mac_address = self.mac_address.replace(':', ' ')
        

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
            print "Exit Code " + exit_code
            print "Error message: " + output
            return [False,output]
        
        return [True,output]
        
    def send_packet_to_nps(self, pcap_file):
        logging.log(logging.DEBUG,"Send packet to NPS") 
        local_dir = os.getcwd()
        cmd = "tcpreplay --intf1=ens6 " + local_dir + "/" + pcap_file
        logging.log(logging.DEBUG,"run command on client:\n" + cmd) #todo
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
        
        if ezbox == 'ezbox29':
            ezbox_interface = 'eth0.5'
            data_ip = '192.168.0.1'
            setup_num = 1
        if ezbox == 'ezbox24':
            ezbox_interface = 'eth0.6'
            data_ip = '192.168.0.2'
            setup_num = 2
        if ezbox == 'ezbox35':
            ezbox_interface = 'eth0.7'
            data_ip = '192.168.0.3'
            setup_num = 3
        if ezbox == 'ezbox55':
            ezbox_interface = 'eth0.8'
            data_ip = '192.168.0.4'
            setup_num = 4                                    
        
        print "ezbox setup: " + str(ezbox)
        print "host_ip: " + host_ip
        print "nps_ip: " + nps_ip
        print "host data ip: " + data_ip
        print "ezbox interface is: " + ezbox_interface
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
        
    return {'setup_num':setup_num, 'ezbox_interface':ezbox_interface, 'data_ip':data_ip, 'log_file':log_file, 'host_ip':host_ip, 'nps_ip':nps_ip ,'cp_bin':cp_bin, 'dp_bin':dp_bin, 'scenarios':scenarios, 'ezbox':ezbox, 'hard_reset':hard_reset}       

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
#     os.system("rm -f tmp.txt")
    os.system("rm -f " + output_pcap_file)
    cmd = "echo 0000    " + packet_string + " > tmp.txt"   
    os.system(cmd)
    cmd = "text2pcap " + "tmp.txt " + output_pcap_file + ' &> /dev/null'
    os.system(cmd)
  
def create_pcap_file(packets_list, output_pcap_file_name):
    
    # create temp text file
    os.system("rm -f verification/testing/dp/temp.txt")
    os.system("rm -f "+ output_pcap_file_name)
#     time.sleep(5)
#     cmd = "(echo 0000    " + packets_list[0].packet + ' '
#     for packet in packets_list[1:]:
#         cmd += " & echo 0000    " + packet.packet
#     cmd += ") > temp.txt"   
        
    for packet in packets_list:
        os.system("echo 0000    " + packet.packet + " >> verification/testing/dp/temp.txt")

    os.system("echo 0000 >> verification/testing/dp/temp.txt")
    
    cmd = "text2pcap " + "verification/testing/dp/temp.txt " + output_pcap_file_name + ' &> /dev/null'
    os.system(cmd)
    
    return output_pcap_file_name
    
def compare_pcap_files(file_name_1, file_name_2):
    
#     num_of_packets_1 = int(os.popen("tcpdump -r %s | wc -l"%file_name_1).read().strip('\n'))
#     num_of_packets_2 = int(os.popen("tcpdump -r %s | wc -l"%file_name_2).read().strip('\n'))
    
    num_of_packets_1 = os.popen("tcpdump -r %s"%file_name_1).read().strip('\n')
    num_of_packets_2 = os.popen("tcpdump -r %s"%file_name_1).read().strip('\n')
    
    
    if len(num_of_packets_1) != len(num_of_packets_2):
        print "ERROR, num of packets on pcap files is not equal"
        print "first pcap num of packets " + num_of_packets_1
        print "second pcap num of packets " + num_of_packets_2
        exit(1) 
    
    
    data_1 = os.popen("tcpdump -r %s -XX "%file_name_1).read().split('\n')
    data_2 = os.popen("tcpdump -r %s -XX "%file_name_2).read().split('\n')
    
    
    for i in range(len(data_1)):
        print i
        print data_1[i]
        print data_2[i]
        
        if data_1[i] in num_of_packets_1: # this line we are not comparing (description of the packet, includes imestamp)
            continue
       
        if data_1[i] != data_2[i]:
            return False
        
    return True
     
def check_packets_on_pcap(pcap_file_name, ssh_object=None):
    cmd = "tcpdump -r %s | wc -l "%pcap_file_name
    logging.log(logging.DEBUG,"executing: "+cmd)
    
    if ssh_object == None:
        output = os.popen(cmd)
        output = output.read()
        try:
            num_of_packets_received = int(output.split('\n')[1])
            return num_of_packets_received
        except:
            print "ERROR, cannot get num of packets that was received"
            print "output: " + output
            return -1
    else:
        ssh_object.sendline(cmd)
        ssh_object.prompt()
        output = ssh_object.before
        logging.log(logging.DEBUG,"output: "+output)
    
        try:
            num_of_packets_received = int(output.split('\n')[2])
            return num_of_packets_received
        except:
            print "ERROR, cannot get num of packets that was received"
            print "output: " + output
            return -1
                
       
    
    
    
    
    
    
