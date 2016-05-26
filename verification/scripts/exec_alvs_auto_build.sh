#!/bin/bash
echo "=========================== exec wrapper ================================"

echo "connecting to 10.157.7.200"
ssh root@10.157.7.200 "su nps_sw_release; cd /mswg/release/nps/solutions/; ./auto_build.sh ALVS 1.0 |& tee ./ALVS/1.0/auto_build.log; mv ./ALVS/1.0/auto_build.log ./ALVS/1.0/last_release/; exit"
if [ $? -eq 0 ]; then
    echo "Test passed"
    rc=0
else
    echo "Test Failed"
    rc=1
fi

echo "========================= END exec wrapper =============================="
exit $rc



