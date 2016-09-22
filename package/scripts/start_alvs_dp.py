#!/usr/bin/python

import sys
import os
import time
import socket
import ftplib
import telnetlib
import subprocess

# ALVS
ALVS_CONFIG = '/etc/default/alvs'
ALVS_FOLDER = '/usr/lib/alvs'
ALVS_DP_BIN = 'alvs_dp'
ALVS_DP_PATH = os.path.join(ALVS_FOLDER, ALVS_DP_BIN)

# NPS
NPS_IP = 'alvs_nps'
REMOTE_ALVS_DP_LOCATION = '/tmp'
REMOTE_ALVS_DP_PATH = os.path.join(REMOTE_ALVS_DP_LOCATION, os.path.basename(ALVS_DP_BIN))
REMOTE_ALVS_DP_PROMPT = '/ # '

# General
FTP_RETRIES = 50

def shell_source(script, vars):
    result = []
    pipe = subprocess.Popen(". %s; export %s; env" % (script, " ".join(vars)), stdout=subprocess.PIPE, shell=True)
    output = pipe.communicate()[0]
    env = dict((line.split("=", 1) for line in output.splitlines()))
    for var in vars:
        result.append(env.get(var,None))
    return result

WAIT_FOR_NPS, ALVS_DP_ARGS = shell_source(ALVS_CONFIG, ['WAIT_FOR_NPS','ALVS_DP_ARGS'])

def is_alive(ipaddr):
    rc = os.system('ping -c1 -w100 %s > /dev/null 2>&1' % ipaddr)
    if rc == 0:
        return True
    return False

def wait_for_chip():
    retries = int(WAIT_FOR_NPS)
    while True:
        if retries == 0:
            return False
        if is_alive(NPS_IP):
            return True
        retries -= 1

def copy_dp_app():
    retries = FTP_RETRIES
    while True:
        try:
            ftp = ftplib.FTP(host=NPS_IP, timeout=5)
            ftp.login()
        except socket.error:
            if not retries:
                raise
            retries -= 1
            time.sleep(1)
            
        else:
            break
    ftp.cwd('/tmp')
    ftp.storbinary("STOR %s" % (REMOTE_ALVS_DP_PATH), open(ALVS_DP_PATH, 'r'))
    ftp.quit()

def run_dp_app():
    tn = telnetlib.Telnet(NPS_IP)
    tn.read_until(REMOTE_ALVS_DP_PROMPT)
    tn.write("chmod +x %s\n" % REMOTE_ALVS_DP_PATH)
    tn.read_until(REMOTE_ALVS_DP_PROMPT)
    dp_args = ""
    if ALVS_DP_ARGS:
        dp_args += ALVS_DP_ARGS
    tn.write("%s %s &\n" % (REMOTE_ALVS_DP_PATH,dp_args))
    tn.read_until(REMOTE_ALVS_DP_PROMPT)
    tn.close()


def main():
    if not os.path.isfile(ALVS_DP_PATH):
        print "Cannot find DP application! %s" % ALVS_DP_PATH
        return False
    try:
        if not int(WAIT_FOR_NPS) > 0:
            raise Exception()
    except Exception:
        print "Invalid ALVS configuration! WAIT_FOR_NPS is not a positive number. See %s" % ALVS_CONFIG
        return False
    try:
        if not wait_for_chip():
            raise Exception("Timeout! NPS is not responsive")
        copy_dp_app()
        run_dp_app()
    except Exception, e:
        print "Failed: %s" % e
        return False
    return True

if __name__ == "__main__":
    if main():
        sys.exit(0)
    else:
        sys.exit(1)
