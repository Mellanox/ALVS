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
*  File:                tc_utils.h
*  Desc:                function utilities for tc - stats and timestamps, learn new flows, locks
*/

#ifndef TC_UTILS_H_
#define TC_UTILS_H_

#include <linux/ip.h>
#include <linux/tcp.h>
#include "tc_defs.h"
#include "tc_conf.h"


/******************************************************************************
 * \brief         perform a lock on cache flow ( 10 tuple) using DP spinlock
 *
 * \return        0 - success
 */
#if 0
static __always_inline
void tc_lock_cache_flow(ezdp_hashed_key_t *hash_value)
{
	/*calc hash key for lock*/
	*hash_value = ezdp_bulk_hash((uint8_t *)&cmem_tc.flow_cache_classifier_key, sizeof(struct tc_flow_cache_classifier_key));
	cmem_tc.flow_cache_spinlock.addr.address = *hash_value & TC_CACHE_FLOW_LOCK_ELEMENTS_MASK;
	anl_write_log(LOG_DEBUG, "cmem_tc.conn_spinlock.addr.address = 0x%x", cmem_tc.flow_cache_spinlock.addr.address);
	ezdp_lock_spinlock(&cmem_tc.flow_cache_spinlock);
}
#endif
/******************************************************************************
 * \brief      perform a try lock on flow cache ( 10 tuple) using DP spinlock
 *
 * \return     0 - success
 *             1 - failure
 */
#if 0
static __always_inline
uint32_t tc_try_lock_cache_flow(ezdp_hashed_key_t *hash_value)
{
	/*calc hash key for lock*/
	*hash_value = ezdp_bulk_hash((uint8_t *)&cmem_tc.flow_cache_classifier_key, sizeof(struct tc_flow_cache_classifier_key));
	cmem_tc.flow_cache_spinlock.addr.address = *hash_value & TC_CACHE_FLOW_LOCK_ELEMENTS_MASK;
	return ezdp_try_lock_spinlock(&cmem_tc.flow_cache_spinlock);
}

/******************************************************************************
 * \brief      unlock flow cache ( 10 tuple) using DP spinlock
 *
 * \return     0 - success
 *             2 - try to unlock twice
 */
static __always_inline
uint32_t tc_unlock_cache_flow(ezdp_hashed_key_t hash_value)
{
	cmem_tc.flow_cache_spinlock.addr.address = hash_value & TC_CACHE_FLOW_LOCK_ELEMENTS_MASK;
	return ezdp_unlock_spinlock(&cmem_tc.flow_cache_spinlock);
}
#endif
/******************************************************************************
 * \brief
 * \return
 */
