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

#ifndef _NW_CONF_H_
#define _NW_CONF_H_

#include <ezdp_defs.h>


/*
 * Misc
 */
#define BUILD_SUM_ADDR(mem_space_type, msid, element_index) (((mem_space_type) << EZDP_SUM_ADDR_MEM_TYPE_OFFSET) | ((msid) << EZDP_SUM_ADDR_MSID_OFFSET) | ((element_index) << EZDP_SUM_ADDR_ELEMENT_INDEX_OFFSET))

/*
 * Interface configuration
 */

#define NW_IF_NUM                 4
#define NW_BASE_LOGICAL_ID        0
#define HOST_LOGICAL_ID           (NW_BASE_LOGICAL_ID + NW_IF_NUM)
#define REMOTE_BASE_LOGICAL_ID    5
#define REMOTE_IF_NUM             4


/* Local Host interface parameters */
#define INFRA_HOST_IF_SIDE          1
#define INFRA_HOST_IF_PORT          24

/* Network interface #0 parameters */
#define INFRA_NW_IF_0_SIDE          0
#define INFRA_NW_IF_0_PORT          0

/* Network interface #1 parameters */
#define INFRA_NW_IF_1_SIDE          0
#define INFRA_NW_IF_1_PORT          12

/* Network interface #2 parameters */
#define INFRA_NW_IF_2_SIDE          1
#define INFRA_NW_IF_2_PORT          0

/* Network interface #3 parameters */
#define INFRA_NW_IF_3_SIDE          1
#define INFRA_NW_IF_3_PORT          12

/* Remote interface #0 parameters */
#define INFRA_REMOTE_IF_0_SIDE      0
#define INFRA_REMOTE_IF_0_PORT      24

/* Remote interface #1 parameters */
#define INFRA_REMOTE_IF_1_SIDE      0
#define INFRA_REMOTE_IF_1_PORT     25

/* Remote interface #2 parameters */
#define INFRA_REMOTE_IF_2_SIDE     0
#define INFRA_REMOTE_IF_2_PORT     26

/* Remote interface #3 parameters */
#define INFRA_REMOTE_IF_3_SIDE     0
#define INFRA_REMOTE_IF_3_PORT     27


/*! interface configuration parameters possible values. */
enum infra_interface_params {
	INFRA_INTERFACE_PARAMS_SIDE             = 0,
	INFRA_INTERFACE_PARAMS_PORT             = 1,
	INFRA_NUM_OF_INTERFACE_PARAMS
};

/* Network Interfaces Array */
extern uint32_t network_interface_params[NW_IF_NUM][INFRA_NUM_OF_INTERFACE_PARAMS];

/* Remote Interfaces Array */
extern uint32_t remote_interface_params[REMOTE_IF_NUM][INFRA_NUM_OF_INTERFACE_PARAMS];

/*
 * External MSIDs
 */
enum nw_external_msid {
	NW_EMEM_SEARCH_MSID = 0,
	NW_EMEM_DATA_MSID,
	NW_POSTED_STATS_MSID,
	NW_ON_DEMAND_STATS_MSID,
	NUM_OF_NW_EXTERNAL_MSIDS
};




/*
 * Memory configuration
 */
#define NW_HALF_CLUSTER_SEARCH_SIZE    0
#define NW_1_CLUSTER_SEARCH_SIZE       3 /* need only 1.5KB */
#define NW_2_CLUSTER_SEARCH_SIZE       0
#define NW_4_CLUSTER_SEARCH_SIZE       0
#define NW_16_CLUSTER_SEARCH_SIZE      0
#define NW_ALL_CLUSTER_SEARCH_SIZE     0

#define NW_HALF_CLUSTER_DATA_SIZE      0
#define NW_1_CLUSTER_DATA_SIZE         0
#define NW_2_CLUSTER_DATA_SIZE         0
#define NW_4_CLUSTER_DATA_SIZE         0
#define NW_16_CLUSTER_DATA_SIZE        0
#define NW_ALL_CLUSTER_DATA_SIZE       0

#define NW_EMEM_SEARCH_SIZE            9
#define NW_EMEM_DATA_SIZE              1


/* EMEM data offsets */
#define SYSLOG_MUTEX_ARRAY_OFFSET      0x0




/*
 * Statistics configuration
 */

