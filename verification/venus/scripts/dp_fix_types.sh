#!/bin/bash

sed 's/typedef unsigned int u_int8_t;/typedef unsigned char u_int8_t;/g' $@/verification/venus/output/dp/alvs_packet_processing/mock/dut.h >  $@/verification/venus/output/dp/alvs_packet_processing/mock/dut.h_new && mv $@/verification/venus/output/dp/alvs_packet_processing/mock/dut.h_new $@/verification/venus/output/dp/alvs_packet_processing/mock/dut.h
sed 's/typedef unsigned int u_int16_t;/typedef unsigned short u_int16_t;/g' $@/verification/venus/output/dp/alvs_packet_processing/mock/dut.h > $@/verification/venus/output/dp/alvs_packet_processing/mock/dut.h_new && mv $@/verification/venus/output/dp/alvs_packet_processing/mock/dut.h_new $@/verification/venus/output/dp/alvs_packet_processing/mock/dut.h

