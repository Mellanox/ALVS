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
*  File:                nw_api_defs.h
*  Desc:                Network API definitions.
*
*/

#ifndef _NW_API_DEFS_H_
#define _NW_API_DEFS_H_

#include <stdbool.h>
#include <netinet/in.h>
#include <net/ethernet.h>

enum nw_api_rc {
	NW_API_OK,
	NW_API_FAILURE,
	NW_API_DB_ERROR,
};

struct nw_api_inet_addr {
	u_int16_t	af;
	/* Address family. supported AF are: AF_INET & AF_INET6 */
	union {
		struct in_addr  in;
		/* IPv4 address */
		struct in6_addr in6;
		/* IPv6 address */
	};
};

struct nw_api_fib_entry {
	struct nw_api_inet_addr dest;
	/* Destination address */
	uint8_t mask_length;
	/* Destination mask length for address matching */
	uint8_t route_type;
	/* rtm_type defined in rtnetlink.h */
	uint8_t next_hop_count;
	/* Number of next hops. 0 = neighbor, 1 = gateway, 1 < not supported  */
	struct nw_api_inet_addr next_hop;
	/* Next hop address */
	uint8_t next_hop_if;
	/* Next hop interface  */
};

struct nw_api_arp_entry {
	uint8_t if_index;
	/* Interface index */
	struct nw_api_inet_addr ip_addr;
	/* IP address */
	struct ether_addr mac_addr;
	/* MAC address */
};

struct nw_api_if_entry {
	uint8_t if_index;
	/* Interface index */
	struct ether_addr mac_addr;
	/* MAC address */
};

#endif /* _NW_API_DEFS_H_ */
