RM := rm -rf
ENV_BASE := $(DP_BASE)../../..

file := $(ENV_BASE)/tools/sdk/sdk_version.txt
EZDK_BASE := $(ENV_BASE)/$(shell cat ${file})
PATH := $(PATH):$(abspath $(EZDK_BASE)/ldk/toolchain/bin)
