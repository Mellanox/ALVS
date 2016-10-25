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
#include <ezdp_defs.h>
#include <linux/ip.h>
#include <linux/udp.h>

#define __fast_path_code __imem_1_cluster_func
#define __slow_path_code __imem_all_cluster_func

#define IP_DF           0x4000 /* TODO take from netinet/ip.h after fixing includes */
#define IP_V4           0

struct net_hdr {
	struct iphdr ipv4;
	struct udphdr udp;
} __packed;

#define BUILD_SUM_ADDR(mem_space_type, msid, element_index) (((mem_space_type) << EZDP_SUM_ADDR_MEM_TYPE_OFFSET) | ((msid) << EZDP_SUM_ADDR_MSID_OFFSET) | ((element_index) << EZDP_SUM_ADDR_ELEMENT_INDEX_OFFSET))

#define SYSLOG_SERVER_IP          "169.254.42.41"
#define SYSLOG_CLIENT_ETH_ADDR    {0x00, 0x02, 0xc9, 0x42, 0x42, 0x43}

#define ALVS_SIZE_OF_SCHED_BUCKET   256

#define ALVS_CONN_MAX_ENTRIES       (64*1024*1024)
#define ALVS_SERVICES_MAX_ENTRIES   256
#define ALVS_SCHED_MAX_ENTRIES      (ALVS_SERVICES_MAX_ENTRIES * ALVS_SIZE_OF_SCHED_BUCKET)
#define ALVS_SERVERS_MAX_ENTRIES    (ALVS_SERVICES_MAX_ENTRIES * 1024)

enum alvs_service_posted_stats_offsets {
	ALVS_SERVICE_STATS_IN_PKTS_BYTES_OFFSET       = 0,
	ALVS_SERVICE_STATS_OUT_PKTS_BYTES_OFFSET      = 2,
	ALVS_SERVICE_STATS_CONN_SCHED_OFFSET          = 4,
	ALVS_SERVICE_STATS_REFCNT_OFFSET              = 5,
	ALVS_NUM_OF_SERVICE_STATS                     = 6
};

enum alvs_server_posted_stats_offsets {
	ALVS_SERVER_STATS_IN_PKTS_BYTES_OFFSET        = 0,
	ALVS_SERVER_STATS_OUT_PKTS_BYTES_OFFSET       = 2,
	ALVS_SERVER_STATS_CONN_SCHED_OFFSET           = 4,
	ALVS_SERVER_STATS_REFCNT_OFFSET               = 5,
	ALVS_SERVER_STATS_INACTIVE_CONN_OFFSET        = 6,
	ALVS_SERVER_STATS_ACTIVE_CONN_OFFSET          = 7,
	ALVS_NUM_OF_SERVER_STATS                      = 8
};
enum alvs_server_on_demand_stats_offsets {
	ALVS_SERVER_STATS_CONNECTION_TOTAL_OFFSET = 0,
	ALVS_NUM_OF_SERVERS_ON_DEMAND_STATS
};

/* when adding an error, add to nw_if_posted_stats_offsets_names (error_names.h) the name of this error */
enum nw_if_posted_stats_offsets {
	NW_IF_STATS_FRAME_VALIDATION_FAIL   = 0,
	NW_IF_STATS_MAC_ERROR               = 1,
	NW_IF_STATS_IPV4_ERROR              = 2,
	NW_IF_STATS_NOT_MY_MAC              = 3,
	NW_IF_STATS_NOT_IPV4                = 4,
	NW_IF_STATS_NOT_TCP                 = 5,
	NW_IF_STATS_NO_VALID_ROUTE          = 6,
	NW_IF_STATS_FAIL_ARP_LOOKUP         = 7,
	NW_IF_STATS_FAIL_INTERFACE_LOOKUP   = 8,
	NW_IF_STATS_FAIL_FIB_LOOKUP         = 9,
	NW_IF_STATS_REJECT_BY_FIB           = 10,
	NW_IF_STATS_UNKNOWN_FIB_RESULT	    = 11,
	NW_IF_STATS_FAIL_STORE_BUF          = 12,


