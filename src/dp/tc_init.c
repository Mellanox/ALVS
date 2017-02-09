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
*  File:                tc_init.c
*  Desc:                tc init process
*/

#include <stdbool.h>
#include "global_defs.h"
#include "tc_conf.h"
#include "tc_defs.h"


/******************************************************************************
 * \brief         Initialize tc shared CMEM constructor
 *
 * \return        true/false (success / failed )
 */
bool init_tc_shared_cmem(void)
{
	uint32_t  result;

	/*Init tc classifier DB*/
	result = ezdp_init_hash_struct_desc(STRUCT_ID_TC_CLASSIFIER,
					    &shared_cmem_tc.tc_classifier_struct_desc,
					    cmem_wa.tc_wa.classifier_hash_wa,
					    sizeof(cmem_wa.tc_wa.classifier_hash_wa));
	if (result != 0) {
		printf("ezdp_init_hash_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_TC_CLASSIFIER, result, ezdp_get_err_msg());
		return false;
	}

	result = ezdp_validate_hash_struct_desc(&shared_cmem_tc.tc_classifier_struct_desc,
						true,
						sizeof(union tc_classifier_key),
						sizeof(struct tc_classifier_result));
	if (result != 0) {
		printf("ezdp_validate_hash_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_TC_CLASSIFIER, result, ezdp_get_err_msg());
		return false;
	}

#if 0
	/*Init tc flow cache classifier DB*/
	result = ezdp_init_hash_struct_desc(STRUCT_ID_TC_FLOW_CACHE_CLASSIFIER,
					    &shared_cmem_tc.tc_flow_cache_classifier_struct_desc,
					    cmem_wa.tc_wa.flow_cache_classifier_hash_wa,
					    sizeof(cmem_wa.tc_wa.flow_cache_classifier_hash_wa));
	if (result != 0) {
		printf("ezdp_init_hash_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_TC_FLOW_CACHE_CLASSIFIER, result, ezdp_get_err_msg());
		return false;
	}

	result = ezdp_validate_hash_struct_desc(&shared_cmem_tc.tc_flow_cache_classifier_struct_desc,
						true,
						sizeof(struct tc_flow_cache_classifier_key),
						sizeof(struct tc_flow_cache_classifier_result));
	if (result != 0) {
		printf("ezdp_validate_hash_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_TC_FLOW_CACHE_CLASSIFIER, result, ezdp_get_err_msg());
		return false;
	}

	/*Init tc action DB*/
	result = ezdp_init_table_struct_desc(STRUCT_ID_TC_FLOW_CACHE_INDEX,
					     &shared_cmem_tc.tc_flow_cache_idx_struct_desc,
					     cmem_wa.tc_wa.flow_cache_idx_table_wa,
					     sizeof(cmem_wa.tc_wa.flow_cache_idx_table_wa));
	if (result != 0) {
		printf("ezdp_init_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_TC_FLOW_CACHE_INDEX, result, ezdp_get_err_msg());
		return false;
	}

	result = ezdp_validate_table_struct_desc(&shared_cmem_tc.tc_flow_cache_idx_struct_desc,
						 sizeof(struct tc_flow_cache_idx_result));
	if (result != 0) {
		printf("ezdp_validate_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_TC_FLOW_CACHE_INDEX, result, ezdp_get_err_msg());
		return false;
	}
#endif

	/*Init tc filter actions index DB*/
	result = ezdp_init_table_struct_desc(STRUCT_ID_TC_FILTER_ACTIONS,
					     &shared_cmem_tc.tc_filter_actions_index_struct_desc,
					     cmem_wa.tc_wa.filter_actions_index_table_wa,
					     sizeof(cmem_wa.tc_wa.filter_actions_index_table_wa));
	if (result != 0) {
		printf("ezdp_init_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_TC_FILTER_ACTIONS, result, ezdp_get_err_msg());
		return false;
	}

	result = ezdp_validate_table_struct_desc(&shared_cmem_tc.tc_filter_actions_index_struct_desc,
						 sizeof(struct tc_filter_actions_result));
	if (result != 0) {
		printf("ezdp_validate_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_TC_FILTER_ACTIONS, result, ezdp_get_err_msg());
		return false;
	}

	/*Init tc action DB*/
	result = ezdp_init_table_struct_desc(STRUCT_ID_TC_ACTION_INFO,
					     &shared_cmem_tc.tc_action_info_struct_desc,
					     cmem_wa.tc_wa.action_info_table_wa,
					     sizeof(cmem_wa.tc_wa.action_info_table_wa));
	if (result != 0) {
		printf("ezdp_init_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_TC_ACTION_INFO, result, ezdp_get_err_msg());
		return false;
	}

	result = ezdp_validate_table_struct_desc(&shared_cmem_tc.tc_action_info_struct_desc,
						 sizeof(struct tc_action_result));
	if (result != 0) {
		printf("ezdp_validate_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_TC_ACTION_INFO, result, ezdp_get_err_msg());
		return false;
	}

	/*Init tc rules list DB*/
	result = ezdp_init_table_struct_desc(STRUCT_ID_TC_RULES_LIST,
					     &shared_cmem_tc.tc_rules_list_struct_desc,
					     cmem_wa.tc_wa.rules_list_table_wa,
					     sizeof(cmem_wa.tc_wa.rules_list_table_wa));
	if (result != 0) {
		printf("ezdp_init_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_TC_RULES_LIST, result, ezdp_get_err_msg());
		return false;
	}

	result = ezdp_validate_table_struct_desc(&shared_cmem_tc.tc_rules_list_struct_desc,
						 sizeof(struct tc_rules_list_result));
	if (result != 0) {
		printf("ezdp_validate_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_TC_RULES_LIST, result, ezdp_get_err_msg());
		return false;
	}

	/*Init tc mask DB*/
	result = ezdp_init_table_struct_desc(STRUCT_ID_TC_MASK_BITMAP,
					     &shared_cmem_tc.tc_mask_bitmap_struct_desc,
					     cmem_wa.tc_wa.mask_bitmap_table_wa,
					     sizeof(cmem_wa.tc_wa.mask_bitmap_table_wa));
	if (result != 0) {
		printf("ezdp_init_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_TC_MASK_BITMAP, result, ezdp_get_err_msg());
		return false;
	}

	result = ezdp_validate_table_struct_desc(&shared_cmem_tc.tc_mask_bitmap_struct_desc,
						 sizeof(struct tc_mask_result));
	if (result != 0) {
		printf("ezdp_validate_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_TC_MASK_BITMAP, result, ezdp_get_err_msg());
		return false;
	}


	/*Init tc pedit action info DB*/
	result = ezdp_init_table_struct_desc(STRUCT_ID_TC_ACTION_EXTRA_INFO,
					     &shared_cmem_tc.tc_pedit_action_info_struct_desc,
					     cmem_wa.tc_wa.pedit_action_info_table_wa,
					     sizeof(struct tc_pedit_action_info_result));
	if (result != 0) {
		printf("ezdp_init_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_TC_ACTION_EXTRA_INFO, result, ezdp_get_err_msg());
		return false;
	}

	result = ezdp_validate_table_struct_desc(&shared_cmem_tc.tc_pedit_action_info_struct_desc,
						 sizeof(struct tc_pedit_action_info_result));
	if (result != 0) {
		printf("ezdp_validate_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_TC_ACTION_EXTRA_INFO, result, ezdp_get_err_msg());
		return false;
	}


	/*Init tc timestamp DB*/
	result = ezdp_init_table_struct_desc(STRUCT_ID_TC_TIMESTAMPS,
					     &shared_cmem_tc.tc_timestamp_struct_desc,
					     cmem_wa.tc_wa.timestamp_table_wa,
					     sizeof(cmem_wa.tc_wa.timestamp_table_wa));
	if (result != 0) {
		printf("ezdp_init_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_TC_TIMESTAMPS, result, ezdp_get_err_msg());
		return false;
	}

	result = ezdp_validate_table_struct_desc(&shared_cmem_tc.tc_timestamp_struct_desc,
						 sizeof(struct tc_timestamp_result));
	if (result != 0) {
		printf("ezdp_validate_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_TC_TIMESTAMPS, result, ezdp_get_err_msg());
		return false;
	}

	return true;
}


/******************************************************************************
 * \brief         Initialize tc private CMEM constructor
 *
 * \return        true/false (success / failed )
 */
bool init_tc_private_cmem(void)
{
#if 0
	cmem_tc.flow_cache_spinlock.addr.mem_type = EZDP_EXTERNAL_MS;
	cmem_tc.flow_cache_spinlock.addr.msid     = 1; /*ALVS_EMEM_DATA_OUT_OF_BAND_MSID*/;
#endif
	return true;
}

bool init_tc_emem(void)
{
#if 0
	struct ezdp_ext_addr	addr;
	uint32_t id;
	ezdp_spinlock_t flow_cache_spinlock;

	ezdp_mem_set(&addr, 0x0, sizeof(struct ezdp_ext_addr));
	addr.mem_type = EZDP_EXTERNAL_MS;
	addr.msid     = 1;/*ALVS_EMEM_DATA_OUT_OF_BAND_MSID*/;

	for (id = 0; id < TC_FLOW_CACHE_LOCK_ELEMENTS_COUNT; id++) {
		ezdp_init_spinlock_ext_addr(&flow_cache_spinlock, &addr);
		addr.address++;
	}
#endif
	return true;
}
