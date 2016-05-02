#!/bin/bash
# This script receives a result directory and an expected directory,
# and for every expected pcap file finds its correspondig result pcap file,
# and returns true if all the files contain the same packet data, and false otherwise.
# This means that the result and the expected pcap files contain the same data,
# not necessarily in the same order.
#
# Usage: CheckDirByData.sh <result directory> <expected directory>
# Example: CheckDirByData.sh resultDir expectedDir

if [ "$#" -le  1 ]; then
  echo Missing arguments!
  echo Usage: $0 resultDir expectedDir
  exit -1;
fi

RC=0;

if [ ! -d $1 ]; then
  echo "Directory $1 doesn't exist!";
  exit -1;
fi

if [ ! -d $2 ]; then
  echo "Directory $2 doesn't exist!";
  exit -1;
fi

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )";

NUM_OF_RESULT_FILES=$(find $1/ -name *.pcap -type f | wc -l);
NUM_OF_EXPECTED_FILES=$(find $2/ -name *.pcap -type f | wc -l);

if [ "$NUM_OF_EXPECTED_FILES" -gt 0 ]; then
  for EXPECTED in $2/*.pcap; do 
    RESULT=$1/${EXPECTED:${#2} + 1}
    echo "------ Comparing $RESULT to $EXPECTED ------";
    $DIR/CheckByData.pl $RESULT $EXPECTED;
    if [ "$?" -ne 0 ]; then
      RC=-1;
      echo FAIL!
      echo
    fi
    echo
  done
fi

if [ "$NUM_OF_RESULT_FILES" -lt "$NUM_OF_EXPECTED_FILES" ]; then
  echo
  echo "There are more .pcap files in $2 than $1";
  echo FAIL!
  echo
  RC=-1;
fi

if [ "$NUM_OF_RESULT_FILES" -gt "$NUM_OF_EXPECTED_FILES" ]; then
  echo
  echo "There are more .pcap files in $1 than $2";
  echo FAIL!
  echo
  RC=-1;
fi

if [ "$RC" -eq -1 ]; then
  echo
  echo ===================
  echo Final result: FAIL!
  echo ===================
  echo
else
  echo
  echo ===================
  echo Final result: PASS!
  echo ===================
  echo
fi

exit $RC;
