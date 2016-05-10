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

create_log_folder
find ./src/ -type f ! -name 'sql*.*' -exec ./verification/scripts/checkpatch.pl -terse -file -no-tree {} \; | grep -v 'line over 80 characters' | grep -v 'externs should be avoided in .c files' | grep -v 'braces {} are not necessary for single statement blocks' | grep -v 'quoted string split across lines' | grep -v 'braces {} are not necessary for any arm of this statement' | grep -v 'lines checked' > logs/coding_style.log
rc=$(cat logs/coding_style.log | wc -l)
if [ $rc -ne 0 ]; then
    echo 'Coding style script failed, please refer to logs/coding_style.log'
fi
exit $rc
