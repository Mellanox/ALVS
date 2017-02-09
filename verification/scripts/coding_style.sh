#!/bin/bash

function create_log_folder()
{
    wa_path="logs/"
    test -d $wa_path
    if [ $? -ne 0 ]; then
        echo "Creating $wa_path folder"
        mkdir $wa_path
    fi
}

# check arguments
test $# -eq 0
if [ $? -eq 0 ]; then
    src="./src/"
else
    test $# -eq 1
    if [ $? -eq 0 ]; then
        src="./$1"
    else
        echo "too many arguments..."
        exit 1
    fi
fi
echo "Running coding style on $src"


create_log_folder

#run coding style
find $src  -type f ! -name 'sql*.*' -exec ./verification/scripts/checkpatch.pl -terse -file -no-tree {} \; | grep -v 'Prefer ether_addr_copy() over memcpy() if the Ethernet addresses are __aligned(2)' | grep -v 'line over 80 characters' | grep -v 'externs should be avoided in .c files' | grep -v 'braces {} are not necessary for single statement blocks' | grep -v 'quoted string split across lines' | grep -v 'braces {} are not necessary for any arm of this statement' | grep -v 'lines checked' | grep -v 'CVS style keyword markers, these will _not_ be updated' | grep -v 'Prefer ether_addr_equal() or ether_addr_equal_unaligned() over memcmp()' | grep -v 'Macros with flow control statements should be avoided' > logs/coding_style.log

# check coding style results
rc=$(cat logs/coding_style.log | wc -l)
if [ $rc -ne 0 ]; then
    echo 'Coding style script failed, please refer to logs/coding_style.log'
else
    echo 'Coding style pass succesfuly'
fi

exit $rc
