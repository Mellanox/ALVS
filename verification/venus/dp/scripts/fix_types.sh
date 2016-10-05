#!/bin/bash

sed 's/typedef unsigned int u_int8_t;/typedef unsigned char u_int8_t;/g' output/alvs_packet_processing/mock/dut.h > output/alvs_packet_processing/mock/dut.h_new && mv output/alvs_packet_processing/mock/dut.h_new output/alvs_packet_processing/mock/dut.h
sed 's/typedef unsigned int u_int16_t;/typedef unsigned short u_int16_t;/g' output/alvs_packet_processing/mock/dut.h > output/alvs_packet_processing/mock/dut.h_new && mv output/alvs_packet_processing/mock/dut.h_new output/alvs_packet_processing/mock/dut.h

