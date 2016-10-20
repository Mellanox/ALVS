#!/usr/bin/env python

#===============================================================================
# imports
#===============================================================================
# system  
import cmd
import logging
import os
import sys
import inspect
import time
import struct
import socket
import pprint
import struct
from multiprocessing import Process
from pexpect import pxssh
import pexpect
from ftplib import FTP
from netaddr import *
import subprocess
import itertools

import stf_path
from trex_client import *

# pythons modules 
from test_infra import *
from cmd import Cmd

parentdir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ALVSdir = os.path.dirname(parentdir)

from e2e_infra import *

#===============================================================================
# Classes
#===============================================================================
class TrexServer:
    def __init__(self, ip,
                vip         = None,
                weight      = 1):
        self.ip = ip
        self.vip = vip
        self.weight = weight
#===============================================================================
class TrexRunParams:
    def __init__(self, 
                 multiplier = 800,
                 nc = False,
                 p = False,
                 duration = 30,   
                 yaml_file_path = None,
                 cores = 5,
                 latency = 0):
        self.multiplier = multiplier            #Factor for bandwidth (multiplies the CPS of each template by this value).
        self.nc = nc                            #If set, will terminate exacly at the end of the duration. This provides a faster, more accurate TRex termination.
        self.p =p                               #Flow-flip. Sends all flow packets from the same interface.
        self.duration = duration                #Duration of the test (sec).
        self.yaml_file_path = yaml_file_path    #Traffic YAML configuration file.
        self.cores =cores                       #Number of cores.
        self.latency =latency                   #Run the latency daemon in this Hz rate. A value of zero (0) disables the latency check.
        
    def get_run_params(self):
        return dict( m = self.multiplier,
                     nc = self.nc,
                     p = self.p,
                     d = self.duration,
                     f = self.yaml_file_path,
                     c = self.cores,
                     l =self.latency)
