/* Copyright (c) 2016 Mellanox Technologies, Ltd. All rights reserved.
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
*
*  Project:             NPS400 TC application
*  File:                tc_search_defs.c
*  Desc:                TC search DBs defines.
*
*/

#ifndef TC_SERACH_DEFS_H_
#define TC_SERACH_DEFS_H_

/* system includes */
#include <stdint.h>

/* Linux includes */
#include <netinet/in.h>
#include <net/ethernet.h>

/* dp includes */
#include <ezdp_search_defs.h>

#define MAX_NUM_OF_ACTIONS_IN_FILTER     6

struct l4_hdr {
	uint16_t l4_src;
	uint16_t l4_dst;
} __packed;

CASSERT(sizeof(struct l4_hdr) == 4);

/*tc action types*/
enum tc_action_type {
	/* GACT */
	TC_ACTION_TYPE_GACT_FAMILY		= 0x00,
	TC_ACTION_TYPE_GACT_OK			= 0x01,
	TC_ACTION_TYPE_GACT_DROP		= 0x02,
	TC_ACTION_TYPE_GACT_RECLASSIFY		= 0x03,
	TC_ACTION_TYPE_GACT_CONTINUE		= 0x04,
	TC_ACTION_TYPE_GACT_PIPE		= 0x05,

	/* MIRRED */
	TC_ACTION_TYPE_MIRRED_FAMILY		= 0x10,
	TC_ACTION_TYPE_MIRRED_EGR_REDIRECT	= 0x11,
	TC_ACTION_TYPE_MIRRED_EGR_MIRROR	= 0x12,

	/* PEDIT */
	TC_ACTION_TYPE_PEDIT_FAMILY		= 0x20,
	TC_ACTION_TYPE_PEDIT_IP_ACTION		= 0x21,
	TC_ACTION_TYPE_PEDIT_TCP_ACTION		= 0x22,
	TC_ACTION_TYPE_PEDIT_UDP_ACTION		= 0x23,
	TC_ACTION_TYPE_PEDIT_ICMP_ACTION	= 0x24,

	/* Others */
	TC_ACTION_TYPE_OTHER_FAMILY		= 0x30,
	TC_ACTION_TYPE_IFE			= 0x31,

	/* action masks */
	TC_ACTION_FAMILY_MASK			= 0xF0,
	TC_ACTION_TYPE_MASK			= 0x0F
};


/*mask bitmap*/
union tc_mask_bitmap {
	struct{
#ifdef NPS_BIG_ENDIAN
		unsigned             is_l4_dst_set		: 1;
		unsigned             is_l4_src_set		: 1;
		unsigned             is_ip_proto_set		: 1;
		unsigned             is_ipv4_dst_set		: 1;
		unsigned             is_ipv4_src_set		: 1;
		unsigned             is_eth_type_set		: 1;
		unsigned             is_eth_src_set		: 1;
		unsigned             is_eth_dst_set		: 1;
#else
		unsigned             is_eth_dst_set		: 1;
		unsigned             is_eth_src_set		: 1;
		unsigned             is_eth_type_set		: 1;
		unsigned             is_ipv4_src_set		: 1;
		unsigned             is_ipv4_dst_set		: 1;
		unsigned             is_ip_proto_set		: 1;
		unsigned             is_l4_src_set		: 1;
		unsigned             is_l4_dst_set		: 1;
#endif

#ifdef NPS_BIG_ENDIAN
		unsigned             is_vlan_eth_set		: 1;
		unsigned             is_vlan_id_set		: 1;
		unsigned             is_vlan_prio_set		: 1;
		unsigned             /*reserved*/               : 5;
#else
		unsigned             /*reserved*/               : 5;
		unsigned             is_vlan_prio_set		: 1;
		unsigned             is_vlan_id_set		: 1;
		unsigned             is_vlan_eth_set		: 1;
#endif
	} __packed;
	uint16_t raw_data;
};
CASSERT(sizeof(union tc_mask_bitmap) == 2);

