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

#ifndef DEFS_H_
#define DEFS_H_

#include "user_defs.h"

union temp {
	int x;
	int y;
};

#define SYSLOG_SERVER_IP          "169.254.42.41"
#define SYSLOG_CLIENT_ETH_ADDR    {0x00, 0x02, 0xc9, 0x42, 0x42, 0x43}

#define ALVS_SIZE_OF_SCHED_BUCKET   256

#define ALVS_CONN_MAX_ENTRIES       (32*1024*1024)
#define ALVS_SERVICES_MAX_ENTRIES   256
#define ALVS_SCHED_MAX_ENTRIES      (ALVS_SERVICES_MAX_ENTRIES * ALVS_SIZE_OF_SCHED_BUCKET)
#define ALVS_SERVERS_MAX_ENTRIES    (ALVS_SERVICES_MAX_ENTRIES * 1024)

enum alvs_service_posted_stats_offsets {
	ALVS_SERVICE_STATS_IN_PKTS_BYTES_OFFSET       = 0,
	ALVS_SERVICE_STATS_OUT_PKTS_BYTES_OFFSET      = 2,
	ALVS_SERVICE_STATS_CONN_SCHED_OFFSET          = 4,
	ALVS_NUM_OF_SERVICE_STATS                     = 6
};

enum alvs_server_posted_stats_offsets {
	ALVS_SERVER_STATS_IN_PKTS_BYTES_OFFSET        = 0,
	ALVS_SERVER_STATS_OUT_PKTS_BYTES_OFFSET       = 2,
	ALVS_SERVER_STATS_CONN_SCHED_REFCNT_OFFSET    = 4,
	ALVS_SERVER_STATS_INACTIVE_ACTIVE_CONN_OFFSET = 6,
	ALVS_NUM_OF_SERVER_STATS                      = 8
};

enum nw_if_posted_stats_offsets {
	NW_IF_STATS_FRAME_VALIDATION_FAIL   = 0,
	NW_IF_STATS_MAC_ERROR               = 1,
	NW_IF_STATS_IPV4_ERROR              = 2,
	NW_IF_STATS_NOT_MY_MAC              = 3,
	NW_IF_STATS_NOT_IPV4                = 4,
	NW_IF_STATS_NOT_UDP_OR_TCP          = 5,
	NW_IF_STATS_NO_VALID_ROUTE          = 6,
	NW_IF_STATS_FAIL_ARP_LOOKUP         = 7,
	NW_IF_STATS_FAIL_INTERFACE_LOOKUP   = 8,
	NW_NUM_OF_IF_STATS                  = 10
};

enum alvs_error_stats_offsets {
	ALVS_ERROR_UNSUPPORTED_ROUTING_ALGO     = 0,
	ALVS_ERROR_CANT_EXPIRE_CONNECTION       = 1,
	ALVS_ERROR_CANT_UPDATE_CONNECTION_STATE = 2,
	ALVS_ERROR_CONN_INFO_LKUP_FAIL          = 3,
	ALVS_ERROR_CONN_CLASS_ALLOC_FAIL        = 4,
	ALVS_ERROR_CONN_INFO_ALLOC_FAIL         = 5,
	ALVS_ERROR_CONN_INDEX_ALLOC_FAIL        = 6,
	ALVS_ERROR_SERVICE_CLASS_LKUP_FAIL      = 7,
	ALVS_ERROR_FAIL_SH_SCHEDULING           = 8,
	ALVS_ERROR_SERVER_INFO_LKUP_FAIL        = 9,
	ALVS_ERROR_SERVER_IS_UNAVAILABLE        = 10,
	ALVS_ERROR_SERVER_INDEX_LKUP_FAIL       = 11,
	ALVS_ERROR_CONN_CLASS_LKUP_FAIL         = 12,
	ALVS_ERROR_SERVICE_INFO_LOOKUP          = 13,
	ALVS_ERROR_UNSUPPORTED_SCHED_ALGO       = 14,
	ALVS_ERROR_CANT_MARK_DELETE             = 15,
	ALVS_ERROR_DEST_SERVER_IS_NOT_AVAIL     = 16,
	ALVS_ERROR_SEND_FRAME_FAIL              = 17,
	ALVS_ERROR_CONN_MARK_TO_DELETE          = 18,
	ALVS_ERROR_SERVICE_CLASS_LOOKUP         = 19,
	ALVS_NUM_OF_ALVS_ERROR_STATS            = 20
};

/* Posted Statistics */
#define EMEM_ALVS_ERROR_STATS_POSTED_MSID   USER_POSTED_STATS_MSID
#define EMEM_ALVS_ERROR_STATS_POSTED_OFFSET 0x0
#define EMEM_SERVICE_STATS_POSTED_MSID      USER_POSTED_STATS_MSID
#define EMEM_SERVICE_STATS_POSTED_OFFSET    (EMEM_ALVS_ERROR_STATS_POSTED_OFFSET + ALVS_NUM_OF_ALVS_ERROR_STATS)
#define EMEM_SERVER_STATS_POSTED_MSID       USER_POSTED_STATS_MSID
#define EMEM_SERVER_STATS_POSTED_OFFSET     (EMEM_SERVICE_STATS_POSTED_OFFSET + ALVS_SERVICES_MAX_ENTRIES * ALVS_NUM_OF_SERVICE_STATS)
#define EMEM_IF_STATS_POSTED_MSID           USER_POSTED_STATS_MSID
#define EMEM_IF_STATS_POSTED_OFFSET         (EMEM_SERVER_STATS_POSTED_OFFSET + ALVS_SERVERS_MAX_ENTRIES * ALVS_NUM_OF_SERVER_STATS)

#define EMEM_SPINLOCK_MSID            USER_EMEM_OUT_OF_BAND_MSID
#define EMEM_SPINLOCK_OFFSET          0x0

#define ALVS_HOST_LOGICAL_ID            USER_HOST_LOGICAL_ID
#define ALVS_AGING_TIMER_LOGICAL_ID     USER_TIMER_LOGICAL_ID
#define ALVS_CONN_INDEX_POOL_ID	        USER_POOL_ID

enum struct_id {
	STRUCT_ID_NW_INTERFACES                = 0,
	STRUCT_ID_NW_LAG                       = 1,
	STRUCT_ID_ALVS_CONN_CLASSIFICATION     = 2,
	STRUCT_ID_ALVS_CONN_INFO               = 3,
	STRUCT_ID_ALVS_SERVICE_CLASSIFICATION  = 4,
	STRUCT_ID_ALVS_SERVICE_INFO            = 5,
	STRUCT_ID_ALVS_SCHED_INFO              = 6,
	STRUCT_ID_ALVS_SERVER_INFO             = 7,
	STRUCT_ID_NW_FIB                       = 8,
	STRUCT_ID_NW_ARP                       = 9,
	NUM_OF_STRUCT_IDS
};

#endif /* DEFS_H_ */