#===============================================================================
class TrexTestParams:
    def __init__(self, 
                 clients_start,
                 services_start,
                 servers_start,
                 dual_port_mask,
                 client_count = None,
                 server_count = None,
                 service_count = None,
                 is_dual_port = True):
        self.client_count = client_count
        self.server_count = server_count
        self.service_count = service_count 
        self.clients_start = IPAddress(clients_start)  
        self.services_start = IPAddress(services_start)  
        self.servers_start = IPAddress(servers_start)
        self.dual_port_mask = IPAddress(dual_port_mask)
        self.is_dual_port = is_dual_port
        self.pcap_list = []
        self.yaml_filename = self.generate_yaml_file_name()
        self.run_params = TrexRunParams()
        
    def get_num_of_ports(self):
        num_of_ports = 1
        if (self.is_dual_port):
            num_of_ports = 2
        return num_of_ports
        
    def generate_yaml_file_name(self):
        current_time = strftime("%Y-%m-%d_%H-%M-%S", gmtime())
        yaml_filename = 'trex_conf_' + current_time + '.yaml'
        return yaml_filename
        
    def create_trex_logs_dir(self):
        current_time = strftime("%Y-%m-%d_%H-%M-%S", gmtime())  
        if not os.path.exists("trex_logs"):
            os.makedirs("trex_logs")
            
        self.logs_dir_name = 'trex_logs/trex_logs_'
        self.logs_dir_name += current_time
        cmd = "mkdir -p %s" %self.logs_dir_name
        os.system(cmd)
        
    def remove_trex_yaml_from_localhost(self):
        #Check whether yaml file exists
        if (os.path.isfile(self.yaml_filename)):
            os.remove(self.yaml_filename)
        
    def save_output_to_log_file(self, file_name, file_contents):
        log_file_name = self.logs_dir_name + '/' + file_name
        log_file = open(log_file_name, 'w')
        log_file.write(file_contents)
        log_file.close()
        
    def get_ip_range(self, ip_start_address, len): 
        ip_sub_lists  = self.get_ip_sub_lists(ip_start_address, len)
        ip_range = self.concat_sub_lists_to_list(ip_sub_lists)
        return map(str, ip_range)
            
    def get_ip_sub_lists(self, ip_start_address, len):
        num_of_cores = self.run_params.cores * self.get_num_of_ports() #cores per port * num of ports
        chunks = len/num_of_cores
        if (chunks == 0):
            raise ValueError('The number of ips:%s should be at least number of cores:%s'% (len,num_of_cores))
        ip_sub_lists = []        
        for i in range(num_of_cores):
            sub_list_start = ip_start_address + i*chunks
            #add dual port mask to odd elements
            if (i%2):
                sub_list_start+= self.dual_port_mask
            sub_list_end = sub_list_start + chunks -1
            ip_sub_lists.append(list(iter_iprange(sub_list_start,sub_list_end)))               
        return ip_sub_lists
                
    def concat_sub_lists_to_list(self, sub_lists):
        return list(itertools.chain.from_iterable(sub_lists))

    def get_clients_end(self):
        clients_end = self.clients_start + self.client_count 
        return clients_end
    
    def get_services_end(self):
        services_end = self.services_start + self.service_count 
        return services_end
    
    def get_vip_list(self):
        return self.get_ip_range(self.services_start, self.service_count)
    
    def get_client_list(self):
        return self.get_ip_range(self.clients_start, self.client_count)  
    
    def get_server_list(self):
        server_ip_list = self.get_ip_range(self.servers_start, self.server_count) 
        return [TrexServer(ip = server_ip) for server_ip in server_ip_list]
    
    def set_test_players(self, client_count, server_count, service_count):
        self.client_count = client_count
        self.server_count = server_count
        self.service_count = service_count
        
    def add_trex_pcap(self, trex_pcap):
        self.pcap_list.append(trex_pcap)
        
    def generate_trex_yaml(self):
        outfile = open(self.yaml_filename, 'w')
        outfile.write('################################################################\n')
        outfile.write('# Automaticaly generated yaml file from TRex test environment. #\n')
        outfile.write('################################################################\n')
        outfile.write('- duration : 0.1\n')
        outfile.write('  generator :  \n')
        outfile.write('          distribution : "seq"\n')
        outfile.write('          clients_start : "%s"\n'%str(self.clients_start))
        outfile.write('          clients_end   : "%s"\n'%str(self.get_clients_end()))
        outfile.write('          servers_start : "%s"\n'%str(self.services_start))
        outfile.write('          servers_end   : "%s"\n'%str(self.get_services_end()))
        outfile.write('          clients_per_gb : 201\n')
        outfile.write('          min_clients    : 101\n')
        outfile.write('          dual_port_mask : "%s" \n'%str(self.dual_port_mask))
        outfile.write('          tcp_aging      : 0\n')
        outfile.write('          udp_aging      : 0\n')
        outfile.write('  mac        : [0x0,0x0,0x0,0x1,0x0,0x00]\n')
        outfile.write('  cap_ipg    : true\n')
        outfile.write('  cap_info : \n')
        
        for trex_pcap in self.pcap_list:
            outfile.write('     - name: %s\n'%trex_pcap.name)
            outfile.write('       cps : %d\n'%trex_pcap.cps)
            outfile.write('       ipg : %d\n'%trex_pcap.ipg)
            outfile.write('       rtt : %d\n'%trex_pcap.rtt)
            outfile.write('       w   : %d\n'%trex_pcap.weight)
        
        outfile.close()
#===============================================================================     
class TrexPcap:
    def __init__(self, 
                 name,
                 cps = 1000,
                 ipg = 10000,
                 weight =1):
        self.name = name     #pcap file that will be used as a template.
        self.cps = cps       #connections per second
        self.ipg = ipg       #Inter-packet gap (microseconds). 10,000 = 10 msec
        self.rtt = ipg       #Should be the same as ipg
        self.weight = weight