/*mask info*/
struct tc_mask_info {
	union tc_mask_bitmap mask_bitmap;
	uint8_t             ipv4_src_mask_val;
	uint8_t             ipv4_dst_mask_val;
};
CASSERT(sizeof(struct tc_mask_info) == 4);

/*********************************
 * TC flow cache classifier DB defs
 *********************************/

/*key*/
struct tc_flow_cache_classifier_key {
	/*byte0*/
	uint8_t              zero_rsrv1;
	/*byte1*/
	uint8_t              ingress_logical_id;
	/*byte2-7*/
	struct ether_addr    dst_mac;
	/*byte8-13*/
	struct ether_addr    src_mac;
	/*byte14-15*/
	uint16_t             ether_type;
	/*byte16-19*/
	in_addr_t            src_ip;
	/*byte20-23*/
	in_addr_t            dst_ip;
	/*byte24-25*/
	uint16_t             ip_proto;
	/*byte26-27*/
	uint16_t             l4_src_port;
	/*byte28-29*/
	uint16_t             l4_dst_port;
	/*byte30-31*/
	uint16_t             zero_rsrv2; /*reserved for future vlan fields and for alignment issues - must be set to zero*/
} __packed;

CASSERT(sizeof(struct tc_flow_cache_classifier_key) == 32);

/*result*/
struct tc_flow_cache_classifier_result {
	/*byte0*/
#ifdef NPS_BIG_ENDIAN
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned             /*reserved*/                 : 4;
#else
	unsigned             /*reserved*/                 : 4;
	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
#endif
	/*byte1-3*/
	unsigned             /*reserved*/  : 24;
	/*byte4-7*/
	uint32_t              flow_cache_index;
} __packed;

CASSERT(sizeof(struct tc_flow_cache_classifier_result) == 8);

/*********************************
 * TC flow cache index DB defs
 *********************************/

/*key*/
struct tc_flow_cache_idx_key {
	/*byte0-3*/
	uint32_t             flow_index;
};

CASSERT(sizeof(struct tc_flow_cache_idx_key) == 4);

/*result*/
struct tc_flow_cache_idx_result {
	/*byte0*/
#ifdef NPS_BIG_ENDIAN
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	uint32_t              aging_bit                   : 1;
	unsigned             /*reserved*/                 : 3;
#else
	unsigned             /*reserved*/                 : 3;
	uint32_t              aging_bit                   : 1;

	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
#endif
	/*byte1*/
	unsigned             /*reserved*/  : 8;
	/*byte2-3*/
	uint32_t              priority     : 16;
	/*byte4-7*/
	uint32_t              action_index;
	/*byte8-9*/
	uint16_t              rules_list_index;  /*0xffff is saved for not valid index*/
	/*byte10-11*/
	unsigned             /*reserved*/  : 16;
	/*byte12-15*/
	unsigned             /*reserved*/  : 32;
	/*byte16-19*/
	unsigned             /*reserved*/  : 32;
	/*byte20-23*/
	unsigned             /*reserved*/  : 32;
	/*byte24-27*/
	unsigned             /*reserved*/  : 32;
	/*byte28-31*/
	unsigned             /*reserved*/  : 32;
	/*byte32-63*/
	struct tc_flow_cache_classifier_key	flow_cache_classifier_key;
} __packed;
CASSERT(sizeof(struct tc_flow_cache_idx_result) == 64);

/*********************************
 * TC classifier DB defs
 *********************************/

/*key*/
union tc_classifier_key {
	struct {
		/*byte0-3*/
		struct tc_mask_info   tc_mask_info;
		/*byte4-9*/
		struct ether_addr     dst_mac;
		/*byte10-15*/
		struct ether_addr     src_mac;
		/*byte16-17*/
		uint16_t              ether_type;
		/*byte18-21*/
		in_addr_t             src_ip;
		/*byte22-25*/
		in_addr_t             dst_ip;
		/*byte26*/
		uint8_t              ip_proto;
		/*byte27*/
		uint8_t               ingress_logical_id;
		/*byte28-29*/
		uint16_t              l4_src_port;
		/*byte30-31*/
		uint16_t              l4_dst_port;
		/*byte32-35*/
		unsigned              vlan_id		:12;
		unsigned              vlan_prio		: 3;
		unsigned              zero_rsv1		: 1;
		uint16_t              zero_rsv2;
	} __packed;
	uint32_t raw_data[9];
};

