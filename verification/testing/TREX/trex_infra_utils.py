#!/usr/bin/env python

#===============================================================================
# imports
#===============================================================================
# system  
from time import gmtime, strftime
from netaddr import *
import itertools
import os
import sys
import inspect

# TRex api  
import stf_path
from trex_client import *

#Local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.append(parentdir) 
from common_infra import *
#===============================================================================
# Classes
#===============================================================================
class TrexRunParams:
    def __init__(self, 
                 multiplier = 100,
                 nc = False,
                 p = False,
                 duration = 30,   
                 yaml_file_path = None,
                 config_file = None,
                 cores = 4,
                 latency = 0):
        self.multiplier = multiplier            #Factor for bandwidth (multiplies the CPS of each template by this value).
        self.nc = nc                            #If set, will terminate exacly at the end of the duration. This provides a faster, more accurate TRex termination.
        self.p = p                              #Flow-flip. Sends all flow packets from the same interface.
        self.duration = duration                #Duration of the test (sec).
        self.yaml_file_path = yaml_file_path    #Traffic YAML configuration file.
        self.config_file = config_file          #TRex topology file
        self.cores =cores                       #Number of cores.
        self.latency =latency                   #Run the latency daemon in this Hz rate. A value of zero (0) disables the latency check.
        
    def set_multiplier(self, multiplier):
        self.multiplier = multiplier

    def set_duration(self, duration):
        self.duration = duration
        
    def set_cores(self, cores):
        self.cores = cores   
        
    def set_config_file(self, config_file):
        self.config_file = config_file   
    
    def get_run_params(self):
        return dict( m = self.multiplier,
                     nc = self.nc,
                     p = self.p,
                     d = self.duration,
                     f = self.yaml_file_path,
                     cfg = self.config_file,
                     c = self.cores,
                     l =self.latency)
#===============================================================================
class TrexTestParams:
    def __init__(self,
                 trex_info,
                 client_count = None,
                 server_count = None,
                 is_dual_port = True):
        self.clients_start = IPAddress(trex_info['clients_start']) 
        self.client_count = client_count
        self.servers_start = IPAddress(trex_info['servers_start'])
        self.server_count = server_count 
        self.dual_port_mask = IPAddress(trex_info['dual_port_mask'])
        self.is_dual_port = is_dual_port
        self.pcap_list = []
        self.yaml_filename = self.generate_yaml_file_name()
        self.run_params = TrexRunParams(config_file = trex_info['config_file'])
        
    def generate_yaml_file_name(self):
        current_time = strftime("%Y-%m-%d_%H-%M-%S", gmtime())
        yaml_filename = 'trex_conf_' + current_time + '.yaml'
        return yaml_filename
        
    def remove_trex_yaml_from_localhost(self):
        if (os.path.isfile(self.yaml_filename)):
            os.remove(self.yaml_filename)

    def get_clients_end(self):
        clients_end = self.clients_start + self.client_count 
        return clients_end
    
    def get_servers_end(self):
        servers_end = self.servers_start + self.server_count 
        return servers_end
    
    def get_client_list(self):
        client_list=self.get_client_list_first_pair()
        client_list+=self.get_client_list_second_pair()
        return client_list
    
    def get_client_list_first_pair(self):
        return self.get_ip_string_list(self.clients_start,self.get_clients_end())
    
    def get_client_list_second_pair(self):
        client_start = self.clients_start + self.dual_port_mask
        client_end = self.get_clients_end() + self.dual_port_mask
        return self.get_ip_string_list(client_start,client_end)
    
    def get_server_list(self):
        server_list=self.get_server_list_first_pair()
        server_list+=self.get_server_list_second_pair()
        return server_list
    
    def get_server_list_first_pair(self):
        return self.get_ip_string_list(self.servers_start,self.get_servers_end())
    
    def get_server_list_second_pair(self):
        server_start = self.servers_start + self.dual_port_mask
        server_end = self.get_servers_end() + self.dual_port_mask
        return self.get_ip_string_list(server_start,server_end)
    
    def get_ip_string_list(self, ip_start, ip_end):
        ip_list = list(iter_iprange(ip_start,ip_end))
        return map(str, ip_list)
        
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
        outfile.write('          servers_start : "%s"\n'%str(self.servers_start))
        outfile.write('          servers_end   : "%s"\n'%str(self.get_servers_end()))
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
    def __init__(self, trex_index, client_count, server_count):
        self.trex_info = self.get_trex_host_info(trex_index)       
        self.ssh_object = SshConnect(self.trex_info['host'], self.trex_info['username'], self.trex_info['password'])
        self.trex_api =  CTRexClient(self.trex_info['host'])
        self.params = TrexTestParams(self.trex_info, client_count, server_count)
        self.init_trex_host()
        
    def get_trex_host_info(self, trex_index):
        if (trex_index < 0 or trex_index > 2):
            raise ValueError('trex index is out of range')
        
        # Open list file
        file_name = '%s/setup7_trex_hosts.csv' %(g_setups_dir)
        infile    = open(file_name, 'r')
        
        # Extract list from file  
        for index,line in enumerate(infile):
            input_list = line.strip().split(',')
            if (index == trex_index):
                trex_host_info = {'host':                input_list[0],
                                  'username':            input_list[1],
                                  'password':            input_list[2],
                                  'port0_client_mac':    input_list[3],
                                  'port0_server_mac':    input_list[4],
                                  'port1_client_mac':    input_list[5],
                                  'port1_server_mac':    input_list[6],
                                  'mng_ip':              input_list[7],
                                  'clients_start':       input_list[8],
                                  'servers_start':       input_list[9],
                                  'dual_port_mask':      input_list[10],
                                  'config_file':         input_list[11]}
                break
        
        if (not trex_host_info):
            raise ValueError('Trex index: %s, has not been found under: %s'%(trex_index,file_name))
        
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
        return self.ssh_object.execute_command("cd /opt/trex/trex_v2_14/trex-core/scripts;./trex_daemon_server " + cmd)

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
        
    def run_trex_test(self, trex_test_result_queue): 
        self.prepare_trex_to_run()
        self.trex_api.start_trex(**self.params.run_params.get_run_params())
        self.print_output_while_running()
        result = self.trex_api.sample_to_run_finish()
        trex_test_result_queue.put(result)
    
    def prepare_trex_to_run(self):
        self.trex_api.push_files(self.params.yaml_filename)
        self.set_trex_yaml_path()

    def set_trex_yaml_path(self):
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
            self.print_tx_rate(obj)
            time.sleep(5)
        print('End of TRex run')
        
    def print_tx_rate(self, obj):
        current_tx_rate = int(obj.get_current_tx_rate()['m_tx_bps']) / 1e9
        print "Tx Rate: " + str(current_tx_rate) + "GB"
        