#===============================================================================
class TrexHost:
    def __init__(self, trex_hostname):
        self.trex_info = self.get_trex_host_info(trex_hostname)       
        self.ssh_object = SshConnect(self.trex_info['host'], self.trex_info['username'], self.trex_info['password'])
        self.init_trex_host()
        self.trex_api =  CTRexClient(self.trex_info['host'])
        self.params = TrexTestParams(self.trex_info['clients_start'],self.trex_info['services_start'],
                                     self.trex_info['servers_start'],self.trex_info['dual_port_mask'])
        
    def get_trex_host_info(self, trex_hostname):
        if ('None' == trex_hostname):
            raise ValueError('Trex host does not exist for the required setup')
        
        # Open list file
        file_name = '%s/trex_hosts.csv' %(g_setups_dir)
        infile    = open(file_name, 'r')
        
        # Extract list from file  
        for line in infile:
            input_list = line.strip().split(',')
            if (trex_hostname == input_list[0]):
                trex_host_info = {'host':                input_list[0],
                                  'username':            input_list[1],
                                  'password':            input_list[2],
                                  'port1_client':        input_list[3],
                                  'port1_server':        input_list[4],
                                  'port2_client':        input_list[5],
                                  'port2_server':        input_list[6],
                                  'mng_ip':              input_list[7],
                                  'clients_start':       input_list[8],
                                  'services_start':      input_list[9],
                                  'servers_start':       input_list[10],
                                  'dual_port_mask':      input_list[11]}
                break
        
        if (not trex_host_info):
            raise ValueError('Trex host name: %s, has not been found under: %s'%(trex_hostname,file_name))
        
        return trex_host_info
        
    def init_trex_host(self):
        self.connect()
        if(not self.is_daemon_server_running()):
            self.trex_daemon_server_start()
        
    def connect(self):
        self.ssh_object.connect()
        
    def logout(self):
        self.ssh_object.recreate_ssh_object()
        self.ssh_object.logout()
        
    def trex_daemon_server(self, cmd):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called for " + cmd + " service"
        return self.ssh_object.execute_command("cd /home/trex/trex_core_v3/trex-core/scripts;./trex_daemon_server " + cmd)

    def trex_daemon_server_start(self):
        self.trex_daemon_server("start")

    def trex_daemon_server_stop(self):
        self.trex_daemon_server("stop")

    def trex_daemon_server_restart(self):
        self.trex_daemon_server("restart")
    
    def trex_daemon_server_show(self):
        return self.trex_daemon_server("show")
        
    def is_daemon_server_running(self):
        is_daemon_running = False
        result,output = self.trex_daemon_server_show()
        print(output)
        if ('TRex server daemon is running' in output):
            is_daemon_running = True
        return is_daemon_running
        
    def run_trex_test(self): 
        self.prepare_trex_to_run()
        self.trex_api.start_trex(**self.params.run_params.get_run_params())
        self.print_output_while_running()
        return self.trex_api.sample_to_run_finish()
    
    def prepare_trex_to_run(self):
        # push yaml file to server
        self.trex_api.push_files(self.params.yaml_filename)
        self.set_trex_yaml()

    def set_trex_yaml(self):
        trex_files_path = self.trex_api.get_trex_files_path()
        yaml_file_path = trex_files_path + self.params.yaml_filename
        self.params.run_params.yaml_file_path = yaml_file_path
        
    def print_output_while_running(self):
        print('Sample TRex until end')
        last_res = dict()
        while self.trex_api.is_running(dump_out = last_res):
            print "CURRENT RESULT OBJECT:"
            obj = self.trex_api.get_result_obj()
            print obj
            time.sleep(5)
        print('End of TRex run')
