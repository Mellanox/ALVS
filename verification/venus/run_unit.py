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
	unit_tests_dir = os.path.join(curr_dir,'unit_tests/')
	res = []
	for root, _, filenames in os.walk(unit_tests_dir):
		for f in filenames:
			res.append(root + "/" + f)
	return res

#===============================================================================

def get_include_stat(unit_test):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	with open(unit_test,'r') as f:
		mock_include_statement = f.readline()
	split_stat = mock_include_statement.split('/')
	split_stat_len = len(split_stat)
	if split_stat_len < 7 or ( split_stat_len >= 7 and split_stat[-2] != "mock"):
		print "Warning: Unit test first line must be #include statement to the mock library!!! Skip running unit test: %s" % unit_test
		return ""
	#print "include statement of unit_test %s is: %s" %(unit_test , mock_include_statement)
	return mock_include_statement

#===============================================================================

def run_all_cp_or_dp(src_unittests_dict, all_cp_or_dp, dependencies, results, app):
	rc = 0
	final_rc = 0

	for src_name, unit_tests in src_unittests_dict.items():
		make_venus_args = app + " " + all_cp_or_dp + " " + src_name + " "
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
			rc = os.system("./scripts/run_unit_test.sh " + all_cp_or_dp + " " + src_name + " " + unit_test)
			if rc:
				print "FAIL: ./scripts/run_unit_test.sh " + all_cp_or_dp + " " +  src_name + " " + unit_test
				final_rc = 1
				results[unit_test] = "FAIL"
			else:
				print "\nPASS: ./scripts/run_unit_test.sh " + all_cp_or_dp + " " +  src_name + " " + unit_test + "\n"
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

def run_all(all_cp_or_dp, dependencies, app):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"

	cp_src_unittests_dict = {}
	dp_src_unittests_dict = {}
	results = {}
	unit_tests = get_unit_tests_path()
	for unit_test in unit_tests:

		mock_include_statement = get_include_stat(unit_test)
		if mock_include_statement == "":
			results[unit_test] = "SKIPPED"
			continue

		src_name_without_extension = mock_include_statement.split('/')[-3]
		#print "src name without extension of unit_test %s is: %s" %(unit_test , src_name_without_extension)
		cp_or_dp = mock_include_statement.split('/')[-4]
		#print "%s is %s unit test" %(unit_test , cp_or_dp)
		if cp_or_dp == "cp":
			if src_name_without_extension not in cp_src_unittests_dict.keys():
				cp_src_unittests_dict[src_name_without_extension] = []
			cp_src_unittests_dict[src_name_without_extension].append(unit_test)
		else:
			if src_name_without_extension not in dp_src_unittests_dict.keys():
				dp_src_unittests_dict[src_name_without_extension] = []
			dp_src_unittests_dict[src_name_without_extension].append(unit_test)
	
	if all_cp_or_dp == "cp":
		return run_all_cp_or_dp(cp_src_unittests_dict, all_cp_or_dp, dependencies, results, app)
	return run_all_cp_or_dp(dp_src_unittests_dict, all_cp_or_dp, dependencies, results, app)

#===============================================================================

def run_unit_test(unit_test , test_case , run_make_venus, dependencies, app):
	print "FUNCTION " + sys._getframe().f_code.co_name + " called"
	
	mock_include_statement = get_include_stat(unit_test)
	if mock_include_statement == "":
		return 1

	src_name_without_extension = mock_include_statement.split('/')[-3]
	print "src name without extension of unit_test %s is: %s" %(unit_test , src_name_without_extension)
	
	cp_or_dp = mock_include_statement.split('/')[-4]
	print "%s is %s unit test" %(unit_test , cp_or_dp)
	
	make_venus_args = app + " " + cp_or_dp + " " + src_name_without_extension + " "
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
	
	rc = os.system("./scripts/run_unit_test.sh " + cp_or_dp + " " + src_name_without_extension + " " + unit_test + " " + test_case)
	if rc:
		print "\nFAIL: ./scripts/run_unit_test.sh " + cp_or_dp + " " +  src_name_without_extension + " " + unit_test + " " + test_case + "\n"
		return 1
	else:
		print "\nPASS: ./scripts/run_unit_test.sh " + cp_or_dp + " " +  src_name_without_extension + " " + unit_test + " " + test_case + "\n"
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
	parser.add_option("-a", "--app", dest="app",
					  help="Name of application to run. Valid values: ALVS", default='', type="str")
	parser.add_option("-n","--no_make_venus", action="store_false", dest="no_make_venus",
					  help="Do NOT run make_venus script which build model & mock environment")
	parser.add_option("-d", "--dependencies", dest="dependencies", action="append",
					  help="Other src files which our tested src file depends on", type="str")
	parser.add_option("-p", "--cp_or_dp", dest="cp_or_dp",
					  help="Run all cp | dp regression. Valid values: cp | dp", type="str")
	(options, args) = parser.parse_args()

	if options.app != "ALVS":
		print "Invalid application name (-a ARG). Currently supported ALVS app only!"
		return 1

	if not options.unit_test:	
		print "-u argument was not used. Start running all regression..."
		if not options.cp_or_dp:
			print "-p argument is missing!!!"
			return 1
		if options.cp_or_dp != "cp" and options.cp_or_dp != "dp":
			print "BAD ARG: -p argument valid values: cp | dp"
			return 1
		return run_all(options.cp_or_dp, options.dependencies, options.app)
	
	run_make_venus = True if options.no_make_venus == None else False

	return run_unit_test(options.unit_test , options.test_case , run_make_venus, options.dependencies, options.app)

rc = main()
exit(rc)