CASSERT(sizeof(union tc_classifier_key) == 36);


/*result*/
struct tc_classifier_result {
	/*byte0*/
#ifdef NPS_BIG_ENDIAN
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	uint32_t             is_rules_list_valid	  : 1;
	unsigned             /*reserved*/                 : 3;
#else
	unsigned             /*reserved*/                 : 3;
	uint32_t             is_rules_list_valid	  : 1;

	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
#endif
	/*byte1*/
	unsigned             /*reserved*/  : 8;
	/*byte2-3*/
	unsigned             /*reserved*/  : 16;
	/*byte4-7*/
	uint32_t              filter_actions_index;
	/*byte8-12*/
	uint32_t              rules_list_index;
	/*byte12-15*/
	uint32_t              priority;

} __packed;
CASSERT(sizeof(struct tc_classifier_result) == 16);

/*********************************
 * TC classifier rules list DB defs
 * this DB is used only in case of continue action
 * and user configuration of same mask, same key, different priority
 *********************************/

/*key*/
struct tc_rules_list_key {
	uint32_t	rule_index;
} __packed;

CASSERT(sizeof(struct tc_rules_list_key) == 4);

/*result*/
struct tc_rules_list_result {
	/*byte0*/
#ifdef NPS_BIG_ENDIAN
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned             is_next_rule_valid : 1;
	unsigned             /*reserved*/       : 3;
#else
	unsigned             /*reserved*/       : 3;
	unsigned             is_next_rule_valid : 1;

	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
#endif
	/*byte1*/
	unsigned             /*reserved*/  : 8;
	/*byte2-3*/
	unsigned             /*reserved*/  : 16;
	/*byte4-7*/
	uint32_t              filter_actions_index;
	/*byte8-12*/
	uint32_t              next_rule_index;
	/*byte12-15*/
	uint32_t              priority;
};

CASSERT(sizeof(struct tc_rules_list_result) == 16);


/*********************************
 * TC filter actions DB defs
 *********************************/
#define TC_ACTION_INVALID_VALUE		0xffffffff

/*key*/
struct tc_filter_actions_key {
	/*byte0*/
	uint32_t             filter_actions_index;
};
CASSERT(sizeof(struct tc_filter_actions_key) == 4);

/*result*/
struct tc_filter_actions_result {
	/*byte0*/
#ifdef NPS_BIG_ENDIAN
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned             is_next_filter_actions_index_valid  : 1;
	unsigned             /*reserved*/  : 3;
#else
	unsigned             /*reserved*/  : 3;
	unsigned             is_next_filter_actions_index_valid  : 1;

	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
#endif
	/*byte1-3*/
	unsigned             /*reserved*/  : 24;
	/*byte4-7*/
	uint32_t             next_filter_actions_index;
	/*byte8-31*/
	uint32_t             action_index_array[MAX_NUM_OF_ACTIONS_IN_FILTER];  /*0xffffffff - not valid action index*/
};
CASSERT(sizeof(struct tc_filter_actions_result) == 32);

/*********************************
 * TC action DB defs
 *********************************/

/*key*/
struct tc_action_key {
	/*byte0*/
	uint32_t             action_index;
};

CASSERT(sizeof(struct tc_action_key) == 4);


struct mirred_action_data {
	uint8_t             output_if;
#ifdef NPS_BIG_ENDIAN
	unsigned             is_output_lag           : 1;
	unsigned             /*reserved*/            : 7;
#else

	unsigned             /*reserved*/            : 7;
	unsigned             is_output_lag           : 1;
#endif
	unsigned             /*reserved*/            : 16;
} __packed;

CASSERT(sizeof(struct mirred_action_data) == 4);


