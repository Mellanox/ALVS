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
*/

#ifndef NW_SEARCH_DEFS_H_
#define NW_SEARCH_DEFS_H_

/* system includes */
#include <stdint.h>

/* Linux includes */
#include <net/ethernet.h>
#include <netinet/in.h>

/* dp includes */
#include <ezdp_search_defs.h>

/*********************************
 * Interfaces DB defs
 *********************************/

enum dp_path_type {
	DP_PATH_FROM_NW_PATH        = 0,
	/* Frame received on a network port */
	DP_PATH_FROM_HOST_PATH      = 1,
	/* Frame received on a host port */
	DP_PATH_NOT_VALID           = 2
};

/*key*/
struct nw_if_key {
	uint8_t	logical_id;
};

CASSERT(sizeof(struct nw_if_key) == 1);

/*result*/
struct nw_if_result {
	/*byte0*/
#ifdef NPS_BIG_ENDIAN
	unsigned           /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
	unsigned           /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;

	unsigned           oper_status   : 1;
	enum dp_path_type  path_type     : 2;
	unsigned           is_vlan       : 1;
#else
	unsigned           is_vlan       : 1;
	enum dp_path_type  path_type     : 2;
	unsigned           oper_status   : 1;

	unsigned           /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned           /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
#endif
	/*byte1*/
	uint8_t              lag_id;
	/*byte2-3*/
	uint16_t             default_vlan;
	/*byte4-9*/
	struct ether_addr    mac_address;
	/*byte10*/
	uint8_t              output_channel;
	/*byte11*/
	unsigned             /*reserved*/       : 8;
	/*byte12-15*/
	ezdp_sum_addr_t      nw_stats_base;
};

CASSERT(sizeof(struct nw_if_result) == 16);


/*********************************
 * arp DB defs
 *********************************/

/*key*/
struct nw_arp_key {
	in_addr_t real_server_address;
};

CASSERT(sizeof(struct nw_arp_key) == 4);

/*result*/
struct nw_arp_result {
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
	/*byte1*/
	uint8_t              base_logical_id;
	/*byte2-7*/
	struct ether_addr    dest_mac_addr;
};

CASSERT(sizeof(struct nw_arp_result) == 8);

/*********************************
 * FIB DB defs
 *********************************/
/* FIB */
#define NW_FIB_TCAM_SIDE                0
#define NW_FIB_TCAM_LOOKUP_TABLE_COUNT  1
#define NW_FIB_TCAM_TABLE               0
#define NW_FIB_TCAM_PROFILE             0
#define NW_FIB_TCAM_MAX_SIZE	        0x2000

enum nw_fib_type {
	NW_FIB_NEIGHBOR       = 0,
	/* Destination IP is neighbor. use it for ARP */
	NW_FIB_GW             = 1,
	/* Destination IP is GW. use result IP */
	NW_FIB_DROP   = 2
	/* unknown handling. Drop frame */
};


/*key*/
struct nw_fib_key {
	/* bytes 0-3 */
	uint32_t             rsv0;

	/* bytes 4-5 */
	uint16_t             rsv1;

	/* bytes 6-9 */
	in_addr_t            dest_ip;
} __packed;

CASSERT(sizeof(struct nw_fib_key) == 10);

/*result*/
struct nw_fib_result {
	/* byte 0-2 */
#ifdef ALVS_BIG_ENDIAN
	unsigned             match         : EZDP_LOOKUP_INT_TCAM_8B_DATA_RESULT_MATCH_SIZE;
	unsigned             /*reserved*/  : 23;

#else
	unsigned             /*reserved*/  : 23;
	unsigned             match         : EZDP_LOOKUP_INT_TCAM_8B_DATA_RESULT_MATCH_SIZE;

#endif
	/* byte 3 */
	enum nw_fib_type     result_type    : 8;

	/* bytes 4-7 */
	in_addr_t            dest_ip;
};

CASSERT(sizeof(struct nw_fib_result) == 8);

#endif /* NW_SEARCH_DEFS_H_ */
