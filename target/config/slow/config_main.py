#!/usr/bin/env python

def export_configuration_to_python():
    #Exporting XML based target configuration files to Python scripts using the EZConfigExport tool
    run_config_export("config.xml", "config.py")
    run_config_export("search_entries_slow.xml", "search_entries_slow.py")

def run_config_export(config_filename, output_filename):
	EZConfigExport_executable = sys.argv[3] + "/tools/EZConfigExport/bin/EZConfigExport.sh"
	script_folder = os.path.abspath(os.path.dirname(sys.argv[0])) + "/"
	config_export_arguments = [EZConfigExport_executable, "-b", sys.argv[3], "-c", script_folder + config_filename, "-o", script_folder + output_filename]
	print "Executing " + string.join(config_export_arguments)
	subprocess.check_call(config_export_arguments)

import sys
import string
print 'Running', sys.argv[0], 'with arguments:', string.join(sys.argv[1:])

if len(sys.argv) != 4:
	print 'Usage:', sys.argv[0], '<host> <port> <EZdk base dir full path>'
	sys.exit(1)
sys.path.append(sys.argv[3] + "/tools/EZcpPyLib/lib")

import subprocess
import os

export_configuration_to_python()
	
import config
import search_entries_slow

from ezpy_cp import EZpyCP
cpe = EZpyCP(sys.argv[1],sys.argv[2])

print 'Running create()'
cpe.cp.channel.create()
config.created(cpe)

print 'Running initialize()'
cpe.cp.channel.initialize()
config.initialized(cpe)
cpe.cp.struct.load_partition(partition=0)

print 'Running finalize()'
cpe.cp.channel.finalize()
config.finalized(cpe)
search_entries_slow.add_entries(cpe)

print 'Running go()'
cpe.cp.channel.go()
config.running(cpe)

print 'Done'
