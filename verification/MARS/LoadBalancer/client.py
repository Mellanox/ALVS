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
        print 'HTML is a string'
    else:
        print 'HTML is not a string'
    #expected nps006 is in the house!!!
    print html
    if 'nps005' in str(html):
        return 5
    elif 'nps006' in html:
        return 6
        
    return

    
################################################################################
# Function: Main
################################################################################
def main():
    #ip = 'http://10.157.7.210'
    usage = "usage: %prog [-i -r -p -l -d]"
    parser = OptionParser(usage=usage, version="%prog 1.0")
    
    parser.add_option("-i", "--http_ip", dest="http_ip",
                      help="IP of the HTTP server")
    parser.add_option("-r", "--requests", dest="num_of_requests",
                      help="Number of HTTP requests", default=1, type="int")

    (options, args) = parser.parse_args()
    if not options.http_ip:
        print 'HTTP IP is not given'
        return 
    

    try:
        # TODO: get IP as argument
        ip = 'http://10.157.7.210'
        
        loadBalancerResArr = [0,0,0,0,0,0,0,0,0,0,0]
        for i in range(0, options.num_of_requests):
            loadBalancerResArr[ readHtml(options.http_ip) ] += 1 

        j = 0
        for i in loadBalancerResArr:
            print 'loadBalancerResArr['+str(j)+'] = ' + str(i)
            j = j+1

        return

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
    main()
    print '----M a i n   E n d e d------'

