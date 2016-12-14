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
from alvs_tester_class import ALVS_Tester

# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir) 
from alvs_infra import *


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

class Test25(ALVS_Tester):

    def user_init(self, setup_num):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        
        self.test_resources = generic_init(setup_num, service_count, server_count, client_count)
        
        w = 1
        for i,s in enumerate(self.test_resources['server_list']):
            s.vip = self.test_resources['vip_list'][i%service_count]
            s.weight = w
            if (i+1)%service_count == 0 :
                w *= 2
    
    def client_execution(self, client, vip, index):
        new_log = client.logfile_name + '_%d'%index
        client.add_log(new_log)
        client.exec_params += " -i %s -r %d" %(vip, request_count)
        client.execute()
        
    def create_client_log_list(self, client):
        logfile_name = client.logfile_name
        client.loglist.pop()    
        for i in range(service_count):
            new_log = logfile_name + '_%d'%i
            client.add_log(new_log)
    
    def run_user_test(self):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        process_list = []
        port = '80'
        client = self.test_resources['client_list'][0]
        
        for i in range(service_count):
            self.test_resources['ezbox'].add_service(self.test_resources['vip_list'][i], port, sched_alg_opt='-b sh-port')
        for server in self.test_resources['server_list']:
            self.test_resources['ezbox'].add_server(server.vip, port, server.ip, port,server.weight)
        
        for i in range(service_count):
            process_list.append(Process(target=self.client_execution, args=(client,self.test_resources['vip_list'][i],i,)))
                
        for p in process_list:
            p.start()
        for p in process_list:
            p.join()
        
        self.create_client_log_list(client)
        
        print 'End user test'
    
    def create_server_list_per_service(self):
        server_list_per_service = []
        for i in range(service_count):
            servers_per_current_service = self.test_resources['server_list'][i::service_count]
            server_list_per_service.append(servers_per_current_service)
        
        return server_list_per_service
    
    def run_user_checker(self, log_dir):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        sd = 0.035
        server_count_per_client = server_count/service_count
        steps_count = service_count
        expected_dict = {}
        server_list_per_service = self.create_server_list_per_service()
        
        for i in range(service_count):
            expected_dict[i] = {'client_response_count':request_count,
                                'client_count': client_count,
                                'server_count_per_client':server_count_per_client,
                                'no_connection_closed': True,
                                'no_404': True,
                                'check_distribution':(server_list_per_service[i],self.test_resources['vip_list'],sd),
                                'expected_servers':server_list_per_service[i]}
        
        return client_checker(log_dir, expected_dict, steps_count)
    
current_test = Test25()
current_test.main()
