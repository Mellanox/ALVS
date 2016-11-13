#!/usr/bin/env python
import os
import sys
import thread
import time
from pexpect import pxssh
import pexpect

sys.path.append('/auto/nps_release/EZdk/EZdk-2.0a-patch-1.0.0/tools/EZcpPyLib/lib')
from ezpy_cp import EZpyCP

host1_ip = '10.7.103.30'
host2_ip = '10.7.101.92'
host3_ip = '10.7.101.193'

host_ip = host2_ip

app_name = "alvs_daemon"
alvs_db = "alvs.db"

def kill_cp_app(ssh_object):
    ssh_object.sendline("ps -ef | grep " + app_name)
    ssh_object.prompt()
    pid = ssh_object.before.split('\n')[1].split()[1]
    print "pid is " + pid
    ssh_object.sendline("kill " + pid)
    ssh_object.prompt()

def flush(ssh_object):
    ssh_object.sendline("ipvsadm -C")
    ssh_object.prompt()
    time.sleep(3)
    
def get_services(ssh_object):
    ssh_object.sendline("echo -e \"select * from services;\" | sqlite3 /" + alvs_db)
    ssh_object.prompt()
    return map(lambda str: str.split('|'), ssh_object.before.split('\n')[1:-1])

def get_servers(ssh_object):
    ssh_object.sendline("echo -e \"select * from servers;\" | sqlite3 /" + alvs_db)
    ssh_object.prompt()
    return map(lambda str: str.split('|'), ssh_object.before.split('\n')[1:-1])

def listen_forever(name, ssh_object):
    while (True):
        if (ssh_object.prompt() == True):
            print ssh_object.before
            print name + ': Prompt recieved'
        print ssh_object.before

#os.system("/.autodirect/LIT/SCRIPTS/rreboot ezbox24")
#time.sleep(60)

print "Clear SysLog & start"
syslog_ssh = pxssh.pxssh()
syslog_ssh.force_password=True
syslog_ssh.login(host_ip, 'root', 'ezchip')
syslog_ssh.sendline("> /var/log/syslog")
syslog_ssh.prompt()
syslog_ssh.sendline("./syslog.sh")

print "Open CP application prompt"
cp_app_ssh = pxssh.pxssh()
cp_app_ssh.login(host_ip, 'root', 'ezchip')

print "Clear ipvs"
cp_app_ssh.sendline("ipvsadm -C")
cp_app_ssh.prompt()

print "Setup NL interface..."
cp_app_ssh.sendline("./nl_if_setup.sh")
cp_app_ssh.prompt()

print "Reset chip..."
cp_app_ssh.sendline("bsp_nps_power -r por")
cp_app_ssh.prompt()

print "Run CP application (with AGT)"
cp_app_ssh.sendline("rm /tmp/" + app_name)
cp_app_ssh.prompt()
os.system("sshpass -p ezchip scp /swgwork/roee/ALVS/bin/" + app_name + " root@" + host_ip + ":/tmp/ ")
cp_app_ssh.sendline("/tmp/" + app_name + " --agt_enabled")
cp_app_ssh.prompt()

print 'Waiting for NPS initialization to complete...'
while (True):
    index = syslog_ssh.expect_exact(['alvs_db_manager_poll...','Shut down ALVS daemon',pexpect.EOF, pexpect.TIMEOUT])
    if index == 0:
        print 'Completed...'
        print syslog_ssh.before + syslog_ssh.match + syslog_ssh.after
        break
    elif index == 1:
        print 'Shut down...'
        print syslog_ssh.before + syslog_ssh.match
        cp_app_ssh.sendline("cat /var/log/alvs_ezcp_log")
        cp_app_ssh.prompt()
        print cp_app_ssh.before
        exit(1)
        break
    elif index == 2:
        print 'EOF...'
        print syslog_ssh.before + syslog_ssh.match
        exit(1)
        break
    elif index == 3:
        print 'TIMEOUT...'
        print syslog_ssh.before