#===============================================================================    
class TrexTestResult:
    def __init__(self, 
                 trex_result_list = [],
                 ezbox_stats = None):
        self.trex_result_list = trex_result_list
        self.expected_sd = 0.05
        self.test_bool_rc = False
           
    def set_result(self, trex_result_list):
        self.trex_result_list = trex_result_list
        
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
        self.real_sd = abs(self.dropped_packets)/self.trex_tx_packets
        if (self.real_sd < self.expected_sd):
            rc = True
        return rc
    
    def get_test_stats(self):  
        self.trex_tx_packets = self.get_trex_tx_packets()
        self.trex_rx_packets = self.get_trex_rx_packets()
        self.dropped_packets = self.trex_tx_packets - self.trex_rx_packets
               
    def print_result(self):
        self.print_trex_cpu_util()
        self.print_packets_by_ports()
        self.print_packets_drop()
        self.print_real_sd()
        
    def get_rc(self):
        rc_val = 1
        if (self.test_bool_rc):
            rc_val= 0   
        return rc_val
    
    def print_final_result(self):
        if (self.test_bool_rc):
            print 'Test passed !!!'
        else:
            print 'Test failed !!!'
            
    def print_trex_cpu_util(self):
        print('\n CPU utilization:')
        for trex_result in self.trex_result_list:
            print(trex_result.get_value_list('trex-global.data.m_cpu_util'))
        
    def print_packets_by_ports(self):
        for trex_result in self.trex_result_list:
            tx_ptks_dict = trex_result.get_last_value('trex-global.data', 'opackets-*')
            print('\n TX by ports:')
            print('  |  '.join(['%s: %s' % (k.split('-')[-1], tx_ptks_dict[k]) for k in sorted(tx_ptks_dict.keys())]))
            print('\n RX by ports:')
            rx_ptks_dict = trex_result.get_last_value('trex-global.data', 'ipackets-*')
            print('  |  '.join(['%s: %s' % (k.split('-')[-1], rx_ptks_dict[k]) for k in sorted(rx_ptks_dict.keys())]))        
        
    def print_packets_drop(self):
        print('\n TRex_total_tx_packets:%s'%str(self.trex_tx_packets))
        print('\n TRex_total_rx_packets:%s'%str(self.trex_rx_packets))
        print('\n TRex_total dropped packets:%s \n'%str(self.dropped_packets))
        
    def print_real_sd(self):
        print 'real sd is: ' + str(self.real_sd)
    
    def get_trex_tx_packets(self):              
        return self.get_trex_packets('tx')
    
    def get_trex_rx_packets(self):              
        return self.get_trex_packets('rx')
    
    def get_trex_packets(self, direction):
        trex_packets = 0
        direction_str = None
        if direction == 'tx':
            direction_str = 'opackets-*'
        elif direction == 'rx':
            direction_str = 'ipackets-*'
        else:
            raise ValueError('Direction must be either tx or rx')
        
        for trex_result in self.trex_result_list:
            tx_ptks_dict = trex_result.get_last_value('trex-global.data', direction_str)
            for k in sorted(tx_ptks_dict.keys()):
                trex_packets += int(tx_ptks_dict[k])               
        return trex_packets