#===============================================================================    
class TrexTestResult:
    def __init__(self, 
                 trex_result = None,
                 ezbox_stats = None,
                 test_resources = None):
        self.trex_result = trex_result
        self.ezbox_stats = ezbox_stats
        self.test_resources = test_resources
        self.expected_sd = 0.05
        self.test_bool_rc = False
        
    def get_rc(self):
        rc_val = 1
        if (self.test_bool_rc):
            rc_val= 0
            
        return rc_val
           
    def set_result(self, trex_result, ezbox_stats, test_resources):
        self.trex_result = trex_result
        self.ezbox_stats = ezbox_stats
        self.test_resources = test_resources
        
    def general_checker(self):
        self.get_test_stats()
        self.analyze_results()
        self.print_result()
        
    def analyze_results(self):
        drop_rc = self.analyze_drops()
        if (drop_rc):
            self.test_bool_rc = True
    
    def analyze_drops(self):
        rc = False
        self.real_sd = abs(self.dropped_packets)/self.trex_client_tx_packets
        if (self.real_sd < self.expected_sd):
            rc = True
        return rc
    
    def get_test_stats(self):
        self.ezbox_rx_packets = self.get_ezbox_rx_packets()
        self.trex_client_tx_packets = self.get_trex_client_tx_packets()
        self.dropped_packets = self.trex_client_tx_packets - self.ezbox_rx_packets
               
    def print_result(self):
        self.print_packets_by_ports()
        self.print_packets_drop()
        self.print_real_sd()
        self.print_final_result()
        
    def print_final_result(self):
        if (self.test_bool_rc):
            print 'Test passed !!!'
        else:
            print 'Test failed !!!'
    
    def print_trex_res_obj(self):
        print (self.trex_result)
        
    def print_packets_by_ports(self):
        tx_ptks_dict = self.trex_result.get_last_value('trex-global.data', 'opackets-*')
        print('TX by ports:')
        print('  |  '.join(['%s: %s' % (k.split('-')[-1], tx_ptks_dict[k]) for k in sorted(tx_ptks_dict.keys())]))
        print('RX by ports:')
        rx_ptks_dict = self.trex_result.get_last_value('trex-global.data', 'ipackets-*')
        print('  |  '.join(['%s: %s' % (k.split('-')[-1], rx_ptks_dict[k]) for k in sorted(rx_ptks_dict.keys())]))        
        
    def print_packets_drop(self):
        print('\nTRex_client_tx_packets:%s'%str(self.trex_client_tx_packets))
        print('\nezbox_rx_packets:%s'%str(self.ezbox_rx_packets))
        print('\nDropped packets:%s \n'%str(self.dropped_packets))
        
    def print_real_sd(self):
        print 'real sd is: ' + str(self.real_sd)
    
    def get_ezbox_rx_packets(self):
        ezbox_rx_packets = 0
        for service in self.test_resources['vip_list']:
            ezbox_rx_packets += int(self.ezbox_stats['service_stats'][service]['IN_PACKETS'])
        return ezbox_rx_packets
        
    def get_trex_client_tx_packets(self):
        trex_client_tx_packets = 0
        tx_ptks_dict = self.trex_result.get_last_value('trex-global.data', 'opackets-*')
        for k in sorted(tx_ptks_dict.keys()):
            #Sum only clients tx packets(on even port numbers)
            if (int((k.split('-')[-1]))%2) == 0:
                trex_client_tx_packets += int(tx_ptks_dict[k])
                
        return trex_client_tx_packets
#=============================================================================== 
def get_ezbox_stats(ezbox):
    ezbox.get_ipvs_stats();
    stats_results = ezbox.read_stats_on_syslog()
    return stats_results
#===============================================================================
def get_trex_test_resources(setup_num, client_count, server_count, service_count):
    print "FUNCTION " + sys._getframe().f_code.co_name + " called"
    
    ezbox = ezbox_host(setup_num)
    trex = TrexHost(ezbox.setup['trex_hostname'])
    trex.params.set_test_players(client_count, server_count, service_count) 
    vip_list = trex.params.get_vip_list()  
    client_list = trex.params.get_client_list()
    server_list = trex.params.get_server_list()

    # create resources dictionary
    dict={}
    dict['vip_list']    = vip_list
    dict['server_list'] = server_list
    dict['client_list'] = client_list 
    dict['ezbox']       = ezbox 
    dict['trex']        = trex 
    return dict

#===============================================================================
def init_trex_players(test_resources, test_config={}):
    print "FUNCTION " + sys._getframe().f_code.co_name + " called"
    
    ezbox = test_resources['ezbox']    
    init_ezbox(ezbox, test_resources['server_list'], test_resources['vip_list'], test_config,)
        
#===============================================================================
def clean_trex_players(test_resources, use_director = False, stop_ezbox = False):
    print "FUNCTION " + sys._getframe().f_code.co_name + " called"

    ezbox = test_resources['ezbox']
    trex = test_resources['trex']
            
    if ezbox:
        ezbox.ssh_object.recreate_ssh_object()
        ezbox.run_app_ssh.recreate_ssh_object()
        ezbox.syslog_ssh.recreate_ssh_object()
        ezbox.clean_arp_table()
        clean_ezbox(ezbox, use_director, stop_ezbox,)
        
    if trex:
        trex.logout()
        trex.params.remove_trex_yaml_from_localhost()