	/*
	 * Note: 1. The following define must be at the end.
	 *       2. The following define must be even
	 */
	NW_NUM_OF_IF_STATS                  = 20
};

enum alvs_error_stats_offsets {
	ALVS_ERROR_UNSUPPORTED_ROUTING_ALGO        = 0,
	ALVS_ERROR_CANT_EXPIRE_CONNECTION          = 1,
	ALVS_ERROR_CANT_UPDATE_CONNECTION_STATE    = 2,
	ALVS_ERROR_CONN_INFO_LKUP_FAIL             = 3,
	ALVS_ERROR_CONN_CLASS_ALLOC_FAIL           = 4,
	ALVS_ERROR_CONN_INFO_ALLOC_FAIL            = 5,
	ALVS_ERROR_CONN_INDEX_ALLOC_FAIL           = 6,
	ALVS_ERROR_SERVICE_CLASS_LKUP_FAIL         = 7,
	ALVS_ERROR_SCHEDULING_FAIL                 = 8,
	ALVS_ERROR_SERVER_INFO_LKUP_FAIL           = 9,
	ALVS_ERROR_SERVER_IS_UNAVAILABLE           = 10,
	ALVS_ERROR_SERVER_INDEX_LKUP_FAIL          = 11,
	ALVS_ERROR_CONN_CLASS_LKUP_FAIL            = 12,
	ALVS_ERROR_SERVICE_INFO_LOOKUP             = 13,
	ALVS_ERROR_UNSUPPORTED_SCHED_ALGO          = 14,
	ALVS_ERROR_CANT_MARK_DELETE                = 15,
	ALVS_ERROR_DEST_SERVER_IS_NOT_AVAIL        = 16,
	ALVS_ERROR_SEND_FRAME_FAIL                 = 17,
	ALVS_ERROR_CONN_MARK_TO_DELETE             = 18,
	ALVS_ERROR_SERVICE_CLASS_LOOKUP            = 19,
	ALVS_ERROR_UNSUPPORTED_PROTOCOL            = 20,
	ALVS_ERROR_NO_ACTIVE_SERVERS               = 21,
	ALVS_ERROR_CREATE_CONN_MEM_ERROR           = 22,
	ALVS_ERROR_STATE_SYNC                      = 23,
	ALVS_ERROR_STATE_SYNC_LOOKUP_FAIL          = 24,
	ALVS_ERROR_STATE_SYNC_BACKUP_DOWN          = 25,
	ALVS_ERROR_STATE_SYNC_BAD_HEADER_SIZE      = 26,
	ALVS_ERROR_STATE_SYNC_BACKUP_NOT_MY_SYNCID = 27,
	ALVS_ERROR_STATE_SYNC_BAD_BUFFER           = 28,
	ALVS_ERROR_STATE_SYNC_DECODE_CONN          = 29,
	ALVS_ERROR_STATE_SYNC_BAD_MESSAGE_VERSION  = 30,
	ALVS_NUM_OF_ALVS_ERROR_STATS               = 40 /* MUST BE EVEN! */
};

#define ALVS_CONN_LOCK_ELEMENTS_COUNT (256 * 1024)