/* when adding an error, add to nw_if_posted_stats_offsets_names (error_names.h) the name of this error */
enum nw_if_posted_stats_offsets {
	NW_IF_STATS_FRAME_VALIDATION_FAIL   = 0,
	NW_IF_STATS_MAC_ERROR               = 1,
	NW_IF_STATS_IPV4_ERROR              = 2,
	NW_IF_STATS_NOT_MY_MAC              = 3,
	NW_IF_STATS_NOT_IPV4                = 4,
	NW_IF_STATS_NOT_SUPPORTED_PROTOCOL  = 5,
	NW_IF_STATS_NO_VALID_ROUTE          = 6,
	NW_IF_STATS_FAIL_ARP_LOOKUP         = 7,
	NW_IF_STATS_FAIL_INTERFACE_LOOKUP   = 8,
	NW_IF_STATS_FAIL_FIB_LOOKUP         = 9,
	NW_IF_STATS_REJECT_BY_FIB           = 10,
	NW_IF_STATS_UNKNOWN_FIB_RESULT	    = 11,
	NW_IF_STATS_FAIL_STORE_BUF          = 12,
	NW_IF_STATS_FAIL_GET_MAC_ADDR       = 13,
	NW_IF_STATS_FAIL_SET_ETH_HEADER     = 14,
	NW_IF_STATS_FAIL_LAG_GROUP_LOOKUP   = 15,
	NW_IF_STATS_DISABLE_LAG_GROUP_DROPS  = 16,
	NW_IF_STATS_DISABLE_IF_EGRESS_DROPS  = 17,
	NW_IF_STATS_DISABLE_IF_INGRESS_DROPS = 18,


	/*
	 * Note: 1. The following define must be at the end.
	 *       2. The following define must be even
	 */
	NW_NUM_OF_IF_STATS                  = 20
};

/* when adding an error, add to remote_if_posted_stats_offsets_names (error_names.h) the name of this error */
enum remote_if_posted_stats_offsets {
	REMOTE_IF_STATS_FRAME_VALIDATION_FAIL   = 0,
	REMOTE_IF_STATS_MAC_ERROR               = 1,
	REMOTE_IF_STATS_IPV4_ERROR              = 2,
	REMOTE_IF_STATS_NOT_MY_MAC              = 3,
	REMOTE_IF_STATS_NOT_IPV4                = 4,
	REMOTE_IF_STATS_NOT_TCP                 = 5,
	REMOTE_IF_STATS_NO_VALID_ROUTE          = 6,
	REMOTE_IF_STATS_FAIL_ARP_LOOKUP         = 7,
	REMOTE_IF_STATS_FAIL_INTERFACE_LOOKUP   = 8,
	REMOTE_IF_STATS_FAIL_FIB_LOOKUP         = 9,
	REMOTE_IF_STATS_REJECT_BY_FIB           = 10,
	REMOTE_IF_STATS_UNKNOWN_FIB_RESULT      = 11,
	REMOTE_IF_STATS_FAIL_STORE_BUF          = 12,


	/*
	 * Note: 1. The following define must be at the end.
	 *       2. The following define must be even
	 */
	REMOTE_NUM_OF_IF_STATS                  = 20
};

#define HOST_NUM_OF_IF_STATS                NW_NUM_OF_IF_STATS


/* Posted Statistics offsets */
#define NW_IF_STATS_POSTED_OFFSET      0x0
#define REMOTE_IF_STATS_POSTED_OFFSET  (NW_IF_STATS_POSTED_OFFSET + (NW_IF_NUM * NW_NUM_OF_IF_STATS))
#define HOST_IF_STATS_POSTED_OFFSET    (REMOTE_IF_STATS_POSTED_OFFSET + (REMOTE_IF_NUM * REMOTE_NUM_OF_IF_STATS))

#define NW_POSTED_STATS_LAST_OFFSET    (HOST_IF_STATS_POSTED_OFFSET + HOST_NUM_OF_IF_STATS)
#define NW_TOTAL_POSTED_STATS          (NW_IF_NUM * NW_NUM_OF_IF_STATS + REMOTE_IF_NUM * REMOTE_NUM_OF_IF_STATS + HOST_NUM_OF_IF_STATS)


/* On demand Statistics offsets */
#define SYSLOG_TB_STATS_ON_DEMAND_OFFSET           0x0
#define SYSLOG_NUM_OF_TB_STATS                     1
#define SYSLOG_COLOR_FLAG_STATS_ON_DEMAND_OFFSET   (SYSLOG_TB_STATS_ON_DEMAND_OFFSET + SYSLOG_NUM_OF_TB_STATS)
#define SYSLOG_NUM_OF_COLOR_FLAG_STATS             1

#define NW_ON_DENAMD_STATS_LAST_OFFSET             (SYSLOG_COLOR_FLAG_STATS_ON_DEMAND_OFFSET + SYSLOG_NUM_OF_COLOR_FLAG_STATS)
#define NW_TOTAL_ON_DEMAND_STATS                   2



/*
 * Structures configuration
 */
enum nw_struct_id {
	STRUCT_ID_NW_INTERFACES                = 0,
	STRUCT_ID_NW_LAG_GROUPS,
	STRUCT_ID_NW_ARP,
	STRUCT_ID_NW_INTERFACE_ADDRESSES,
	NUM_OF_NW_STRUCT_IDS
};


#endif /* _NW_CONF_H_ */
