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
*  Project:             NPS400 TC application
*  File:                tc_defs.h
*  Desc:                general defs for data path TC
*/

#ifndef TC_DEFS_H_
#define TC_DEFS_H_

#include "tc_search_defs.h"
#include "nw_defs.h"


/****************************************************************
 * General definitions
 ***************************************************************/

/*prototypes*/
bool init_tc_shared_cmem(void);
bool init_tc_private_cmem(void);
bool init_tc_emem(void);

/****************************************************************
 * TC definitions
 ***************************************************************/
#define TC_MAX_CONTINUE_RESULTS 8

#define TC_MAX_VALID_BIT ((1 << TC_MAX_CONTINUE_RESULTS)-1)

enum tc_action_rc {
	TC_ACTION_RC_CHECK_NEXT_ACTION,
	TC_ACTION_RC_PACKET_DROP,
	TC_ACTION_RC_PACKET_STOLEN,
	TC_ACTION_RC_ACCEPT,
	TC_ACTION_RC_RECLASSIFY,
	TC_ACTION_RC_CONTINUE
};

enum tc_processing_rc {
	TC_PROCESSING_RC_PACKET_STOLEN = TC_ACTION_RC_PACKET_STOLEN,
	TC_PROCESSING_RC_ACCEPT = TC_ACTION_RC_ACCEPT
};

#define TC_CACHE_FLOW_LOCK_ELEMENTS_MASK  (TC_FLOW_CACHE_LOCK_ELEMENTS_COUNT - 1)

/*************************************************************
 * SEARCH defs
 ************************************************************/

/***************** global CMEM data *************************/
struct classifier_match_entry {
	uint16_t              priority;
	uint32_t              filter_actions_index;
} __packed;

struct classifier_entry {
	struct tc_classifier_result result;
	union {
		union tc_classifier_key key;

		EZDP_PAD_HASH_ENTRY(sizeof(struct tc_classifier_result), sizeof(union tc_classifier_key));
	};
};

/***********************************************************************//**
 * \struct      tc_cmem
 * \brief       include all variables located on private CMEM
 **************************************************************************/
struct tc_cmem {
	union {
		union tc_classifier_key                classifier_key;
		/**< classifier key */
		struct tc_filter_actions_result        filter_action_idx_res;
		/**< filter action index result */
	};
	union {
#if 0
		struct tc_flow_cache_classifier_key     flow_cache_classifier_key;
		/**< flow cache classifier key */
#endif
		struct tc_action_result                 action_res;
		/**< action info result */
		struct tc_rules_list_result             rules_list_res;
		/**< rules list result */
	};
	union {
		struct tc_mask_result                   mask_res;
		/**< mask result */
		struct tc_timestamp_result              timestamp_res;
		/**< timestamp result */
		struct classifier_entry			class_entry;
		/**< classifier entry */
		struct tc_pedit_action_info_result      action_pedit_info_res;
		/**< pedit result */
#if 0
		struct tc_flow_cache_idx_result         flow_cache_idx_res;
		/**< flow cache index result */
		struct tc_flow_cache_classifier_result  flow_cache_classifier_res;
		/**< flow cache classifier result */
#endif
	};


#if 0
	ezdp_spinlock_t                         flow_cache_spinlock;
	/**< connection spinlock */
#endif
} __packed;


struct match_info {
	struct classifier_match_entry		best_match_array[TC_MAX_CONTINUE_RESULTS];
	/**< timestamp key */
	uint8_t					match_array_valid_bitmap;
	/**/
	uint8_t					highest_priority_index;

};
/***********************************************************************//**
 * \struct      tc_workarea
 * \brief       DP workarea for hash, direct tables, etc used by TC.
 *              temp variables located on private CMEM
 **************************************************************************/
union tc_workarea {
#if 0
	char flow_cache_classifier_hash_wa[EZDP_HASH_WORK_AREA_SIZE(sizeof(struct tc_flow_cache_classifier_result), sizeof(struct tc_flow_cache_classifier_key))];
	char flow_cache_idx_table_wa[EZDP_TABLE_WORK_AREA_SIZE(sizeof(struct tc_flow_cache_idx_result))];
#endif
	char classifier_prm_hash_wa[EZDP_HASH_PRM_WORK_AREA_SIZE];
	char action_info_table_wa[EZDP_TABLE_WORK_AREA_SIZE(sizeof(struct tc_action_result))];
	char filter_actions_index_table_wa[EZDP_TABLE_WORK_AREA_SIZE(sizeof(struct tc_filter_actions_result))];
	char timestamp_table_wa[EZDP_TABLE_WORK_AREA_SIZE(sizeof(struct tc_timestamp_result))];
	char mask_bitmap_table_wa[EZDP_TABLE_WORK_AREA_SIZE(sizeof(struct tc_mask_result))];
	char rules_list_table_wa[EZDP_TABLE_WORK_AREA_SIZE(sizeof(struct tc_rules_list_result))];
	char pedit_action_info_table_wa[EZDP_TABLE_WORK_AREA_SIZE(sizeof(struct tc_pedit_action_info_result))];
	uint64_t counter_work_area;
	struct ezdp_rtc  real_time_clock;
};

/***********************************************************************//**
 * \struct      tc_shared_cmem
 * \brief       all variables located on shared CMEM
 **************************************************************************/

struct tc_shared_cmem {
#if 0
	ezdp_hash_struct_desc_t     tc_flow_cache_classifier_struct_desc;
	ezdp_table_struct_desc_t    tc_flow_cache_idx_struct_desc;
#endif
	ezdp_hash_struct_desc_t     tc_classifier_struct_desc;
	ezdp_table_struct_desc_t    tc_action_info_struct_desc;
	ezdp_table_struct_desc_t    tc_filter_actions_index_struct_desc;
	ezdp_table_struct_desc_t    tc_mask_bitmap_struct_desc;
	ezdp_table_struct_desc_t    tc_timestamp_struct_desc;
	ezdp_table_struct_desc_t    tc_rules_list_struct_desc;
	ezdp_table_struct_desc_t    tc_pedit_action_info_struct_desc;
} __packed;

#endif /* _TC_DEFS_H_ */