#print 'Launcing listener for syslog...'    
#thread.start_new_thread(listen_forever, ("Syslog", syslog_ssh))

print 'Connect to AGT...'   
cpe = EZpyCP(host_ip, 1234)

print 'Start scenarios...'

print 'Scenario #1 (add service):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd service...'
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:80 -s sh")
cp_app_ssh.prompt()
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 1):
    print '\tTest failed - number of services not correct'
    
if (len(servers) != 0):
    print '\tTest failed - number of servers not correct'

if (hex(int(services[0][0]) + 2**32) != "0xc865070a"
    or hex(int(services[0][1])) != '0x5000'
    or hex(int(services[0][2])) != '0x600'
    or hex(int(services[0][4])) != '0x0'
    or hex(int(services[0][5])) != '0x5'):
    print '\tTest failed - service not correct'

num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 1):
    print '\tTest failed - number of services in NPS not correct'

print 'Scenario #2 (add same service twice):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd services...'
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:80 -s sh")
cp_app_ssh.prompt()
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 1):
    print '\tTest failed - number of services not correct'
    
if (len(servers) != 0):
    print '\tTest failed - number of servers not correct'
    
if (hex(int(services[0][0]) + 2**32) != "0xc865070a"
    or hex(int(services[0][1])) != '0x5000'
    or hex(int(services[0][2])) != '0x600'
    or hex(int(services[0][4])) != '0x0'
    or hex(int(services[0][5])) != '0x5'):
    print '\tTest failed - service not correct'
    
num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 1):
    print '\tTest failed - number of services in NPS not correct'


print 'Scenario #3 (sched algorithm not supported):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd service...'
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.203:80 -s wlc")
cp_app_ssh.prompt()
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 0):
    print '\tTest failed - number of services not correct'
    
if (len(servers) != 0):
    print '\tTest failed - number of servers not correct'

num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 0):
    print '\tTest failed - number of services in NPS not correct'


print 'Scenario #4 (protocol not supported):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd service...'
cp_app_ssh.sendline("ipvsadm -A -u 10.7.101.203:80 -s sh")
cp_app_ssh.prompt()
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 0):
    print '\tTest failed - number of services not correct'
    
if (len(servers) != 0):
    print '\tTest failed - number of servers not correct'

num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 0):
    print '\tTest failed - number of services in NPS not correct'


print 'Scenario #5 (add 5 different services):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd services...'
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:90 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.201:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.202:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.203:80 -s sh")
cp_app_ssh.prompt()
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 5):
    print '\tTest failed - number of services not correct'
    
if (len(servers) != 0):
    print '\tTest failed - number of servers not correct'
    
if (hex(int(services[0][0]) + 2**32) != "0xc865070a"
    or hex(int(services[0][1])) != '0x5000'
    or hex(int(services[0][2])) != '0x600'
    or hex(int(services[0][4])) != '0x0'
    or hex(int(services[0][5])) != '0x5'):
    print '\tTest failed - service[0] not correct'
if (hex(int(services[1][0]) + 2**32) != "0xc865070a"
    or hex(int(services[1][1])) != '0x5a00'
    or hex(int(services[1][2])) != '0x600'
    or hex(int(services[1][4])) != '0x0'
    or hex(int(services[1][5])) != '0x5'):
    print '\tTest failed - service[1] not correct'
if (hex(int(services[2][0]) + 2**32) != "0xc965070a"
    or hex(int(services[2][1])) != '0x5000'
    or hex(int(services[2][2])) != '0x600'
    or hex(int(services[2][4])) != '0x0'
    or hex(int(services[2][5])) != '0x5'):
    print '\tTest failed - service[2] not correct'
