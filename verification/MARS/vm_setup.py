#!/usr/bin/env python
import os
import inspect
from optparse import OptionParser

'''
	Functions Definition
'''
def vm_cmd(vm, cmd, verbose=False):
	if verbose:
		print cmd
	else:
		print cmd + '  ...'
		cmd += " > /dev/null 2>&1"
	return (os.system('sshpass -p 3tango ssh root@%s "%s"' % (vm, cmd)) == 0)

def option_parser():
	usage = "usage: %prog [-v OR -f]"
	parser = OptionParser(usage=usage, version="%prog 1.0")

	parser.add_option("-v", "--vm", dest="vm",
					  help="VM IP or name")
	parser.add_option("-f", "--filename", dest="vm_list_filename",
					  help="VM list filename")
	(options, args) = parser.parse_args()

	# validate options
	if not options.vm and not options.vm_list_filename :
	    print 'ERROR: One option must be set!'
	    print '       Please provide VM name/IP or VM list filename.'
	    exit(1)
	
	return options

def open_file(file_path):
	try:
		fd = open(file_path, 'r')
	except:
		print '****** Error openning file %s !!!' % file_path
		exit(1)
	return fd

def install(vm):
	cmd = 'echo "hello world"'
	retval = vm_cmd(vm,cmd)
	if retval == False:
		print "ERROR: Bad VM IP or name %s" %vm
		exit(1)
	else:
		print "==== Installing VM %s ====" %vm

	cmd = "wget https://bootstrap.pypa.io/get-pip.py"
	retval = vm_cmd(vm,cmd)
	if retval == False:
		exit(1)

	cmd = "python2.7 get-pip.py"
	retval = vm_cmd(vm,cmd)
	if retval == False:
		exit(1)

	cmd = "pip2.7 install pexpect"
	retval = vm_cmd(vm,cmd)
	if retval == False:
		exit(1)

	cmd = "wget http://dl.fedoraproject.org/pub/epel/6/x86_64/sshpass-1.05-1.el6.x86_64.rpm"
	retval = vm_cmd(vm,cmd)
	if retval == False:
		exit(1)

	cmd = "rpm -ivh sshpass-1.05-1.el6.x86_64.rpm"
	retval = vm_cmd(vm,cmd)
	if retval == False:
		exit(1)

'''
	MAIN 
'''
options = option_parser()
vm_list = []
if options.vm_list_filename:
	fd = open_file(options.vm_list_filename)
	for line in fd:
		if line[0] != '#':
			split_line = split(line)
			split_line = [x.strip() for x in split_line]
			vm_list.append(split_line[0])
else:
	vm_list.append(options.vm)

for vm in vm_list:
	install(vm)

print "==== installation ended succesfully on %d VMs ====" %len(vm_list)
exit(0)





