import sys
sys.path.append("verification/testing")
from test_infra import * 
import random
args = read_test_arg(sys.argv)    

log_file = "scheduling_algorithm_test.log"
if 'log_file' in args:
    log_file = args['log_file']
init_logging(log_file)

ezbox = ezbox_host(args['setup_num'])

if args['hard_reset']:
    ezbox.reset_ezbox()
    
    

def check_service_info(expected_service_info, service_info):
    
    if service_info['sched_alg'] != expected_service_info['sched_alg'] :
        print "ERROR, sched_alg  = %d expected = %d\n" % (service_info['sched_alg'], expected_service_inf['sched_alg'])
        exit(1)

    if service_info['server_count'] != expected_service_info['server_count'] :
        print "ERROR, server_count = %d, expected = %d\n" % (service_info['server_count'], expected_service_info['server_count'])
        exit(1)

    if service_info['flags'] != expected_service_info['flags'] :
        print "ERROR, flags  = %d expected = %d\n" % (service_info['flags'], expected_service_inf['flags'])
        exit(1)
                                       
    if len(service_info['sched_info']) != len(expected_service_info['sched_info']) :
        print "ERROR, len(sched_info)  = %d expected = %d\n" % (len(service_info['sched_info']), len(expected_service_info['sched_info']))
        exit(1)
    
    if cmp(service_info['sched_info'], expected_service_info['sched_info']) != 0 :
        print "ERROR, sched_info is not as expected\n"
        exit(1)


# init ALVS daemon
ezbox.connect()
ezbox.flush_ipvs()
ezbox.alvs_service_stop()
ezbox.copy_cp_bin(debug_mode=1)
ezbox.copy_dp_bin(debug_mode=args['debug'])
ezbox.alvs_service_start()
ezbox.wait_for_cp_app()
ezbox.wait_for_dp_app()
ezbox.clean_director()


# each setup can use differen VMs
ip_list = get_setup_list(args['setup_num'])

# each setup can use different the virtual ip 
virtual_ip_address_1 = get_setup_vip(args['setup_num'], 0)
virtual_ip_address_2 = get_setup_vip(args['setup_num'], 1)

# create servers
server1 = real_server(management_ip=ip_list[0]['hostname'], data_ip=ip_list[0]['ip'])
server2 = real_server(management_ip=ip_list[1]['hostname'], data_ip=ip_list[1]['ip'])
server3 = real_server(management_ip=ip_list[2]['hostname'], data_ip=ip_list[2]['ip'])
server4 = real_server(management_ip=ip_list[3]['hostname'], data_ip=ip_list[3]['ip'])
 
# create services on ezbox
print "PART 1 - create first service - START\n"
first_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_1, port='80', schedule_algorithm = 'source_hash')
first_service.add_server(server1, weight='1')
first_service.add_server(server2, weight='1')

# iterator_params_dict = (ezbox.cpe.cp.struct.iterator_create(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION, { 'channel_id': 0 })).result['iterator_params']
# iterator_params_dict = ezbox.cpe.cp.struct.iterator_get_next(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION, iterator_params_dict).result['iterator_params']

    

#iterator_params_dict = (ezbox.cpe.cp.struct.iterator_create(STRUCT_ID_ALVS_SERVICE_INFO, { 'channel_id': 0 })).result['iterator_params']
#iterator_params_dict = ezbox.cpe.cp.struct.iterator_get_next(STRUCT_ID_ALVS_SERVICE_INFO, iterator_params_dict).result['iterator_params']
service_info = ezbox.get_service(ip2int(virtual_ip_address_1), port=80, protocol = 6)
if service_info == None:
    print "ERROR, first service wasn't created as expected\n"
    exit(1)
    
expected_sched_info = []
while len(expected_sched_info) < 256 :
    expected_sched_info += [0]
    expected_sched_info += [1]
    
expected_service_info = {'sched_alg' : 5,
                         'server_count' : 2,
                         'flags' : 0,
                         'sched_info' : expected_sched_info,
                         'stats_base' : 0}
                         
 
check_service_info(expected_service_info, service_info)    


if ezbox.get_num_of_services() != 1 :
    print "ERROR, number of services should be 1\n"
    exit(1)

print "PART 1 - create first service PASSED\n"
print "PART 2 - create second service START\n"

second_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_2, port='80', schedule_algorithm = 'source_hash')
second_service.add_server(server1, weight=4)
# create services on ezbox
service_info = ezbox.get_service(ip2int(virtual_ip_address_2), port=80, protocol = 6)
if service_info == None:
    print "ERROR, second service wasn't created as expected\n"
    exit(1)

expected_sched_info = []    
while len(expected_sched_info) < 256 :
    expected_sched_info += [2]

expected_service_info = {'sched_alg' : 5,
                         'server_count' : 1,
                         'flags' : 0,
                         'sched_info' : expected_sched_info,
                         'stats_base' : 0}

check_service_info(expected_service_info, service_info)    

if ezbox.get_num_of_services() != 2 :
    print "ERROR, number of services should be 1\n"
    exit(1)

print "PART 2 - create second service PASSED\n"

print "PART 3 - remove second service START\n"

second_service.remove_service();

if ezbox.get_num_of_services() != 1 :
    print "ERROR, number of services should be 1\n"
    exit(1)

service_info = ezbox.get_service(ip2int(virtual_ip_address_2), port=80, protocol = 6)
if service_info != None:
    print "ERROR, second service wasn't removed\n"
    exit(1)
print "PART 3 - remove second service PASSED\n"