if (hex(int(services[3][0]) + 2**32) != "0xca65070a"
    or hex(int(services[3][1])) != '0x5000'
    or hex(int(services[3][2])) != '0x600'
    or hex(int(services[3][4])) != '0x0'
    or hex(int(services[3][5])) != '0x5'):
    print '\tTest failed - service[3] not correct'
if (hex(int(services[4][0]) + 2**32) != "0xcb65070a"
    or hex(int(services[4][1])) != '0x5000'
    or hex(int(services[4][2])) != '0x600'
    or hex(int(services[4][4])) != '0x0'
    or hex(int(services[4][5])) != '0x5'):
    print '\tTest failed - service[4] not correct'
    
num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 5):
    print '\tTest failed - number of services in NPS not correct'


print 'Scenario #6 (add 256 services):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd services...'
for port in range(1000,1256):
    cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:"+ str(port) +" -s sh")
    cp_app_ssh.prompt()
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 256):
    print '\tTest failed - number of services not correct'
    
if (len(servers) != 0):
    print '\tTest failed - number of servers not correct'

for srv in range(256):
    if (hex(int(services[srv][0]) + 2**32) != "0xc865070a"
        #or hex(int(services[srv][1])) != '0x5000'
        or hex(int(services[srv][2])) != '0x600'
        or hex(int(services[srv][4])) != '0x0'
        or hex(int(services[srv][5])) != '0x5'):
        print '\tTest failed - service['+ str(srv) +'] not correct'
    
num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 256):
    print '\tTest failed - number of services in NPS not correct'



print 'Scenario #7 (add 257 services):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd services...'
for port in range(1000,1257):
    cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:"+ str(port) +" -s sh")
    cp_app_ssh.prompt()
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 256):
    print '\tTest failed - number of services not correct'
    
if (len(servers) != 0):
    print '\tTest failed - number of servers not correct'

for srv in range(256):
    if (hex(int(services[srv][0]) + 2**32) != "0xc865070a"
        #or hex(int(services[srv][1])) != '0x5000'
        or hex(int(services[srv][2])) != '0x600'
        or hex(int(services[srv][4])) != '0x0'
        or hex(int(services[srv][5])) != '0x5'):
        print '\tTest failed - service['+ str(srv) +'] not correct'
    
num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 256):
    print '\tTest failed - number of services in NPS not correct'


print 'Scenario #8 (delete service):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd service and delete it...'
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -D -t 10.7.101.200:80")
cp_app_ssh.prompt()
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 0):
    print '\tTest failed - number of services not correct'
    
if (len(servers) != 0):
    print '\tTest failed - number of servers not correct'

num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 0):
    print '\tTest failed - number of services in NPS not correct'


print 'Scenario #9 (delete a service that doesnt exist):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd service and delete it twice...'
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -D -t 10.7.101.200:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -D -t 10.7.101.200:80")
cp_app_ssh.prompt()
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 0):
    print '\tTest failed - number of services not correct'
    
if (len(servers) != 0):
    print '\tTest failed - number of servers not correct'

num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 0):
    print '\tTest failed - number of services in NPS not correct'


   
    
print 'Scenario #11 (modify a service):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd services and modify...'
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:81 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -E -t 10.7.101.200:80 -s sh -b sh-fallback")
cp_app_ssh.prompt()
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 2):
    print '\tTest failed - number of services not correct'
    exit(1)
    print services
    
if (len(servers) != 0):
    print '\tTest failed - number of servers not correct'

if (hex(int(services[0][0]) + 2**32) != "0xc865070a"
    or hex(int(services[0][1])) != '0x5000'
    or hex(int(services[0][2])) != '0x600'
    #or hex(int(services[0][4])) != '0x0'
    or hex(int(services[0][5])) != '0x5'):
    print '\tTest failed - service[0] not correct'
if (hex(int(services[1][0]) + 2**32) != "0xc865070a"
    or hex(int(services[1][1])) != '0x5100'
    or hex(int(services[1][2])) != '0x600'
    or hex(int(services[1][4])) != '0x0'
    or hex(int(services[1][5])) != '0x5'):
    print '\tTest failed - service[1] not correct'

