#!/usr/bin/env python
import sys
import os

def open_input_file(file_name):
    try:
        infile = open(file_name, 'r')
    except:
        print '****** Error openning file %s !!!' % file_name
        exit (-1)
    if infile:
        return infile

directory = "tools/sdk"
file_name = directory+"/sdk_version.txt"
fp = open_input_file(file_name)
sdk_pth = fp.readline()
bashCommand = "./"+sdk_pth[:-1] + "/EZide &"
print bashCommand
os.system(bashCommand)
