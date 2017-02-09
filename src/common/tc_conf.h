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

#ifndef _TC_CONF_H_
#define _TC_CONF_H_

#include "nw_conf.h"


/*
 * External MSIDs
 */
enum tc_external_msid {
	TC_POSTED_STATS_MSID = NW_POSTED_STATS_MSID,
	TC_ON_DEMAND_STATS_MSID = NW_ON_DEMAND_STATS_MSID,
	TC_EMEM_SEARCH_0_MSID = NUM_OF_NW_EXTERNAL_MSIDS,
	NUM_OF_TC_EXTERNAL_MSIDS
};




/*
 * Memory configuration
 */


#define TC_FLOW_CACHE_LOCK_ELEMENTS_COUNT	(256 * 1024)

#ifdef CONFIG_TC
#	define TC_HALF_CLUSTER_SEARCH_SIZE    0
#	define TC_1_CLUSTER_SEARCH_SIZE       7    /* TODO - Can be reduced */
#	define TC_2_CLUSTER_SEARCH_SIZE       0
#	define TC_4_CLUSTER_SEARCH_SIZE       0
#	define TC_16_CLUSTER_SEARCH_SIZE      0
#	define TC_ALL_CLUSTER_SEARCH_SIZE     0

#	define TC_HALF_CLUSTER_DATA_SIZE      0
#	define TC_1_CLUSTER_DATA_SIZE         1
#	define TC_2_CLUSTER_DATA_SIZE         0
#	define TC_4_CLUSTER_DATA_SIZE         0
#	define TC_16_CLUSTER_DATA_SIZE        0
#	define TC_ALL_CLUSTER_DATA_SIZE       1
#	define TC_EMEM_SEARCH_0_SIZE          3150
#else
#	define TC_HALF_CLUSTER_SEARCH_SIZE    0
#	define TC_1_CLUSTER_SEARCH_SIZE       0
#	define TC_2_CLUSTER_SEARCH_SIZE       0
#	define TC_4_CLUSTER_SEARCH_SIZE       0
#	define TC_16_CLUSTER_SEARCH_SIZE      0
#	define TC_ALL_CLUSTER_SEARCH_SIZE     0

#	define TC_HALF_CLUSTER_DATA_SIZE      0
#	define TC_1_CLUSTER_DATA_SIZE         0
#	define TC_2_CLUSTER_DATA_SIZE         0
#	define TC_4_CLUSTER_DATA_SIZE         0
#	define TC_16_CLUSTER_DATA_SIZE        0
#	define TC_ALL_CLUSTER_DATA_SIZE       0
#endif


#define TC_NUM_OF_INTERFACES           4		/* TODO CHANGE */
#define TC_NUM_OF_ETHERTYPES           2		/* TODO CHANGE*/
#define TC_MAX_NUM_OF_MASKS_SUPPORTED  32
#define TC_MASKS_TABLE_SIZE            (TC_NUM_OF_INTERFACES*TC_NUM_OF_ETHERTYPES*TC_MAX_NUM_OF_MASKS_SUPPORTED)

#define TC_CLASSIFICATION_TABLE_SIZE   (64*1024)

#define TC_FILTER_RULES_TABLE_SIZE     TC_CLASSIFICATION_TABLE_SIZE
#define TC_FILTER_ACTIONS_TABLE_SIZE   TC_CLASSIFICATION_TABLE_SIZE

#define TC_NUM_OF_ACTIONS              (TC_CLASSIFICATION_TABLE_SIZE*32) /*todo change to define*/
#define TC_ACTIONS_TABLE_SIZE          TC_NUM_OF_ACTIONS
#define TC_ACTIONS_EXTRA_INFO_TABLE_SIZE TC_NUM_OF_ACTIONS
#define TC_ACTION_TIMESTAMP_TABLE_SIZE TC_NUM_OF_ACTIONS


/*
 * Statistics configuration
 */
enum tc_action_on_demand_stats_offsets {
	TC_ACTION_IN_PACKET,
	TC_ACTION_IN_BYTES,
	TC_NUM_OF_ACTION_ON_DEMAND_STATS
};

enum tc_error_stats_offsets {

	TC_NUM_OF_TC_ERROR_STATS                 = 40 /* LAST DEFINE - MUST BE EVEN! */
};


#define TC_TOTAL_POSTED_STATS    0

/* On demand Statistics offsets */
#define TC_ACTION_STATS_ON_DEMAND_OFFSET           NW_ON_DENAMD_STATS_LAST_OFFSET
#ifdef CONFIG_TC
#	define TC_TOTAL_ON_DEMAND_STATS            (TC_NUM_OF_ACTIONS * TC_NUM_OF_ACTION_ON_DEMAND_STATS)
#else
#	define TC_TOTAL_ON_DEMAND_STATS            0
#endif

/*
 * Structures configuration
 */
enum tc_struct_id {

	STRUCT_ID_TC_CLASSIFIER     = NUM_OF_NW_STRUCT_IDS,
	STRUCT_ID_TC_ACTION_INFO,
	STRUCT_ID_TC_MASK_BITMAP,
	STRUCT_ID_TC_TIMESTAMPS,
	STRUCT_ID_TC_RULES_LIST,
	STRUCT_ID_TC_FLOW_CACHE_CLASSIFIER,
	STRUCT_ID_TC_FLOW_CACHE_INDEX,
	STRUCT_ID_TC_ACTION_EXTRA_INFO,
	STRUCT_ID_TC_FILTER_ACTIONS,
	NUM_OF_TC_STRUCT_IDS
};


/* Timer configuration */
#define TC_TIMER_LOGICAL_ID                   96


/* Index pool configuration */

/* Index pool configuration */
enum tc_index_pool_id {
	TC_FLOW_CACHE_CLASSIFIER_HASH_SIG_PAGE_POOL_ID = 0,
	TC_FLOW_CACHE_CLASSIFIER_HASH_RESULT_POOL_ID,
	TC_FLOW_CACHE_IDX_POOL_ID,
	NUM_OF_TC_INDEX_POOL_IDS
};

#endif /* _TC_CONF_H_ */