num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 2):
    print '\tTest failed - number of services in NPS not correct'


print 'Scenario #12 (modify a service that doesnt exist):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd services and modify...'
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:81 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -D -t 10.7.101.200:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:82 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -E -t 10.7.101.200:80 -s sh -b sh-fallback")
cp_app_ssh.prompt()
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 2):
    print '\tTest failed - number of services not correct'
    
if (len(servers) != 0):
    print '\tTest failed - number of servers not correct'

if (hex(int(services[0][0]) + 2**32) != "0xc865070a"
    or hex(int(services[0][1])) != '0x5100'
    or hex(int(services[0][2])) != '0x600'
    or hex(int(services[0][4])) != '0x0'
    or hex(int(services[0][5])) != '0x5'):
    print '\tTest failed - service[0] not correct'
if (hex(int(services[1][0]) + 2**32) != "0xc865070a"
    or hex(int(services[1][1])) != '0x5200'
    or hex(int(services[1][2])) != '0x600'
    or hex(int(services[1][4])) != '0x0'
    or hex(int(services[1][5])) != '0x5'):
    print '\tTest failed - service[1] not correct'

num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 2):
    print '\tTest failed - number of services in NPS not correct'



print 'Scenario #13 (add a server to a service that doesnt exist):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd service, delete it and add server...'
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -D -t 10.7.101.200:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.200:80 -r 192.168.0.1:80")
cp_app_ssh.prompt()
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 0):
    print '\tTest failed - number of services not correct'
    
if (len(servers) != 0):
    print '\tTest failed - number of servers not correct'

num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 0):
    print '\tTest failed - number of services in NPS not correct'



print 'Scenario #14 (add a server to a service):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd services and a server...'
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:81 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.200:80 -r 192.168.0.1:80")
cp_app_ssh.prompt()
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 2):
    print '\tTest failed - number of services not correct'
    
if (hex(int(services[0][0]) + 2**32) != "0xc865070a"
    or hex(int(services[0][1])) != '0x5000'
    or hex(int(services[0][2])) != '0x600'
    or hex(int(services[0][4])) != '0x0'
    or hex(int(services[0][5])) != '0x5'):
    print '\tTest failed - service[0] not correct'
if (hex(int(services[1][0]) + 2**32) != "0xc865070a"
    or hex(int(services[1][1])) != '0x5100'
    or hex(int(services[1][2])) != '0x600'
    or hex(int(services[1][4])) != '0x0'
    or hex(int(services[1][5])) != '0x5'):
    print '\tTest failed - service[1] not correct'
    
if (len(servers) != 1):
    print '\tTest failed - number of servers not correct'

num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 2):
    print '\tTest failed - number of services in NPS not correct'


print 'Scenario #15 (add a server to a service twice):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd services and a server...'
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:81 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.200:80 -r 192.168.0.1:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:82 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.200:80 -r 192.168.0.1:80")
cp_app_ssh.prompt()
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 3):
    print '\tTest failed - number of services not correct'
    
if (hex(int(services[0][0]) + 2**32) != "0xc865070a"
    or hex(int(services[0][1])) != '0x5000'
    or hex(int(services[0][2])) != '0x600'
    or hex(int(services[0][4])) != '0x0'
    or hex(int(services[0][5])) != '0x5'):
    print '\tTest failed - service[0] not correct'
if (hex(int(services[1][0]) + 2**32) != "0xc865070a"
    or hex(int(services[1][1])) != '0x5100'
    or hex(int(services[1][2])) != '0x600'
    or hex(int(services[1][4])) != '0x0'
    or hex(int(services[1][5])) != '0x5'):
    print '\tTest failed - service[1] not correct'
