#!/usr/bin/python
import os
import sys
import getopt

def usage():
    print """usage: alvs_install.py [OPTIONS]
Install ALVS components (default)
Valid Options:
-h, --help         : This help section
"""

#############
# Constants #
#############

ALVS_NPS_IP = '169.254.42.44'
ALVS_NPS_HOST = 'alvs_nps'
ALVS_CONFIG = '/etc/default/alvs'
ALVS_SERVICE = '/etc/init.d/alvs'
ALVS_DAEMON = '/usr/sbin/alvs_daemon'
ALVS_DP = '/usr/lib/alvs/alvs_dp'
ALVS_DP_START = '/usr/share/alvs/start_alvs_dp'
ALVS_RSYSLOG_CONF = '/etc/rsyslog.d/alvs.conf'
ALVS_SYSCTL_CONF = '/etc/sysctl.d/alvs.conf'
ALVS_PACKAGES = ['ipvsadm','libnl-3-200']
ALVS_MODULES = ['nlmon','ip_vs','8021q']
ALVS_FILES = [('cfg/alvs_defaults',ALVS_CONFIG),
              ('scripts/alvs_init',ALVS_SERVICE),
              ('bin/alvs_daemon',ALVS_DAEMON),
              ('bin/alvs_dp',ALVS_DP),
              ('scripts/start_alvs_dp.py',ALVS_DP_START),
              ('cfg/alvs_rsyslog.conf',ALVS_RSYSLOG_CONF),
              ('cfg/alvs_sysctl.conf',ALVS_SYSCTL_CONF)]
ALVS_NETWORK = {'auto nlmon':['iface nlmon inet manual',
                              'pre-up /sbin/ip link add type nlmon',
                              'pre-up /sbin/ip link set nlmon0 up']}

#############
# Utilities #
#############
def run_cmd(cmd):
    rc = os.system(cmd)
    if rc != 0:
        print "ERROR: Failed to execute '%s'" % cmd
        return False
    return True

################
# Installation #
################
def install_packages():
    print "Installing packages...",
    sys.stdout.flush()
    for package in ALVS_PACKAGES:
        if not run_cmd('apt-get -y install %s >> /dev/null 2>&1' % package):
            print "Failed to install %s!" % package
            return False
    print "Done!"
    return True

def add_modules():
    print "Add relevant modules...",
    sys.stdout.flush()
    modules_to_add = list(ALVS_MODULES)
    with open('/etc/modules','r') as modules_file:
        for module in modules_file:
            if module.strip() in modules_to_add:
                modules_to_add.remove(module.strip())
    if len(modules_to_add) == 0:
        print "Done!"
        return True
    with open('/etc/modules','a') as modules_file:
        for module in modules_to_add:
            if not run_cmd('modprobe %s' % module):
                print "Failed to load %s" % module
                return False
            modules_file.write("%s\n" % module)
    print "Done!"
    return True

def add_nps_host():
    print "Add NPS host...",
    sys.stdout.flush()
    with open('/etc/hosts','r') as hosts_file:
        for host in hosts_file:
            if host.strip().startswith(ALVS_NPS_IP):
                print "Done!"
                return True
    with open('/etc/hosts','a') as hosts_file:
        hosts_file.write("%s\t%s\n" % (ALVS_NPS_IP, ALVS_NPS_HOST))
    print "Done!"
    return True

def copy_alvs_files():
    print "Copying ALVS files...",
    sys.stdout.flush()
    for fsrc, fdst in ALVS_FILES:
        if fsrc == 'alvs_defaults' and os.path.isfile(fdst):
            overwite = raw_input("Do you want to overwrite configuration [Y/n]? ")
            if overwite != 'Y':
                continue
        if not os.path.isdir(os.path.dirname(fdst)):
            if not run_cmd("mkdir -p %s" % (os.path.dirname(fdst))):
                return False
        if not run_cmd("cp -f %s* %s" % (fsrc, fdst)):
            return False
    print "Done!"
    return True

#TODO add as proper startup script and not rc.local
def start_alvs_on_boot():
    print "Start ALVS on boot...",
    sys.stdout.flush()
    cmd = "%s start" % ALVS_SERVICE
    with open('/etc/rc.local','r') as rc_file:
        for line in rc_file:
            if line.strip() == cmd:
                print "Done!"
                return True
    with open('/etc/rc.local','r') as rc_file:
        content = rc_file.readlines()
    with open('/etc/rc.local','w') as rc_file:
        for line in content:
            if not line.strip() == 'exit 0':
                rc_file.write(line)
        rc_file.write(cmd)
        rc_file.write('\nexit 0\n')
    print "Done!"
    return True

def add_network_config():
    print "Add ALVS network configurations...",
    sys.stdout.flush()
    network_conf = ALVS_NETWORK.copy()
    with open('/etc/network/interfaces','r') as if_file:
        for line in if_file:
            if line.strip() in ALVS_NETWORK.keys():
                network_conf.pop(line.strip())
    if len(network_conf) == 0:
        print "Done!"
        return True
    with open('/etc/network/interfaces','a') as if_file:
        for if_name, if_config in network_conf.iteritems():
            if_file.write("%s\n" % if_name)
            for c in if_config:
                if_file.write("%s\n" % c)
    print "Done!"
    return True

def reload_configuration():
    print "Reload ALVS configurations...",
    if not run_cmd("systemctl daemon-reload > /dev/null 2>&1"):
        return False
    if not run_cmd("service rsyslog restart > /dev/null 2>&1"):
        return False
    if not run_cmd("sysctl -p %s > /dev/null 2>&1" % ALVS_SYSCTL_CONF):
        return False
    if not run_cmd("service networking restart > /dev/null 2>&1"):
        return False
    print "Done!"
    return True

def install_alvs():
    print "Installing ALVS..."
    sys.stdout.flush()
    to_install = raw_input("Do you want to continue [Y/n]? ")
    if to_install != 'Y':
        return True
    try:
        if not install_packages():
            raise Exception("Failed to install packages")
        if not add_modules():
            raise Exception("Cannot add modules")
        if not copy_alvs_files():
            raise Exception("Cannot copy ALVS files")
        if not start_alvs_on_boot():
            raise Exception("Cannot add ALVS to boot sequence")
        if not add_network_config():
            raise Exception("Cannot add ALVS network configurations")
        if not add_nps_host():
            raise Exception("Cannot add NPS host")
        if not reload_configuration():
            raise Exception("Cannot reload configurations")
    except Exception, e:
        print "Installation failed! %s" % str(e)
        return False
    else:
        print "ALVS installation completed successfully"
        print "You can configure the ALVS service at %s and start ALVS with '%s start'" % (ALVS_CONFIG, ALVS_SERVICE)
        return True

########
# Main #
########

def main(argv = None):
    if argv == None:
        argv = sys.argv

    try:
        opts, _ = getopt.getopt(argv[1:], "h", ["help"])
    except getopt.GetoptError as err:
        print str(err) # will print something like "option -a not recognized"
        usage()
        sys.exit(1)

    install = True
    for o, a in opts:
        if o in ("-h","--help"):
            usage()
            sys.exit()
        else:
            assert False, "ERROR: unhandled option"
    
    if install:
        if not install_alvs():
            return False
    return True

if __name__ == "__main__":
    if main():
        sys.exit(0)
    else:
        sys.exit(1)
