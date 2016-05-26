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
	DP_PATH_SEND_TO_NW_NA        = 0,
	/* Send to nw interface without application logic - short circuit */
	DP_PATH_SEND_TO_HOST_NA      = 1,
	/* Send to host interface without application logic - short circuit */
	DP_PATH_SEND_TO_NW_APP       = 2,
	/* Send to nw interface with application logic */
	DP_PATH_SEND_TO_HOST_APP     = 3,
	/* Send to host interface with application logic */
};

/*key*/
struct nw_if_key {
	uint8_t	logical_id;
};

/*result*/
struct nw_if_result {
	/*byte0*/
#ifdef ALVS_BIG_ENDIAN
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
	/*byte11-15*/
	unsigned             /*reserved*/       : 8;
	unsigned             /*reserved*/       : 32;
};


/*********************************
 * arp DB defs
 *********************************/

/*key*/
struct nw_arp_key {
	in_addr_t real_server_address;
};

/*result*/
struct nw_arp_result {
	/*byte0*/
#ifdef ALVS_BIG_ENDIAN
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

#endif /* NW_SEARCH_DEFS_H_ */
