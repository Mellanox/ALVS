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
import time
from tester_class import Tester

# pythons modules 
# local
currentdir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
parentdir = os.path.dirname(currentdir)
sys.path.insert(0,parentdir) 
from e2e_infra import *


#===============================================================================
# Test Globals
#===============================================================================
request_count = 500 
server_count = 5
client_count = 1
service_count = 1
service_port='100'


#===============================================================================
# User Area function needed by infrastructure
#===============================================================================

class Test18(Tester):
    
    def user_init(self, setup_num):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        
        self.test_resources = generic_init(setup_num, service_count, server_count, client_count)

            
        for s in self.test_resources['server_list']:
            s.vip = self.test_resources['vip_list'][0]
            #saving original httpd.conf in VM
            os.system("sshpass -p " + s.password + " ssh " + s.username + "@" + s.hostname + " " + "cp /etc/httpd/conf/httpd.conf /etc/httpd/conf/httpd.conf.org")      
            #change httpd to port requested service port
            os.system("sshpass -p " + s.password + " ssh " + s.username + "@" + s.hostname + " " + "\"sed -i '42s/.*/Listen %s/' /etc/httpd/conf/httpd.conf\""%service_port)
            os.system("sshpass -p " + s.password + " ssh " + s.username + "@" + s.hostname + " " + "service httpd restart")
            

    def client_execution(self, client, vip,port):
        client.exec_params += " -i %s -r %d -p %s" %(vip, request_count,port)
        client.execute()
    
    def run_user_test(self):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        process_list = []
        vip = self.test_resources['vip_list'][0]
        port = service_port
        
        self.test_resources['ezbox'].add_service(vip, port, sched_alg_opt='-b sh-port')
        for server in self.test_resources['server_list']:
            self.test_resources['ezbox'].add_server(vip, port, server.ip, port)
            
        for client in self.test_resources['client_list']:
            process_list.append(Process(target=self.client_execution, args=(client,vip,port,)))
        for p in process_list:
            p.start()
        for p in process_list:
            p.join()
        
        #bringing back httpd to original configuration
        for s in self.test_resources['server_list']:
            os.system("sshpass -p " + s.password + " ssh " + s.username + "@" + s.hostname + " " + "cp /etc/httpd/conf/httpd.conf.org /etc/httpd/conf/httpd.conf")
            os.system("sshpass -p " + s.password + " ssh " + s.username + "@" + s.hostname + " " + "service httpd restart")
        
        print 'End user test'
    
    def run_user_checker(self, log_dir):
        print "FUNCTION " + sys._getframe().f_code.co_name + " called"
        servers = [s.ip for s in self.test_resources['server_list']]
        expected_servers = dict((client.ip, servers) for client in self.test_resources['client_list'])
        expected_dict= {'client_response_count':request_count,
                        'client_count': client_count, 
                        'no_404': True,
                        'no_connection_closed':True,
                        'check_distribution':(self.test_resources['server_list'],self.test_resources['vip_list'],0.05),
                        'expected_servers_per_client':expected_servers,
                        'server_count_per_client':server_count}
        
        return client_checker(log_dir, expected_dict)
    
current_test = Test18()
current_test.main()