if (hex(int(services[2][0]) + 2**32) != "0xc865070a"
    or hex(int(services[2][1])) != '0x5200'
    or hex(int(services[2][2])) != '0x600'
    or hex(int(services[2][4])) != '0x0'
    or hex(int(services[2][5])) != '0x5'):
    print '\tTest failed - service[2] not correct'
    
if (len(servers) != 1):
    print '\tTest failed - number of servers not correct'

num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 3):
    print '\tTest failed - number of services in NPS not correct'



print 'Scenario #16 (Routing algorithm not supported):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd services and a server...'
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.200:80 -r 192.168.0.1:80 -m")
cp_app_ssh.prompt()
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 1):
    print '\tTest failed - number of services not correct'
    
if (hex(int(services[0][0]) + 2**32) != "0xc865070a"
    or hex(int(services[0][1])) != '0x5000'
    or hex(int(services[0][2])) != '0x600'
    or hex(int(services[0][4])) != '0x0'
    or hex(int(services[0][5])) != '0x5'):
    print '\tTest failed - service[0] not correct'
    
if (len(servers) != 0):
    print '\tTest failed - number of servers not correct '

num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 1):
    print '\tTest failed - number of services in NPS not correct'



print 'Scenario #17 (Add 5 different servers):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd services and a server...'
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.201:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.200:80 -r 192.168.0.1:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.201:80 -r 192.168.0.1:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.200:80 -r 192.168.0.2:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.201:80 -r 192.168.0.2:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.201:80 -r 192.168.0.3:80")
cp_app_ssh.prompt()
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 2):
    print '\tTest failed - number of services not correct'
    
if (hex(int(services[0][0]) + 2**32) != "0xc865070a"
    or hex(int(services[0][1])) != '0x5000'
    or hex(int(services[0][2])) != '0x600'
    or hex(int(services[0][4])) != '0x0'
    or hex(int(services[0][5])) != '0x5'):
    print '\tTest failed - service[0] not correct'
if (hex(int(services[1][0]) + 2**32) != "0xc965070a"
    or hex(int(services[1][1])) != '0x5000'
    or hex(int(services[1][2])) != '0x600'
    or hex(int(services[1][4])) != '0x0'
    or hex(int(services[1][5])) != '0x5'):
    print '\tTest failed - service[1] not correct'
    
if (len(servers) != 5):
    print '\tTest failed - number of servers not correct'

num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 2):
    print '\tTest failed - number of services in NPS not correct'



print 'Scenario #20 (Delete a server):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd service and server and delete the server...'
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.200:80 -r 192.168.0.1:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -d -t 10.7.101.200:80 -r 192.168.0.1:80")
cp_app_ssh.prompt()
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 1):
    print '\tTest failed - number of services not correct'
    
if (hex(int(services[0][0]) + 2**32) != "0xc865070a"
    or hex(int(services[0][1])) != '0x5000'
    or hex(int(services[0][2])) != '0x600'
    or hex(int(services[0][4])) != '0x0'
    or hex(int(services[0][5])) != '0x5'):
    print '\tTest failed - service[0] not correct'
    
if (len(servers) != 0):
    print '\tTest failed - number of servers not correct'

num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 1):
    print '\tTest failed - number of services in NPS not correct'



print 'Scenario #21 (Delete a server that doesnt exist):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd service and server and delete another server...'
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.200:80 -r 192.168.0.1:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -d -t 10.7.101.200:80 -r 192.168.0.1:81")
cp_app_ssh.prompt()
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 1):
    print '\tTest failed - number of services not correct'
    
if (hex(int(services[0][0]) + 2**32) != "0xc865070a"
    or hex(int(services[0][1])) != '0x5000'
    or hex(int(services[0][2])) != '0x600'
    or hex(int(services[0][4])) != '0x0'
    or hex(int(services[0][5])) != '0x5'):
    print '\tTest failed - service[0] not correct'
    
if (len(servers) != 1):
    print '\tTest failed - number of servers not correct'

