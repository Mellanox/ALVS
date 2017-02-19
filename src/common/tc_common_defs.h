/*
* Copyright (c) 2016 Mellanox Technologies, Ltd. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
* 3. Neither the names of the copyright holders nor the names of its
*    contributors may be used to endorse or promote products derived from
*    this software without specific prior written permission.
*
* Alternatively, this software may be distributed under the terms of the
* GNU General Public License ("GPL") version 2 as published by the Free
* Software Foundation.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
*  Project:             NPS400 ATC application
*  File:                tc_common_defs.h
*  Desc:                TC common defines & structure file
*/
#ifndef TC_COMMON_DEFS_H_
#define TC_COMMON_DEFS_H_

#include "tc_search_defs.h"

#define MAX_TC_FILTER_KIND_SIZE          10
#define MAX_NUM_OF_KEYS_IN_PEDIT_DATA    16

#define TC_INGRESS_PARENT_ID    0xffff


struct tc_flower_rule_policy {
	uint8_t key_eth_dst[ETH_ALEN];
	uint8_t mask_eth_dst[ETH_ALEN];
	uint8_t key_eth_src[ETH_ALEN];
	uint8_t mask_eth_src[ETH_ALEN];
	uint16_t key_eth_type;
	uint16_t mask_eth_type;
	uint8_t key_ip_proto;
	uint8_t mask_ip_proto;
	in_addr_t key_ipv4_src;
	in_addr_t mask_ipv4_src;
	in_addr_t key_ipv4_dst;
	in_addr_t mask_ipv4_dst;
	uint16_t key_l4_src;
	uint16_t mask_l4_src;
	uint16_t key_l4_dst;
	uint16_t mask_l4_dst;
	union tc_mask_bitmap mask_bitmap;
};


struct tc_mirred_action_data {
	uint32_t                ifindex;  /* ifindex of egress port */

};

struct tc_pedit_key_data {
	 uint32_t           mask;  /* AND */
	 uint32_t           val;   /*XOR */
	 int	            off;  /*offset */
	 int	            at;
	 uint32_t           offmask;
	 uint32_t           shift;

};

struct pedit_key_data {
	uint32_t key_index;
	struct tc_pedit_key_data pedit_key_data;

};

struct tc_pedit_action_data {
	enum tc_action_type control_action_type;
	uint8_t num_of_keys;
	struct pedit_key_data key_data[MAX_NUM_OF_KEYS_IN_PEDIT_DATA];

};


enum tc_filter_type {
	TC_FILTER_FLOWER = 1,
	TC_FILTER_U32,
	TC_FILTER_FW
};
struct tc_action_general {
	uint32_t                 index;
	uint32_t                 capab;
	int                   refcnt;
	int                   bindcnt;
	enum tc_action_type type;
	enum tc_action_type family_type;
};

struct tc_action_statistics {
	uint32_t use_timestamp;
	uint32_t created_timestamp;
	uint64_t packet_statictics;
	uint64_t bytes_statictics;
};

struct tc_action {
	struct tc_action_general general;
	union {

		struct tc_mirred_action_data   mirred;
		struct tc_pedit_action_data pedit;
	} action_data;
	struct tc_action_statistics action_statistics;

};

struct tc_actions {
	uint8_t num_of_actions;
	struct tc_action action[MAX_NUM_OF_ACTIONS_IN_FILTER];
};

struct tc_filter_id {
	uint32_t ifindex;
	uint32_t priority;
	uint32_t handle;
};

struct tc_filter {
	uint32_t priority;
	uint32_t handle;
	uint32_t ifindex;
	uint16_t protocol;
	uint16_t flags;
	enum tc_filter_type type;
	struct tc_flower_rule_policy flower_rule_policy;
	struct tc_actions actions;
};


#endif /* TC_COMMON_DEFS_H_ */
