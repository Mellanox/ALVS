#!/usr/bin/env python

import os
import sys

if len(sys.argv) == 1:
	print "examples:"
	print "print services table on line mode from ezbox65: read_sql_db ezbox65 services line"
	print "print servers without line mode from ezbox29: read_sql_db ezbox29 servers"
	print "print servers and fib_entries without line mode from ezbox35: read_sql_db ezbox35 servers fib_entries"
	exit(1)

	
if 'line' in sys.argv:
	column_or_line = "-line"
else:
	column_or_line = "-column"

local_dir = os.getcwd()

# copy from ezbox to our vm
os.system("sshpass -p ezchip scp root@%s-host:/alvs.db %s/alvs_tmp.db"%(sys.argv[1], local_dir))
os.system("sshpass -p ezchip scp root@%s-host:/nw.db %s/nw_tmp.db"%(sys.argv[1], local_dir))
os.system("sshpass -p ezchip scp root@%s-host:/tc_flower.db %s/tc_tmp.db"%(sys.argv[1], local_dir))

alvs_tables = os.popen('/swgwork/tomeri/sandbox/sqlite3 %s/alvs_tmp.db ".tables"'%local_dir)
alvs_tables = alvs_tables.read().split()
print "\nTables on alvs.db: %s\n"%alvs_tables

nw_tables = os.popen('/swgwork/tomeri/sandbox/sqlite3 %s/nw_tmp.db ".tables"'%local_dir)
nw_tables = nw_tables.read().split()
print "\nTables on nw.db: %s\n"%nw_tables

tc_tables = os.popen('/swgwork/tomeri/sandbox/sqlite3 %s/tc_tmp.db ".tables"'%local_dir)
tc_tables = tc_tables.read().split()
print "\nTables on tc.db: %s\n"%tc_tables

for table in alvs_tables:
	if table in sys.argv:
		print "\nTable %s:\n"%table
		os.system('/swgwork/tomeri/sandbox/sqlite3 -header %s %s/alvs_tmp.db "select * from %s"'%(column_or_line,local_dir, table))

for table in tc_tables:
	if table in sys.argv:
		print "\nTable %s:\n"%table
		os.system('/swgwork/tomeri/sandbox/sqlite3 -header %s %s/tc_tmp.db "select * from %s"'%(column_or_line,local_dir, table))
	
		
for table in nw_tables:
	if table in sys.argv:
		print "\nTable %s:\n"%table
		if table == "fib_entries":
			os.system('/swgwork/tomeri/sandbox/sqlite3 -header %s %s/nw_tmp.db "select * from %s ORDER BY nps_index DESC"'%(column_or_line,local_dir, table))
		else:
			os.system('/swgwork/tomeri/sandbox/sqlite3 -header %s %s/nw_tmp.db "select * from %s"'%(column_or_line,local_dir, table))

os.system("rm -f %s/alvs_tmp.db"%local_dir)
os.system("rm -f %s/nw_tmp.db"%local_dir)
# os.system("rm -f %s/tc_tmp.db"%local_dir)
