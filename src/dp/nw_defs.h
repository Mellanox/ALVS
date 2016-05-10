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

#include "nw_search_defs.h"
#include "log.h"

/*prototypes*/
void packet_processing(void) __fast_path_code;
bool init_nw_shared_cmem(void);

enum nw_to_host_cause_id {
	NW_EZFRAME_VALIDATION_FAIL          = 0,
	NW_PACKET_MAC_ERROR                 = 1,
	NW_PACKET_IPV4_ERROR                = 2,
	NW_PACKET_NOT_MY_MAC                = 3,
	NW_PACKET_NOT_IPV4                  = 4,
	NW_PACKET_NOT_UDP_AND_TCP           = 5,
	NW_PACKET_NO_VALID_ROUTE            = 6,
	NW_PACKET_FAIL_ARP                  = 7,
	NW_PACKET_FAIL_INTERFACE_LOOKUP     = 8,
	NW_CAUSE_ID_LAST
};

struct cmem_nw_info {
	union{
		struct ezdp_decode_mac_result      mac_decode_result;
		/**< Result of Decode MAC */
		struct ezdp_decode_ipv4_result     ipv4_decode_result;
		/**< Result of Decode IP */
	};
	struct nw_arp_key                    arp_key;
	/**< arp key */
	struct  nw_if_result                 interface_result;
	/**< interface result */
};

union nw_workarea {
	char             arp_hash_wa[EZDP_HASH_WORK_AREA_SIZE(sizeof(struct nw_arp_result), sizeof(struct nw_arp_key))];
	char             table_work_area[EZDP_TABLE_PRM_WORK_AREA_SIZE];
};

/***********************************************************************//**
 * \struct  nw_shared_cmem
 * \brief
 **************************************************************************/

struct shared_cmem_network {
	ezdp_table_struct_desc_t    interface_struct_desc;
	ezdp_hash_struct_desc_t	    arp_struct_desc;
} __packed;

extern struct cmem_nw_info           cmem_nw;
extern struct shared_cmem_network    shared_cmem_nw;
extern union  cmem_workarea          cmem_wa;

#endif   /*NW_DEFS_H_*/
