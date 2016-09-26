#!/usr/bin/env python


#===============================================================================
# imports
#===============================================================================

# system  
import cmd
from collections import namedtuple
import logging
import os
import sys
import inspect
from multiprocessing import Process

# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir) 
from e2e_infra import *


#===============================================================================
# Test Globals
#===============================================================================
request_count = 1000
server_count = 20
client_count = 1
service_count = 5



#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

def user_init(setup_num):
    print "FUNCTION " + sys._getframe().f_code.co_name + " called"
    
    dict = generic_init(setup_num, service_count, server_count, client_count)
    
    w = 1
    for i,s in enumerate(dict['server_list']):
        s.vip = dict['vip_list'][i%service_count]
        s.weight = w
        if (i+1)%service_count == 0 :
            w *= 2
        
    return convert_generic_init_to_user_format(dict)

def client_execution(client, vip, index):
    new_log = client.logfile_name + '_%d'%index
    client.add_log(new_log)
    client.exec_params += " -i %s -r %d" %(vip, request_count)
    client.execute()
    
def create_client_log_list(client):
    logfile_name = client.logfile_name
    client.loglist.pop()    
    for i in range(service_count):
        new_log = logfile_name + '_%d'%i
        client.add_log(new_log)

def run_user_test(server_list, ezbox, client_list, vip_list):
    print "FUNCTION " + sys._getframe().f_code.co_name + " called"
    process_list = []
    port = '80'
    client = client_list[0]
    
    for i in range(service_count):
        ezbox.add_service(vip_list[i], port, sched_alg_opt='-b sh-port')
    for server in server_list:
        ezbox.add_server(server.vip, port, server.ip, port,server.weight)
    
    for i in range(service_count):
        process_list.append(Process(target=client_execution, args=(client,vip_list[i],i,)))
            
    for p in process_list:
        p.start()
    for p in process_list:
        p.join()
    
    create_client_log_list(client)
    
    print 'End user test'

def create_server_list_per_service(server_list):
    server_list_per_service = []
    for i in range(service_count):
        servers_per_current_service = server_list[i::service_count]
        server_list_per_service.append(servers_per_current_service)
    
    return server_list_per_service

def run_user_checker(server_list, ezbox, client_list, log_dir,vip_list):
    print "FUNCTION " + sys._getframe().f_code.co_name + " called"
    sd = 0.035
    server_count_per_client = server_count/service_count
    steps_count = service_count
    expected_dict = {}
    server_list_per_service = create_server_list_per_service(server_list)
    
    for i in range(service_count):
        expected_dict[i] = {'client_response_count':request_count,
                            'client_count': client_count,
                            'server_count_per_client':server_count_per_client,
                            'no_connection_closed': True,
                            'no_404': True,
                            'check_distribution':(server_list_per_service[i],vip_list,sd),
                            'expected_servers':server_list_per_service[i]}
    
    return client_checker(log_dir, expected_dict, steps_count)

#===============================================================================
# main function
#===============================================================================
def main():
    print "FUNCTION " + sys._getframe().f_code.co_name + " called"
    
    config = generic_main()
    
    server_list, ezbox, client_list, vip_list = user_init(config['setup_num'])
    
    init_players(server_list, ezbox, client_list, vip_list, config)
    
    run_user_test(server_list, ezbox, client_list, vip_list)
    
    log_dir = collect_logs(server_list, ezbox, client_list)

    gen_rc = general_checker(server_list, ezbox, client_list)
    
    clean_players(server_list, ezbox, client_list, True, config['stop_ezbox'])
    
    user_rc = run_user_checker(server_list, ezbox, client_list, log_dir,vip_list)
    
    if user_rc and gen_rc:
        print 'Test passed !!!'
        exit(0)
    else:
        print 'Test failed !!!'
        exit(1)

main()
