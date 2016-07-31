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
 * Service classification DB defs
 *********************************/

/*key*/
struct alvs_service_classification_key {
	in_addr_t service_address;
	uint16_t  service_port;
	uint16_t  service_protocol;
} __packed;

CASSERT(sizeof(struct alvs_service_classification_key) == 8);

/*result*/
struct alvs_service_classification_result {
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
	uint8_t              service_index;
	/*byte2-3*/
	unsigned             /*reserved*/  : 16;
};

CASSERT(sizeof(struct alvs_service_classification_result) == 4);

/*********************************
 * Service info DB defs
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
struct alvs_service_info_key {
	uint8_t service_index;
} __packed;

CASSERT(sizeof(struct alvs_service_info_key) == 1);

/*result*/
struct alvs_service_info_result {
	/*byte0*/
#ifdef ALVS_BIG_ENDIAN
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;

	enum alvs_scheduler_type sched_alg : 4;
#else
	enum alvs_scheduler_type sched_alg : 4;

	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
#endif
	/*byte1*/
	unsigned             /*reserved*/  : 8;
	/*byte2-3*/
	uint16_t             sched_entries_count;
	/*byte4-7*/
	ezdp_sum_addr_t      service_sched_ctr; /* scheduling counter */
	/*byte8-11*/
	ezdp_sum_addr_t      service_stats_base;
	/*byte12-15*/
	uint32_t             service_flags;
};

CASSERT(sizeof(struct alvs_service_info_result) == 16);

/*********************************
 * Scheduling info DB defs
 *********************************/

/*key*/
struct alvs_sched_info_key {
	uint16_t sched_index;
} __packed;

CASSERT(sizeof(struct alvs_sched_info_key) == 2);

/*result*/
struct alvs_sched_info_result {
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
	/*byte1-3*/
	unsigned             /*reserved*/  : 24;
	/*byte4-7*/
	uint32_t             server_index;
	/*byte8-11*/
	unsigned             /*reserved*/  : 32;
	/*byte12-15*/
	unsigned             /*reserved*/  : 32;
};

CASSERT(sizeof(struct alvs_sched_info_result) == 16);

/*********************************
 * Connection classification DB defs
 *********************************/

/*key*/
struct alvs_conn_classification_key {
	in_addr_t client_ip;
	in_addr_t virtual_ip;
	uint16_t  client_port;
	uint16_t  virtual_port;
	uint16_t  protocol;
} __packed;

CASSERT(sizeof(struct alvs_conn_classification_key) == 14);

/*result*/
struct alvs_conn_classification_result {
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
	/*byte1-3*/
	unsigned             /*reserved*/  : 24;
	/*byte4-7*/
	uint32_t             conn_index;
};

CASSERT(sizeof(struct alvs_conn_classification_result) == 8);


/*********************************
 * Connection info DB defs
 *********************************/

/*must be >0 and <256 due to use of ezdp_mod*/
enum alvs_tcp_conn_state {
	ALVS_TCP_CONNECTION_ESTABLISHED = 60,
	ALVS_TCP_CONNECTION_CLOSE_WAIT	= 1
};

/*key*/
struct alvs_conn_info_key {
	uint32_t conn_index;
} __packed;

CASSERT(sizeof(struct alvs_conn_info_key) == 4);

/*result*/
struct alvs_conn_info_result {
	/*byte0*/
#ifdef ALVS_BIG_ENDIAN
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;

	uint8_t              sync_bit      : 1;
	uint8_t              aging_bit     : 1;
	uint8_t              delete_bit    : 1;
	uint8_t              reset_bit     : 1;
#else
	uint8_t              reset_bit     : 1;
	uint8_t              delete_bit    : 1;
	uint8_t              aging_bit     : 1;
	uint8_t              sync_bit      : 1;

	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
#endif
	/*byte1-3*/
	unsigned             /*reserved*/  : 24;
	/*byte4-7*/
	uint32_t             server_index;
	/*byte8-11*/
	struct ezdp_sum_addr conn_stats_base;
	/*byte12-25*/
	struct alvs_conn_classification_key conn_class_key;
	/*byte26*/
	enum alvs_tcp_conn_state conn_state :8;
	/*byte27*/
	uint8_t              age_iteration;
	/*byte28-31*/
	uint32_t             conn_flags;
};

CASSERT(sizeof(struct alvs_conn_info_result) == 32);

/*********************************
 * Server info DB defs
 *********************************/

enum alvs_routing_type {
	ALVS_DIRECT_ROUTING     = 0,
	ALVS_TUNNEL_ROUTING     = 1,
	ALVS_NAT_ROUTING        = 2,
	ALVS_ROUTING_LAST
};

/*key*/
struct alvs_server_info_key {
	uint32_t server_index;
} __packed;

CASSERT(sizeof(struct alvs_server_info_key) == 4);

/*result*/
struct alvs_server_info_result {
	/*byte0*/
#ifdef ALVS_BIG_ENDIAN
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;

	enum alvs_routing_type routing_alg : 4;
#else
	enum alvs_routing_type routing_alg : 4;

	unsigned             /*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned             /*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
#endif
	/*byte1*/
	unsigned             /*reserved*/  : 8;
	/*byte2-3*/
	uint16_t             server_weight;
	/*byte8-11*/
	ezdp_sum_addr_t      server_stats_base;
	/*byte8-11*/
	in_addr_t            server_ip;
	/*byte12-13*/
	uint16_t             server_port;
	/*byte14-15*/
	unsigned             /*reserved*/  : 16;
	/*byte16-19*/
	ezdp_sum_addr_t      service_stats_base;
	/*byte20-23*/
	uint16_t             u_thresh;
	uint16_t             l_thresh;
	/*byte24-27*/
	uint32_t             conn_flags;
	/*byte28-31*/
	uint32_t             server_flags;
};

CASSERT(sizeof(struct alvs_server_info_result) == 32);

#endif /* ALVS_SERACH_DEFS_H_ */
