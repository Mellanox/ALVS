#!/usr/bin/env python

import os
import sys
import pexpect
from __builtin__ import len
from random import randint
import socket
'''
    Global variables
'''
host =   '10.7.103.30'
fname = 'temp_file'                                                                                                                                                  
cp_arp_dict = {}

'''
    Connect to CP AGT
'''
sys.path.append("./EZdk/tools/EZcpPyLib/lib")
from ezpy_cp import EZpyCP
cpe=EZpyCP(host,"1234")

def debug_print(x):
    #print x
    pass

'''
    SSH command to host
'''
def ssh(cmd, to_print = False):
    fout = open(fname, 'w')                                                                                                                                                    
    options = '' #'-q -oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -oPubkeyAuthentication=no'                                                                         
    user = 'root'
    password = 'ezchip'
    if to_print:
        print 'Perform SSH command: ' + cmd
    ssh_cmd = 'ssh %s@%s %s "%s"' % (user, host, options, cmd)                                                                                                                 
    child = pexpect.spawn(ssh_cmd, timeout=20)                                                                                                                            
    child.expect(['password: '])                                                                                                                                                                                                                                                                                               
    child.sendline(password)                                                                                                                                                   
    child.logfile = fout                                                                                                                                                       
    child.expect(pexpect.EOF)                                                                                                                                                  
    child.close()                                                                                                                                                              
    fout.close()                                                                                                                                                               
    
    fin = open(fname, 'r')                                                                                                                                                     
    stdout = fin.read()                                                                                                                                                        
    fin.close()                                                                                                                                                                
    if 0 != child.exitstatus:   
        if stdout.find("No ARP entry for") == -1:                                                                                                                                       
            raise Exception(stdout)                                                                                                                                                
    
'''
    Test ARP table read from host Linux
'''
def test_arp_table():
    ssh('arp -n')
    read_cp_arp_table()

    debug_print('ARP table from Linux:')
    linux_arp_dict = {}
    fout = open(fname, 'r')
    for line in fout:
        if len(line.strip()) > 0:
            split_line = line.strip().split()
            if split_line[0] != 'Address' and split_line[1] != '(incomplete)':
                net_addr = split_line[0].strip()
                mac_addr = split_line[2].strip().upper()
                debug_print( "net address: %s   , mac address: %s" %(net_addr, mac_addr))
                linux_arp_dict[net_addr] = mac_addr
                if net_addr not in cp_arp_dict:
                    print '*** ERROR: entry %s %s not in CP ARP table' %(net_addr , linux_arp_dict[net_addr])
                    exit(1)
                else:
                    if mac_addr != cp_arp_dict[net_addr]:
                        print '*** ERROR: entry in linux table not equal to CP ARP table. %s != %s' %(mac_addr , cp_arp_dict[net_addr])
                        exit(1)
                        
    for key in cp_arp_dict:
        if key not in linux_arp_dict:
            print '*** ERROR: entry %s %s not in Linux ARP table' %(key,cp_arp_dict[key])
            exit(1)
        else:
            if cp_arp_dict[key] != linux_arp_dict[key]:
                print '*** ERROR: entry not equal in Linux and CP ARP tables. %s != %s' %(cp_arp_dict[key]  , linux_arp_dict[key])
                exit(1)
    
    debug_print("ARP table test OK")


'''
    Read ARP table from CP using AGT
'''
def read_cp_arp_table():
    struct_id = 6
    global cp_arp_dict
    cp_arp_dict = {}
    iterator_params_dict = { 'channel_id': 0 }
    iterator_params_dict = (cpe.cp.struct.iterator_create( struct_id , iterator_params_dict )).result['iterator_params']
    num_entries = (cpe.cp.struct.get_num_entries( struct_id = struct_id , channel_id = 0 )).result['num_entries']['number_of_entries']
    debug_print( "Read ARP table:")
    
    for i in range(0,num_entries):
        iterator_params_dict = cpe.cp.struct.iterator_get_next( struct_id, iterator_params_dict ).result['iterator_params'] 
        key = iterator_params_dict['entry']['key']
        result = iterator_params_dict['entry']['result']
        int_key =  socket.ntohl(int(key.replace(" ",""),16))
        str_key = "%08X"%int_key
        parsed_key=(str_key[0:2],str_key[2:4],str_key[4:6],str_key[6:8])
        key_str = '%d.%d.%d.%d'%tuple([int(k, 16) for k in parsed_key])
        result_str = (result.strip()[6:]).replace(" ", ":")
        cp_arp_dict[key_str] = result_str 
        debug_print( "%d - Key: %s   , Result: %s" %(i, key_str, result_str))
    cpe.cp.struct.iterator_delete( struct_id , iterator_params_dict )
    
'''
    Read classification table from CP using AGT
'''
def read_classification_table():
    struct_id = 3
    iterator_params_dict = { 'channel_id': 0 }
    iterator_params_dict = (cpe.cp.struct.iterator_create( struct_id , iterator_params_dict )).result['iterator_params']
    num_entries = (cpe.cp.struct.get_num_entries( struct_id = struct_id , channel_id = 0 )).result['num_entries']['number_of_entries']
    print "Read classification table:"
    for i in range(0,num_entries):
        iterator_params_dict = cpe.cp.struct.iterator_get_next( struct_id, iterator_params_dict ).result['iterator_params'] 
        key = iterator_params_dict['entry']['key']
        result = iterator_params_dict['entry']['result']
        print "Key: " + key
        print "Result: " + result
    
    cpe.cp.struct.iterator_delete( struct_id , iterator_params_dict )

print "========================"
print "    ARP Table Test"
print "========================"
test_arp_table()
for i in range(100):
    rand_ip = randint(1,9)
    rand_cmd = randint(0,1)
    if rand_cmd:
        ssh('arp -s 10.7.103.20%d 00:10:54:CA:E1:4%s'%(rand_ip,rand_ip),True)
    else:
        ssh('arp -d 10.7.103.20%d'%rand_ip,True)
    if i%5 == 0:
        test_arp_table()

test_arp_table()
for i in range(10):
    ssh('arp -d 10.7.103.20%d'%i)

print "========================"
print "*** ARP test PASSED ***"
print "========================"
print ""
read_classification_table()