#=============================================================================== 
def get_ezbox_stats(ezbox):
    ezbox.get_ipvs_stats();
    stats_results = ezbox.read_stats_on_syslog()
    return stats_results
#===============================================================================
class TrexIpToMacMapper:
    def __init__(self,
                 trex_hosts):
        self.trex_subnet_to_mac_pairs = []
        self.get_trex_subnet_to_mac_pairs(trex_hosts)
    
    def get_trex_subnet_to_mac_pairs(self, trex_hosts):
        for trex in trex_hosts:
            self.trex_subnet_to_mac_pairs.append(self.get_servers_subnet_and_mac_pair(trex))
            self.trex_subnet_to_mac_pairs.append(self.get_clients_subnet_and_mac_pair(trex))
            if trex.params.is_dual_port:
                self.trex_subnet_to_mac_pairs.append(self.get_dual_servers_subnet_and_mac_pair(trex))
                self.trex_subnet_to_mac_pairs.append(self.get_dual_clients_subnet_and_mac_pair(trex))

    def get_servers_subnet_and_mac_pair(self, trex):
        servers_first_pair = trex.params.get_server_list_first_pair()
        server_mac_0 = trex.trex_info['port0_server_mac']
        return (servers_first_pair, server_mac_0)
        
    def get_clients_subnet_and_mac_pair(self, trex):
        clients_first_pair = trex.params.get_client_list_first_pair()
        client_mac_0 = trex.trex_info['port0_client_mac']
        return (clients_first_pair, client_mac_0)
        
    def get_dual_servers_subnet_and_mac_pair(self, trex):
        servers_second_pair = trex.params.get_server_list_second_pair()
        server_mac_1 = trex.trex_info['port1_server_mac']
        return (servers_second_pair, server_mac_1) 
        
    def get_dual_clients_subnet_and_mac_pair(self, trex):
        clients_second_pair = trex.params.get_client_list_second_pair()
        client_mac_1 = trex.trex_info['port1_client_mac']
        return (clients_second_pair, client_mac_1) 
        
    def get_mac_for_ip(self, ip_address):
        matching_mac = None
        for ip_list, mac in self.trex_subnet_to_mac_pairs:
            if ip_address in ip_list:
                matching_mac = mac
                break                    
        if matching_mac is None:
            err_msg =  "failed in matching mac address for ip:%s" %(ip_address)
            raise RuntimeError(err_msg)              
        return matching_mac

#===============================================================================    
class TrexPlayers(object):
    def __init__(self):
        self.setup_num = None
        self.trex_hosts = []
        self.trex_ip_to_mac_mapper = None
        self.trex_client_list = None
        self.trex_server_list = None
        
    def get_players(self, setup_num, client_count, server_count, trex_hosts_count = 1):
        self.setup_num = setup_num
        self.get_trex_hosts(trex_hosts_count, client_count, server_count)
        self.trex_ip_to_mac_mapper = TrexIpToMacMapper(self.trex_hosts)
        self.trex_client_list = self.get_client_list()
        self.trex_server_list = self.get_server_list()
        
    def get_trex_hosts(self, trex_hosts_count, client_count, server_count):        
        clients_per_host = client_count/trex_hosts_count
        servers_per_host = server_count/trex_hosts_count       
        for trex_index in range(0,trex_hosts_count):
            self.trex_hosts.append(TrexHost(trex_index, clients_per_host, servers_per_host))
                
    def get_client_list(self):
        client_list = []
        for trex in self.trex_hosts:
            client_list += trex.params.get_client_list()
        return client_list
               
    def get_server_list(self):
        server_list = []
        for trex in self.trex_hosts:
            server_list += trex.params.get_server_list()
        return server_list
            
    def clean_players(self, use_director = False, stop_ezbox = False):
        for trex in self.trex_hosts:
            trex.logout()
            trex.params.remove_trex_yaml_from_localhost()
