#!/bin/bash
echo "=========================== exec wrapper ================================"

echo "connecting to 10.157.7.200"
ssh root@10.157.7.200 "/mswg/release/nps/solutions/auto_build.sh ALVS master; exit"
if [ $? -eq 0 ]; then
    echo "Test passed"
    rc=0
else
    echo "Test Failed"
    rc=1
fi

echo "========================= END exec wrapper =============================="
exit $rc