print "PART 4 - add second service START\n"
second_service = service(ezbox=ezbox, virtual_ip=virtual_ip_address_2, port='80', schedule_algorithm = 'source_hash')
second_service.add_server(server3, weight=5)
second_service.add_server(server1, weight=2)

service_info = ezbox.get_service(ip2int(virtual_ip_address_2), port=80, protocol = 6)
if service_info == None:
    print "ERROR, second service wasn't created as expected\n"
    exit(1)

expected_sched_info = []    
while len(expected_sched_info) < 256 :
    expected_sched_info += [3]*5
    expected_sched_info += [4]*2
expected_sched_info = expected_sched_info[:256]

expected_service_info = {'sched_alg' : 5,
                         'server_count' : 2,
                         'flags' : 0,
                         'sched_info' : expected_sched_info,
                         'stats_base' : 0}

check_service_info(expected_service_info, service_info)    


if ezbox.get_num_of_services() != 2 :
    print "ERROR, number of services should be 1\n"
    exit(1)
print "PART 4 - add second service PASSED\n"

print "PART 5 - modify server  START\n"
second_service.modify_server(server1, weight=0)

#*************** check first service ********************
service_info = ezbox.get_service(ip2int(virtual_ip_address_1), port=80, protocol = 6)
if service_info == None:
    print "ERROR, second service wasn't created as expected\n"
    exit(1)

expected_sched_info = []
while len(expected_sched_info) < 256 :
    expected_sched_info += [0]
    expected_sched_info += [1]

expected_service_info = {'sched_alg' : 5,
                         'server_count' : 2,
                         'flags' : 0,
                         'sched_info' : expected_sched_info,
                         'stats_base' : 0}

check_service_info(expected_service_info, service_info)

#*************** check first service ********************
service_info = ezbox.get_service(ip2int(virtual_ip_address_2), port=80, protocol = 6)
if service_info == None:
    print "ERROR, second service wasn't created as expected\n"
    exit(1)

expected_sched_info = []
while len(expected_sched_info) < 256 :
    expected_sched_info += [3]

expected_service_info = {'sched_alg' : 5,
                         'server_count' : 1,
                         'flags' : 0,
                         'sched_info' : expected_sched_info,
                         'stats_base' : 0}

check_service_info(expected_service_info, service_info)    
    
print "PART 5 - modify server  PASSED\n"
print "PART 6 - add server to the second service START\n"

second_service.add_server(server4, weight=10)

service_info = ezbox.get_service(ip2int(virtual_ip_address_2), port=80, protocol = 6)
if service_info == None:
    print "ERROR, second service wasn't created as expected\n"
    exit(1)

expected_sched_info = []    
while len(expected_sched_info) < 256 :
    expected_sched_info += [3]*5
    expected_sched_info += [5]*10
expected_sched_info = expected_sched_info[:256]

expected_service_info = {'sched_alg' : 5,
                         'server_count' : 2,
                         'flags' : 0,
                         'sched_info' : expected_sched_info,
                         'stats_base' : 0}

check_service_info(expected_service_info, service_info)    
print "PART 6 - add server to the second service PASSED\n"


print "PART 7 - modify service START\n"
ezbox.modify_service(ip2int(virtual_ip_address_2), port=80,sched_alg='sh', sched_alg_opt='-b dh-port')
service_info = ezbox.get_service(ip2int(virtual_ip_address_2), port=80, protocol = 6)

expected_sched_info = []    
while len(expected_sched_info) < 256 :
    expected_sched_info += [3]*5
    expected_sched_info += [5]*10
expected_sched_info = expected_sched_info[:256]

expected_service_info = {'sched_alg' : 5,
                         'server_count' : 2,
                         'flags' : 0,
                         'sched_info' : expected_sched_info,
                         'stats_base' : 0}

ezbox.modify_service(ip2int(virtual_ip_address_2), port=80, sched_alg='sh', sched_alg_opt='-b sh-port')
service_info = ezbox.get_service(ip2int(virtual_ip_address_2), port=80, protocol = 6)
expected_sched_info = []    
while len(expected_sched_info) < 256 :
    expected_sched_info += [3]*5
    expected_sched_info += [5]*10
expected_sched_info = expected_sched_info[:256]

expected_service_info = {'sched_alg' : 5,
                         'server_count' : 2,
                         'flags' : 16,
                         'sched_info' : expected_sched_info,
                         'stats_base' : 0}
check_service_info(expected_service_info, service_info)    

ezbox.modify_service(ip2int(virtual_ip_address_2), port=80, sched_alg='sh', sched_alg_opt='')
service_info = ezbox.get_service(ip2int(virtual_ip_address_2), port=80, protocol = 6)
expected_sched_info = []    
while len(expected_sched_info) < 256 :
    expected_sched_info += [3]*5
    expected_sched_info += [5]*10
expected_sched_info = expected_sched_info[:256]

expected_service_info = {'sched_alg' : 5,
                         'server_count' : 2,
                         'flags' : 0,
                         'sched_info' : expected_sched_info,
                         'stats_base' : 0}
check_service_info(expected_service_info, service_info)    

print "PART 7 - modify service PASSED\n"
print "PART 8 - CLEAR ALL START\n"
ezbox.flush_ipvs()
if ezbox.get_num_of_services()!= 0:
    print "after FLUSH all should be 0\n"
    exit(1)
print "PART 8 - CLEAR ALL PASSED\n"
 
#*************** check first service ********************

print "\nTest Passed\n"
