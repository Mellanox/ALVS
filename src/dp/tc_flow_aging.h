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
*  File:                tc_flow_aging.h
*  Desc:                handle aging functionality
*/

#ifndef TC_FLOW_AGING_H_
#define TC_FLOW_AGING_H_

#include "tc_defs.h"
#include "tc_utils.h"

/******************************************************************************
 * \brief        *
 * \return
 */

#if 0
static __always_inline
void tc_flow_cache_delete(uint32_t flow_index)
{
	ezdp_hashed_key_t hash_value;

	/*lock flow*/
	tc_lock_cache_flow(&hash_value);

	/*verify control bits is set to zero*/
	cmem_tc.flow_cache_idx_res.flow_cache_classifier_key.zero_rsrv1 = 0;

	/*1st remove the classification entry*/
	if (ezdp_delete_hash_entry(&shared_cmem_tc.tc_flow_cache_classifier_struct_desc,
				   &cmem_tc.flow_cache_idx_res.flow_cache_classifier_key,
				   sizeof(struct tc_flow_cache_classifier_key),
				   0,
				   cmem_wa.tc_wa.flow_cache_classifier_hash_wa,
				   sizeof(cmem_wa.tc_wa.flow_cache_classifier_hash_wa)) != 0) {
		return;
	}

	/*now remove the flow cache info entry*/
	if (ezdp_delete_table_entry(&shared_cmem_tc.tc_flow_cache_idx_struct_desc,
				    flow_index,
				    0,
				    cmem_wa.tc_wa.flow_cache_idx_table_wa,
				    sizeof(cmem_wa.tc_wa.flow_cache_idx_table_wa)) != 0) {
		return;
	}

	ezdp_free_index(TC_FLOW_CACHE_IDX_POOL_ID, flow_index);

	/*TODO - update global flow counter*/

	/*unlock*/
	tc_unlock_cache_flow(hash_value);
}

/******************************************************************************
 * \brief
 * \return
 */
static __always_inline
void tc_flow_cache_age_out(uint32_t flow_index)
{
	uint32_t rc;
	ezdp_hashed_key_t hash_value;

	/*lock connection*/
	tc_lock_cache_flow(&hash_value);

	/*perform another lookup to prevent race conditions*/
	rc = tc_lookup_flow_cache_idx(flow_index);

	if (rc != 0) {
		tc_unlock_cache_flow(hash_value);
		return;
	}

	/* turn off the aging bit */
	cmem_tc.flow_cache_idx_res.aging_bit = 0;

	rc = ezdp_modify_table_entry(&shared_cmem_tc.tc_flow_cache_idx_struct_desc,
			flow_index,
			&cmem_tc.flow_cache_idx_res,
			sizeof(struct tc_flow_cache_idx_result),
			EZDP_UNCONDITIONAL,
			cmem_wa.tc_wa.flow_cache_idx_table_wa,
			sizeof(cmem_wa.tc_wa.flow_cache_idx_table_wa));

	/*unlock*/
	tc_unlock_cache_flow(hash_value);
}



/******************************************************************************
 * \brief         perform aging on flow cache entries
 * \return        void
 */
static __always_inline
void tc_handle_flow_cache_aging_event(uint32_t event_id)
{
	uint32_t flow_index = event_id;

	/*discard timer job now, to be able to send sync state frame during this part*/
	tc_discard_frame();

	if (tc_lookup_flow_cache_idx(flow_index) == 0) {
		ezdp_mem_copy(&cmem_tc.flow_cache_classifier_key,
			      &cmem_tc.flow_cache_idx_res.flow_cache_classifier_key,
			      sizeof(struct tc_flow_cache_classifier_key));
		if (cmem_tc.flow_cache_idx_res.aging_bit == 1) {
			tc_flow_cache_age_out(flow_index);
		} else {
			tc_flow_cache_delete(flow_index);
		}
	}
}

#endif
#endif	/*TC_FLOW_AGING_H_*/
