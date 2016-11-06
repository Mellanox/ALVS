#!/bin/bash

sed 's/typedef unsigned int u_int8_t;/typedef unsigned char u_int8_t;/g' $@/verification/venus/dp/output/alvs_packet_processing/mock/dut.h > $@//verification/venus/dp/output/alvs_packet_processing/mock/dut.h_new && mv $@/verification/venus/dp/output/alvs_packet_processing/mock/dut.h_new $@/verification/venus/dp/output/alvs_packet_processing/mock/dut.h
sed 's/typedef unsigned int u_int16_t;/typedef unsigned short u_int16_t;/g' $@/verification/venus/dp/output/alvs_packet_processing/mock/dut.h > $@/verification/venus/dp/output/alvs_packet_processing/mock/dut.h_new && mv $@/verification/venus/dp/output/alvs_packet_processing/mock/dut.h_new $@/verification/venus/dp/output/alvs_packet_processing/mock/dut.h

