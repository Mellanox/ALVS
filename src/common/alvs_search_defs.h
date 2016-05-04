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

#ifndef ALVS_SERACH_DEFS_H_
#define ALVS_SERACH_DEFS_H_

/* system includes */
#include <stdint.h>

/* Linux includes */
#include <netinet/in.h>

/* dp includes */
#include <ezdp_search_defs.h>

/*********************************
 * Services DB defs
 *********************************/

enum alvs_scheduler_type {
	ALVS_ROUND_ROBIN_SCHEDULER                              = 0,
	ALVS_WEIGHTED_ROUND_ROBIN_SCHEDULER                     = 1,
	ALVS_LEAST_CONNECTION_SCHEDULER                         = 2,
	ALVS_WEIGHTED_LEAST_CONNECTION_SCHEDULER                = 3,
	ALVS_DESTINATION_HASH_SCHEDULER                         = 4,
	ALVS_SOURCE_HASH_SCHEDULER                              = 5,
	ALVS_LB_LEAST_CONNECTION_SCHEDULER                      = 6,
	ALVS_LB_LEAST_CONNECTION_SCHEDULER_WITH_REPLICATION     = 7,
	ALVS_SHORTEST_EXPECTED_DELAY_SCHEDULER                  = 8,
	ALVS_NEVER_QUEUE_SCHEDULER                              = 9,
	ALVS_SCHEDULER_LAST
};

/*key*/
struct alvs_service_key {
	in_addr_t service_address;
	uint16_t  service_port;
	uint8_t   service_protocol;
} __packed;

/*result*/
struct alvs_service_result {
	/*byte0*/
#ifdef BIG_ENDIAN
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned             /*reserved*/  : 4;
#else
	unsigned             /*reserved*/  : 4;
	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
#endif
	/*byte1*/
	unsigned             /*reserved*/  : 8;
	/*byte2-3*/
	uint16_t             service_id;
	/*byte4*/
	enum alvs_scheduler_type sched_type;
	/*byte5-7*/
	unsigned             rs_head_index : 24;
	/* Pointer or index to head of attached list of real-servers(rs) */
	/*byte8-11*/
	struct ezdp_sum_addr sched_data;
	/* Sched data pointer */
	/*byte12-15*/
	in_addr_t            real_server_ip;
	/* TEMP field for POC only! */
};

#endif /* ALVS_SERACH_DEFS_H_ */