#if 0
static __always_inline
void tc_learn_new_flow_cache(void)
{
	uint32_t flow_idx;
	uint32_t rc;
	ezdp_hashed_key_t hash_value;
	uint32_t found_result_size;
	struct  tc_flow_cache_classifier_result *flow_cache_classifier_result_ptr;

	/*lock connection*/
	tc_lock_cache_flow(&hash_value);

	/*TODO check dirty bit before starting*/

	/*perform another lookup to check if another thread already created this flow - assumption flow cache key is already set*/
	rc = ezdp_lookup_hash_entry(&shared_cmem_tc.tc_flow_cache_classifier_struct_desc,
				    (void *)&cmem_tc.flow_cache_classifier_key,
				    sizeof(struct tc_flow_cache_classifier_key),
				    (void **)&flow_cache_classifier_result_ptr,
				    &found_result_size, 0,
				    cmem_wa.tc_wa.flow_cache_classifier_hash_wa,
				    sizeof(cmem_wa.tc_wa.flow_cache_classifier_hash_wa));

	if (rc == 0) {
		tc_unlock_cache_flow(hash_value);
		return;
	}

	/*allocate new index*/
	flow_idx = ezdp_alloc_index(TC_FLOW_CACHE_IDX_POOL_ID);
	if (flow_idx == EZDP_NULL_INDEX) {
		tc_unlock_cache_flow(hash_value);
		return;
	}

	/*build flow cache result*/
	cmem_tc.flow_cache_idx_res.action_index = cmem_tc.best_match_array[cmem_tc.highest_priority_index].action_index;
	cmem_tc.flow_cache_idx_res.priority = cmem_tc.best_match_array[cmem_tc.highest_priority_index].priority;
	cmem_tc.flow_cache_idx_res.rules_list_index = cmem_tc.best_match_array[cmem_tc.highest_priority_index].rules_list_index;
	cmem_tc.flow_cache_idx_res.aging_bit = 1;
	ezdp_mem_copy(&cmem_tc.flow_cache_idx_res.flow_cache_classifier_key,
		      &cmem_tc.flow_cache_classifier_key,
		      sizeof(struct tc_flow_cache_classifier_key));

	/*first create connection info entry*/
	(void)ezdp_add_table_entry(&shared_cmem_tc.tc_flow_cache_idx_struct_desc,
			     flow_idx,
			     &cmem_tc.flow_cache_idx_res,
			     sizeof(struct tc_flow_cache_idx_result),
			     EZDP_UNCONDITIONAL,
			     cmem_wa.tc_wa.flow_cache_idx_table_wa,
			     sizeof(cmem_wa.tc_wa.flow_cache_idx_table_wa));

	/*TODO build key & res*/

	cmem_tc.flow_cache_classifier_res.flow_cache_index = flow_idx;

	rc = ezdp_add_hash_entry(&shared_cmem_tc.tc_flow_cache_classifier_struct_desc,
				 &cmem_tc.flow_cache_classifier_key,
				 sizeof(struct tc_flow_cache_classifier_key),
				 &cmem_tc.flow_cache_classifier_res,
				 sizeof(struct tc_flow_cache_classifier_result),
				 EZDP_UNCONDITIONAL,
				 cmem_wa.tc_wa.flow_cache_classifier_hash_wa,
				 sizeof(cmem_wa.tc_wa.flow_cache_classifier_hash_wa));

	if (rc != 0) {
		/*rollback scenario in case of failure*/
		(void)ezdp_delete_table_entry(&shared_cmem_tc.tc_flow_cache_idx_struct_desc,
					    flow_idx,
					    0,
					    cmem_wa.tc_wa.flow_cache_idx_table_wa,
					    sizeof(cmem_wa.tc_wa.flow_cache_idx_table_wa));

		ezdp_free_index(TC_FLOW_CACHE_IDX_POOL_ID, flow_idx);
	}

	/*TODO - update global flow counter*/

	tc_unlock_cache_flow(hash_value);
}

#endif
/******************************************************************************
 * \brief         discard frame due to unexpected error or drop action
 * \return        void
 */
static __always_inline
void tc_discard_frame(void)
{
	/*drop frame*/
	ezframe_free(&frame, 0);
}

/******************************************************************************
 * \brief
 *
 * \return
 */
static __always_inline
void tc_utils_update_action_stats(uint16_t packet_length)
{
	/*update packets and bytes per action*/
	anl_write_log(LOG_DEBUG, "STATS cmem_tc.action_res.action_stats_addr=%08x", cmem_tc.action_res.action_stats_addr);

	ezdp_inc_dual_ctr_async(cmem_tc.action_res.action_stats_addr,
				packet_length,
				1);


}

/******************************************************************************
 * \brief
 *
 * \return
 */
static __always_inline
void tc_utils_update_action_timestamp(uint32_t action_index)
{
	uint32_t rc __unused;

	/* get time form real time cloce */
	rc = ezdp_get_real_time_clock(&cmem_wa.tc_wa.real_time_clock);

	/* update entry in timestamp table */
	/* TODO: check if rc from ezdp_get_real_time_clock represent second, if it does, used it for better performance */
	cmem_tc.timestamp_res.tc_timestamp_val = cmem_wa.tc_wa.real_time_clock.sec;
	rc = ezdp_modify_table_entry(&shared_cmem_tc.tc_timestamp_struct_desc,
				     action_index,
				     &cmem_tc.timestamp_res,
				     sizeof(cmem_tc.timestamp_res),
				     EZDP_UNCONDITIONAL,
				     cmem_wa.tc_wa.timestamp_table_wa,
				     sizeof(cmem_wa.tc_wa.timestamp_table_wa));

#ifndef NDEBUG
	if (rc != 0) {
		anl_write_log(LOG_WARNING, "ezdp_modify_table_entry action_index 0x%x", action_index);
	}
#endif  /* ndef NDEBUG */
}

#endif /* TC_UTILS_H_ */
