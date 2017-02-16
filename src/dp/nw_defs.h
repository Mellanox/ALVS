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
*  Project:             NPS400 ALVS application
*  File:                nw_defs.h
*  Desc:                definitions file for nw variables
*                       - need to be located on CMEM (private & shared)
*/

#ifndef NW_DEFS_H_
#define NW_DEFS_H_

#include <ezdp.h>
#include "log.h"
#include "nw_conf.h"
#include "nw_search_defs.h"
#include "application_search_defs.h"


#ifdef NDEBUG
#define __fast_path_code __imem_1_cluster_func
#else
#define __fast_path_code __imem_all_cluster_func
#endif
#define __slow_path_code __imem_all_cluster_func


/* Number of lag members is hard coded and depended on compilation flag. */
/* in case user wants to disable LAG functionality need to set this flag. */
#define NUM_OF_LAG_MEMBERS                   4

#define MAX_DECODE_SIZE 28


/*prototypes*/
void packet_processing(void) __fast_path_code;
bool init_nw_shared_cmem(void);

struct nw_arp_entry {
	struct nw_arp_result result;
	union {
		struct nw_arp_key key;

		EZDP_PAD_HASH_ENTRY(sizeof(struct nw_arp_result), sizeof(struct nw_arp_key));
	};
};

struct packet_meta_data {
	/*byte0*/
	unsigned number_of_tags                   : EZDP_DECODE_MAC_RESULT_NUMBER_OF_TAGS_SIZE;
	unsigned     /* reserved */               : 5;
	/*byte1*/
	unsigned     /* reserved */               : 8;
	/*byte2-3*/
	struct ezdp_decode_mac_control            mac_control;
	/*byte4-5*/
	struct ezdp_decode_mac_protocol_type      last_tag_protocol_type;
	/*byte6*/
	struct ezdp_decode_ip_next_protocol       ip_next_protocol;
	/*byte7*/
	uint8_t                                   ip_offset;
} __packed;

struct cmem_nw_info {

	struct  nw_if_result            ingress_if_result;
	/* FIB & ARP are not used in the same time - saving CMEM */
	struct nw_arp_key                    arp_key;
	/**< arp key */
	union {
		struct nw_arp_entry                  arp_entry;
		/**< arp entry result */
		struct nw_fib_key                    fib_key;
		/**< FIB key */
		struct  nw_if_result		     egress_if_result;
		/**< interface result */
		struct  nw_if_addresses_result	     ingress_if_addresses_result;
		/**< ingress interface addresses result */
	};
	struct  nw_lag_group_result	     lag_group_result;
	/**< LAG group result */
};

struct ezdp_decode_result {
	union {
		struct ezdp_decode_mac_result      mac_decode_result;
		/**< Result of Decode MAC */
		struct ezdp_decode_ipv4_result     ipv4_decode_result;
		/**< Result of Decode IP */
	};
} __packed;

struct route_entry_result {
	in_addr_t                  dest_ip;
	enum nw_fib_type           result_type;
	bool                       is_lag;
	uint8_t                    output_index;
} __packed;

enum nw_arp_processing_result {
	NW_ARP_OK		= 0,
	/* ARP processing finished successfully */
	NW_ARP_CRITICAL_ERR	= 1,
	/* ARP processing had critical error */
	NW_ARP_LOOKUP_FAIL	= 2
	/* ARP processing had ARP lookup fail */
};

/*temp workarea*/
union nw_workarea {
	char                                 arp_prm_hash_wa[EZDP_HASH_PRM_WORK_AREA_SIZE];
	char                                 table_work_area[EZDP_TABLE_WORK_AREA_SIZE(sizeof(struct nw_if_result))];
	/**< working areas */
	struct ezdp_decode_result            ezdp_decode_result;
	/**< protocol decode result */
	struct nw_fib_result                 fib_result;
	/**< FIB result */
	struct ezdp_lookup_int_tcam_result   int_tcam_result;
	/**< DP general result for iTCAM */
} __packed;

/***********************************************************************//**
 * \struct  nw_shared_cmem
 * \brief
 **************************************************************************/

struct shared_cmem_network {
	ezdp_table_struct_desc_t    interface_struct_desc;
	ezdp_table_struct_desc_t    interface_addresses_struct_desc;
	ezdp_table_struct_desc_t    lag_group_info_struct_desc;
	ezdp_hash_struct_desc_t	    arp_struct_desc;
} __packed;

extern struct cmem_nw_info           cmem_nw;
extern struct shared_cmem_network    shared_cmem_nw;

#endif   /*NW_DEFS_H_*/