/*result*/
struct tc_action_result {
	/*byte0*/
#ifdef NPS_BIG_ENDIAN
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned             /*reserved*/            : 4;
#else
	unsigned             /*reserved*/            : 4;
	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
#endif
	/*byte1*/
	enum tc_action_type   action_type  : 8;
	/*byte2-3*/
	enum tc_action_type   control	   : 8;
	unsigned             /*reserved*/  : 8;
	/*byte4-7*/
	union {
		/*future use - add data action index or actual data for this action*/
		uint32_t                    action_extra_info_index;

		struct mirred_action_data  mirred;
	} action_data;
	/*byte8-11*/
	ezdp_sum_addr_t       action_stats_addr;
	/*byte12-15*/
	uint32_t              reserved;
};

CASSERT(sizeof(struct tc_action_result) == 16);

/*********************************
 * TC mask DB defs
 *********************************/

#define	TC_MASKS_NUM_PER_PORT_AND_PROTO		64
#define TC_NUMBER_OF_PROTOCOLS_SUPPORTED	2
#define TC_CALC_MASK_BASE_INDEX(in_port)	(in_port << ANL_LOG2(TC_MASKS_NUM_PER_PORT_AND_PROTO))

/*key*/
struct tc_mask_key {
#ifdef NPS_BIG_ENDIAN
	unsigned              interface     : 2;
	unsigned              mask_index    : 6;
#else
	unsigned              mask_index    : 6;
	unsigned              interface     : 2;
#endif
} __packed;

CASSERT(sizeof(struct tc_mask_key) == 1);

/*result*/
struct tc_mask_result {
	/*byte0*/
#ifdef NPS_BIG_ENDIAN
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned             /*reserved*/  : 4;
#else
	unsigned             /*reserved*/  : 4;
	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
#endif
	/*byte1-3*/
	unsigned             /*reserved*/  : 24;
	/*byte4-7*/
	struct tc_mask_info  tc_mask_info;
	/*byte8-11*/
	unsigned             /*reserved*/  : 32;
	/*byte11-15*/
	unsigned             /*reserved*/  : 32;
};

CASSERT(sizeof(struct tc_mask_result) == 16);

/*********************************
 * TC timestamp DB defs
 *********************************/
/*key*/
struct tc_timestamp_key {
	/*byte0*/
	uint32_t             action_index;
};

CASSERT(sizeof(struct tc_timestamp_key) == 4);

/*result*/
struct tc_timestamp_result {
	/*byte0*/
#ifdef NPS_BIG_ENDIAN
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned             /*reserved*/  : 4;
#else
	unsigned             /*reserved*/  : 4;
	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
#endif
	/*byte1-3*/
	unsigned             /*reserved*/  : 24;
	/*byte4-7*/
	uint32_t              tc_timestamp_val;
	/*byte8-11*/
	unsigned             /*reserved*/  : 32;
	/*byte11-15*/
	unsigned             /*reserved*/  : 32;
};

CASSERT(sizeof(struct tc_timestamp_result) == 16);

/*********************************
 * TC pedit action info DB defs
 *********************************/
/*key*/
struct tc_pedit_action_info_key {
	uint32_t             pedit_action_index;
};

CASSERT(sizeof(struct tc_pedit_action_info_key) == 4);

/*result*/
struct tc_pedit_action_info_result {
#ifdef NPS_BIG_ENDIAN
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned             is_next_key_valid : 1;
	unsigned             /*reserved*/         : 3;

#else
	unsigned             /*reserved*/         : 3;
	unsigned             is_next_key_valid : 1;

	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
#endif
	/*byte1*/
	unsigned             /*reserved*/  : 8;
	/*byte2-3*/
	unsigned             /*reserved*/  : 16;
	/*byte4-7*/
	uint32_t                mask;  /* AND */
	/*byte8-11*/
	uint32_t                val;   /*XOR */
	/*byte12-15*/
	 int                    off;  /*offset */
	/*byte16-19*/
	 int                    at;
	/*byte20-23*/
	 uint32_t               offmask;
	/*byte24-27*/
	 uint32_t               shift;
	/*byte28-31*/
	 uint32_t               next_key_index;

};
CASSERT(sizeof(struct tc_pedit_action_info_result) == 32);


#endif /* TC_SERACH_DEFS_H_ */