num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 1):
    print '\tTest failed - number of services in NPS not correct'



print 'Scenario #23 (Modify a server):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd services and a server...'
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.200:80 -r 192.168.0.1:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -e -t 10.7.101.201:80 -r 192.168.0.1:80 -w 2")
cp_app_ssh.prompt()
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 1):
    print '\tTest failed - number of services not correct'
    
if (hex(int(services[0][0]) + 2**32) != "0xc865070a"
    or hex(int(services[0][1])) != '0x5000'
    or hex(int(services[0][2])) != '0x600'
    or hex(int(services[0][4])) != '0x0'
    or hex(int(services[0][5])) != '0x5'):
    print '\tTest failed - service[0] not correct'
    
if (len(servers) != 1):
    print '\tTest failed - number of servers not correct'

num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 1):
    print '\tTest failed - number of services in NPS not correct'


print 'Scenario #24 (Modify a server that doesnt exist):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd services and a server...'
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.200:80 -r 192.168.0.1:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -e -t 10.7.101.201:80 -r 192.168.0.1:81 -w 2")
cp_app_ssh.prompt()
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 1):
    print '\tTest failed - number of services not correct'
    
if (hex(int(services[0][0]) + 2**32) != "0xc865070a"
    or hex(int(services[0][1])) != '0x5000'
    or hex(int(services[0][2])) != '0x600'
    or hex(int(services[0][4])) != '0x0'
    or hex(int(services[0][5])) != '0x5'):
    print '\tTest failed - service[0] not correct'
    
if (len(servers) != 1):
    print '\tTest failed - number of servers not correct'

num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 1):
    print '\tTest failed - number of services in NPS not correct'


print 'Scenario #25 (flush):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd services and servers and flush...'
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.201:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.200:80 -r 192.168.0.1:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.201:80 -r 192.168.0.1:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.200:80 -r 192.168.0.2:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.201:80 -r 192.168.0.2:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.201:80 -r 192.168.0.3:80")
cp_app_ssh.prompt()
flush(cp_app_ssh)
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 0):
    print '\tTest failed - number of services not correct'
    
if (len(servers) != 0):
    print '\tTest failed - number of servers not correct'

num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 0):
    print '\tTest failed - number of services in NPS not correct'




print 'Scenario #26 (Delete service that has servers and then try adding them again):'
print '\tFlush...'
flush(cp_app_ssh)

print '\tAdd services and servers and delete (3 times)...'
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.200:80 -r 192.168.0.1:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.200:80 -r 192.168.0.2:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -D -t 10.7.101.200:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.200:80 -r 192.168.0.1:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.200:80 -r 192.168.0.2:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -D -t 10.7.101.200:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -A -t 10.7.101.200:80 -s sh")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.200:80 -r 192.168.0.1:80")
cp_app_ssh.prompt()
cp_app_ssh.sendline("ipvsadm -a -t 10.7.101.200:80 -r 192.168.0.2:80")
cp_app_ssh.prompt()
time.sleep(1)

services = get_services(cp_app_ssh)
servers = get_servers(cp_app_ssh)

if (len(services) != 1):
    print '\tTest failed - number of services not correct'
    
if (hex(int(services[0][0]) + 2**32) != "0xc865070a"
    or hex(int(services[0][1])) != '0x5000'
    or hex(int(services[0][2])) != '0x600'
    or hex(int(services[0][4])) != '0x0'
    or hex(int(services[0][5])) != '0x5'):
    print '\tTest failed - service[0] not correct'
    
if (len(servers) != 2):
    print '\tTest failed - number of servers not correct'

num_entries = (cpe.cp.struct.get_num_entries(struct_id = 4, channel_id = 0)).result['num_entries']['number_of_entries']
if (num_entries != 1):
    print '\tTest failed - number of services in NPS not correct'


print "Killing CP application" 
kill_cp_app(cp_app_ssh)