/* Posted Statistics */
#define EMEM_ALVS_ERROR_STATS_POSTED_MSID   USER_POSTED_STATS_MSID
#define EMEM_ALVS_ERROR_STATS_POSTED_OFFSET 0x0
#define EMEM_SERVICE_STATS_POSTED_MSID      USER_POSTED_STATS_MSID
#define EMEM_SERVICE_STATS_POSTED_OFFSET    (EMEM_ALVS_ERROR_STATS_POSTED_OFFSET + ALVS_NUM_OF_ALVS_ERROR_STATS)
#define EMEM_SERVER_STATS_POSTED_MSID       USER_POSTED_STATS_MSID
#define EMEM_SERVER_STATS_POSTED_OFFSET     (EMEM_SERVICE_STATS_POSTED_OFFSET + ALVS_SERVICES_MAX_ENTRIES * ALVS_NUM_OF_SERVICE_STATS)
#define EMEM_IF_STATS_POSTED_MSID           USER_POSTED_STATS_MSID
#define EMEM_NW_IF_STATS_POSTED_OFFSET      (EMEM_SERVER_STATS_POSTED_OFFSET + ALVS_SERVERS_MAX_ENTRIES * ALVS_NUM_OF_SERVER_STATS)
#define EMEM_HOST_IF_STATS_POSTED_OFFSET    (EMEM_IF_STATS_POSTED_OFFSET + (USER_NW_IF_NUM*NW_NUM_OF_IF_STATS))
#define EMEM_IF_STATS_POSTED_OFFSET         EMEM_NW_IF_STATS_POSTED_OFFSET

#define EMEM_SPINLOCK_MSID            USER_EMEM_OUT_OF_BAND_MSID
#define EMEM_SPINLOCK_OFFSET          0x0

#define EMEM_SERVER_FLAGS_MSID		USER_EMEM_OUT_OF_BAND_MSID
#define EMEM_SERVER_FLAGS_OFFSET	(EMEM_SPINLOCK_OFFSET + ALVS_CONN_LOCK_ELEMENTS_COUNT)
#define EMEM_SERVER_FLAGS_OFFSET_CP	(EMEM_SPINLOCK_OFFSET + ALVS_CONN_LOCK_ELEMENTS_COUNT * 4) /*TODO - change to sizeof()*/

/*definition of long counters for server needs*/
#define EMEM_SERVER_STATS_ON_DEMAND_MSID USER_ON_DEMAND_STATS_MSID
#define EMEM_SERVER_STATS_ON_DEMAND_OFFSET 0x0

/*definition of long counters for TB flag - to know if red was set*/
#define EMEM_TB_FLAG_STATS_ON_DEMAND_OFFSET (ALVS_SERVERS_MAX_ENTRIES * ALVS_NUM_OF_SERVERS_ON_DEMAND_STATS)
#define EMEM_TB_FLAG_STATS_NUM              1


/*definition of TB counters - to know the color of message*/
#define EMEM_STATS_ON_DEMAND_COLOR_FLAG_OFFSET (ALVS_SERVERS_MAX_ENTRIES * ALVS_NUM_OF_SERVERS_ON_DEMAND_STATS)
#define EMEM_STATS_ON_DEMAND_COLOR_FLAG_NUM    1
#define EMEM_STATS_ON_DEMAND_TB_OFFSET         (EMEM_STATS_ON_DEMAND_COLOR_FLAG_OFFSET + EMEM_STATS_ON_DEMAND_COLOR_FLAG_NUM)
#define EMEM_STATS_ON_DEMAND_TB_STATS_NUM      1


#define ALVS_TB_PROFILE_0_CIR_RESOLUTION EZapiStat_TBProfileResolution_1_BYTE
#define ALVS_TB_PROFILE_0_CIR            0x40000000
#define ALVS_TB_PROFILE_0_CBS            0x40000000

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
	STRUCT_ID_ALVS_SERVER_CLASSIFICATION   = 10,
	STRUCT_ID_APPLICATION_INFO             = 11,
	NUM_OF_STRUCT_IDS
};


/* timer defines */
#define ALVS_AGING_TIMER_SCAN_ENTRIES_PER_JOB   128
#define ALVS_AGING_TIMER_EVENTS_PER_ITERATION   (ALVS_CONN_MAX_ENTRIES / ALVS_AGING_TIMER_SCAN_ENTRIES_PER_JOB)


#endif /* DEFS_H_ */
