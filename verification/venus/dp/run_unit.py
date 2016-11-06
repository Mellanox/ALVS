#!/usr/bin/env python


#===============================================================================
# imports
#===============================================================================
import os
from optparse import OptionParser
import sys

#===============================================================================

def get_unit_tests_path():
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	curr_dir = os.path.realpath(os.getcwd())
	unit_tests_dir = curr_dir + "/unit_test/"
	res = []
	for root, dirs, filenames in os.walk(unit_tests_dir):
		for f in filenames:
			res.append(root + f)
	return res

#===============================================================================

def find_include_mock_stat(unit_test):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	with open(unit_test,'r') as f:
		mock_include_statement = f.readline()	
	if len(mock_include_statement.split('/')) < 2 or ( len(mock_include_statement.split('/')) >= 2 and mock_include_statement.split('/')[-2] != "mock"):
		print "Warning: Unit test first line must be #include statement to the mock library!!! Skip running unit test: %s" % unit_test
		return ""		
	src_name_without_extension = mock_include_statement.split('/')[-3]
	print "src name without extension of unit_test %s is: %s" %(unit_test , src_name_without_extension)
	return src_name_without_extension

#===============================================================================

def run_all(dependencies):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	src_unittests_dict = {}
	results = {}
	unit_tests = get_unit_tests_path()
	for unit_test in unit_tests:
		src_name_without_extention = find_include_mock_stat(unit_test)
		if src_name_without_extention == "":
			results[unit_test] = "SKIPPED"
			continue

		if src_name_without_extention not in src_unittests_dict.keys():
					src_unittests_dict[src_name_without_extention] = []
		src_unittests_dict[src_name_without_extention].append(unit_test)

	rc = 0
	final_rc = 0

	for src_name, unit_tests in src_unittests_dict.items():
		make_venus_args = src_name + " "
		if dependencies != None:
			for d in dependencies:
				make_venus_args += d + " "

		rc = os.system("./scripts/make_venus.sh " + make_venus_args)
		if rc:
			print "ERROR in ./scripts/make_venus.sh " +  make_venus_args
			print "Skip running unit tests: " + unit_tests
			final_rc = 1
			continue
			
		for unit_test in unit_tests:
			rc = os.system("./scripts/run_unit_test.sh " + src_name + " " + unit_test)
			if rc:
				print "FAIL: ./scripts/run_unit_test.sh " +  src_name + " " + unit_test
				final_rc = 1
				results[unit_test] = "FAIL"
			else:
				print "\nPASS: ./scripts/run_unit_test.sh " +  src_name + " " + unit_test + "\n"
				results[unit_test] = "PASS"

	print "\nrun_all summary:"
	for unit_test, res in results.items():
		print "[%s] %s" % (unit_test , res)
	if final_rc:
		print "\n\nrun_all FAIL!!!\n"
	else:
		print "\n\nrun_all PASS!!!\n"
	return final_rc

#===============================================================================

def run_unit_test(unit_test , test_case , run_make_venus, dependencies):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	src_name_without_extention = find_include_mock_stat(unit_test)
	if src_name_without_extention == "":
		return 1
	
	make_venus_args = src_name_without_extention + " "
	if dependencies != None:
		for d in dependencies:
			make_venus_args += d + " "

	if run_make_venus:	
		rc = os.system("./scripts/make_venus.sh " + make_venus_args)
		if rc:
			print "ERROR in ./scripts/make_venus.sh " +  make_venus_args
			return 1
	else:
		print "--no_make_venus option was used! skip running make_venus script!"
	
	rc = os.system("./scripts/run_unit_test.sh " + src_name_without_extention + " " + unit_test + " " + test_case)
	if rc:
		print "\nFAIL: ./scripts/run_unit_test.sh " +  src_name_without_extention + " " + unit_test + " " + test_case + "\n"
		return 1
	else:
		print "\nPASS: ./scripts/run_unit_test.sh " +  src_name_without_extention + " " + unit_test + " " + test_case + "\n"
		return 0


#===============================================================================
# main function
#===============================================================================
def main():
	
	usage = "usage: %prog [-u, -t, --no_make_venus, -d]"
	parser = OptionParser(usage=usage, version="%prog 1.0")
	
	parser.add_option("-u", "--unit_test", dest="unit_test",
					  help="Unit test path", type="str")
	parser.add_option("-t", "--test_case", dest="test_case",
					  help="Name of a specific test case to run", default='', type="str")
	parser.add_option("-n","--no_make_venus", action="store_false", dest="no_make_venus",
					  help="Do NOT run make_venus script which build model & mock environment")
	parser.add_option("-d", "--dependencies", dest="dependencies", action="append",
					  help="Other src files which our tested src file depends on", type="str")
	(options, args) = parser.parse_args()

	if not options.unit_test:	
		print "-u argument was not used. Start running all regression..."
		return run_all(options.dependencies)
	
	run_make_venus = True if options.no_make_venus == None else False
	return run_unit_test(options.unit_test , options.test_case , run_make_venus, options.dependencies)

rc = main()
exit(rc)
