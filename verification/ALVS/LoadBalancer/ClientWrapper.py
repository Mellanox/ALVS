#!/usr/bin/env python

import urllib2
import sys
from optparse import OptionParser


################################################################################
# Function: Main
################################################################################
def readHtml(ip):
    print 'Openning HTTP connection with ' + ip
    
    response = urllib2.urlopen(ip)
    html = response.read()
    if isinstance(html, str):
        print 'HTML is a string:' + html
    else:
        print 'HTML is not a string'
        print html

    # TODO: get IPs / index.http from other place
    if '10.157.7.195' in str(html):
        return 1
    elif '10.157.7.196' in html:
        return 2
    elif '10.157.7.197' in html:
        return 3
    elif '10.157.7.198' in html:
        return 4
    elif '10.157.7.199' in html:
        return 5
    elif '10.157.7.200' in html:
        return 6
        
    return 0

    
################################################################################
# Function: Main
################################################################################
def main():
    usage = "usage: %prog [-i, -r, --s1, --s2, --s3, --s4, --s5, --s6]"
    parser = OptionParser(usage=usage, version="%prog 1.0")
    
    parser.add_option("-i", "--http_ip", dest="http_ip",
                      help="IP of the HTTP server")
    parser.add_option("-r", "--requests", dest="num_of_requests",
                      help="Number of HTTP requests", default=1, type="int")
    parser.add_option("--s1", "--expect_res_server1", dest="expect_res_server1",
                      help="Expected response from server 1", default=0, type="int")
    parser.add_option("--s2", "--expect_res_server2", dest="expect_res_server2",
                      help="Expected response from server 2", default=0, type="int")
    parser.add_option("--s3", "--expect_res_server3", dest="expect_res_server3",
                      help="Expected response from server 3", default=0, type="int")
    parser.add_option("--s4", "--expect_res_server4", dest="expect_res_server4",
                      help="Expected response from server 4", default=0, type="int")
    parser.add_option("--s5", "--expect_res_server5", dest="expect_res_server5",
                      help="Expected response from server 5", default=0, type="int")
    parser.add_option("--s6", "--expect_res_server6", dest="expect_res_server6",
                      help="Expected response from server 6", default=0, type="int")

    (options, args) = parser.parse_args()
    if not options.http_ip:
        print 'HTTP IP is not given'
        # format example http://10.157.7.210
        return False
    

    try:
        loadBalancerResArr            = [0,0,0,0,0,0,0]
        loadBalancerResExpectedArr    = [0,1,2,3,4,5,6]
        loadBalancerResExpectedArr[0] = 0 # used for error/unexpected result
        loadBalancerResExpectedArr[1] = options.expect_res_server1
        loadBalancerResExpectedArr[2] = options.expect_res_server2
        loadBalancerResExpectedArr[3] = options.expect_res_server3
        loadBalancerResExpectedArr[4] = options.expect_res_server4
        loadBalancerResExpectedArr[5] = options.expect_res_server5
        loadBalancerResExpectedArr[6] = options.expect_res_server6
        
        # read from HTML server x times (x = options.num_of_requests)
        for i in range(0, options.num_of_requests):
            loadBalancerResArr[ (readHtml('http://'+options.http_ip)) ] += 1 

        # test result
        test_pass = True
        j = 0
        for res in loadBalancerResArr:
            print 'loadBalancerResArr['+str(j)+'] \t= ' + str(res)+'\t expected ' + str(loadBalancerResExpectedArr[j])
            if ( loadBalancerResExpectedArr[j] != res ):
                print 'Error: expected different from actual result'
                test_pass = False
            j = j+1

        return test_pass

    except:
        print "Unexpected error:", sys.exc_info()[0]
        raise




################################################################################
# Script start point
################################################################################
if __name__ == "__main__":
    print '===================='
    print ' Clean trace Script'
    print '===================='
    print 'args:'
    print sys.argv
    print ''
    print '---M a i n   S t a r t e d---'
    rc = main()
    if (rc == True):
        print '----M a i n   E n d e d successfully------'
    else:
        print '----M a i n   E n d e d with failure------'

