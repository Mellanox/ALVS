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
*  File:                tc_action.h
*  Desc:                action handling for tc
*/

#ifndef TC_ACTION_H_
#define TC_ACTION_H_

#include <linux/ip.h>
#include <linux/tcp.h>

#include "tc_utils.h"
#include "tc_action_pedit.h"


/******************************************************************************
 * \brief       lookup in filter actions index table
 *
 * \return      return 0 in case of success, otherwise no match.
 */
static __always_inline
uint32_t tc_filter_actions_index_lookup(uint32_t filter_actions_index)
{
	return ezdp_lookup_table_entry(&shared_cmem_tc.tc_filter_actions_index_struct_desc,
				       filter_actions_index, &cmem_tc.filter_action_idx_res,
				       sizeof(struct tc_filter_actions_result), 0);

}



/******************************************************************************
 * \brief       lookup in action index table
 *
 * \return      return 0 in case of success, otherwise no match.
 */
static __always_inline
uint32_t tc_action_index_lookup(uint32_t action_index)
{
	return ezdp_lookup_table_entry(&shared_cmem_tc.tc_action_info_struct_desc,
				       action_index, &cmem_tc.action_res,
				       sizeof(struct tc_action_result), 0);

}

/******************************************************************************
 * \brief       perform mirred action operations
 *
 * \return
 */
static __always_inline
enum tc_action_rc tc_action_handle_mirred(ezframe_t __cmem * frame,
					  uint8_t          *frame_base)
{
	anl_write_log(LOG_DEBUG, "MIRRED Action: type %d output_if %d is_lag %d",
		      cmem_tc.action_res.action_type,
		      cmem_tc.action_res.action_data.mirred.output_if,
		      cmem_tc.action_res.action_data.mirred.is_output_lag);

	switch (cmem_tc.action_res.action_type) {
	case TC_ACTION_TYPE_MIRRED_EGR_REDIRECT:
	{
		uint32_t frame_buff_size;

		/* Store frame (in case frame changed) */
		frame_buff_size = ezframe_get_buf_len(frame);
		ezframe_store_buf(frame, frame_base, frame_buff_size, 0);

		/* Redirect frame */
		nw_direct_route(frame, frame_base,
				cmem_tc.action_res.action_data.mirred.output_if,
				cmem_tc.action_res.action_data.mirred.is_output_lag);

		return TC_ACTION_RC_PACKET_STOLEN;
	}
	default:
	{
		anl_write_log(LOG_ERR, "got unknown mirred type %d",
			      cmem_tc.action_res.action_type);
		tc_discard_frame();
		return TC_ACTION_RC_PACKET_STOLEN;
	}
	}

}


/******************************************************************************
 * \brief       perform other action operations
 *
 * \return
 */
static __always_inline
enum tc_action_rc tc_action_handle_other_actions(ezframe_t __cmem * frame __unused,
						 uint8_t          *frame_basem __unused)
{
	switch (cmem_tc.action_res.action_type) {
	case TC_ACTION_TYPE_IFE:
	{
		break;
	}
	default:
	{
		break;
	}
	}

	return TC_ACTION_RC_ACCEPT;
}

/******************************************************************************
 * \brief       perform general action operations
 *
 * \return	if need to move to next action or current action type
 */
static __always_inline
enum tc_action_rc tc_action_handle_gact(enum tc_action_type action_type)
{
	switch (action_type) {
	case TC_ACTION_TYPE_GACT_OK:
	{
		anl_write_log(LOG_DEBUG, "got general or control action OK");
		return TC_ACTION_RC_ACCEPT;
	}
	case TC_ACTION_TYPE_GACT_DROP:
	{
		anl_write_log(LOG_DEBUG, "got general or control action DROP");
		tc_discard_frame();
		return TC_ACTION_RC_PACKET_STOLEN;
	}
	case TC_ACTION_TYPE_GACT_RECLASSIFY:
	{
		anl_write_log(LOG_DEBUG, "got general or control action RECLASSIFY");
		return TC_ACTION_RC_RECLASSIFY;
	}
	case TC_ACTION_TYPE_GACT_CONTINUE:
	{
		anl_write_log(LOG_DEBUG, "got general or control action CONTINUE");
		return TC_ACTION_RC_CONTINUE;
	}
	case TC_ACTION_TYPE_GACT_PIPE:
	{
		anl_write_log(LOG_DEBUG, "got general or control action PIPE");
		return TC_ACTION_RC_CHECK_NEXT_ACTION;
	}
	default:
	{
		anl_write_log(LOG_ERR, "got unknown general action 0x%02x",
			      cmem_tc.action_res.action_type);
		tc_discard_frame();
		return TC_ACTION_RC_PACKET_STOLEN;
	}
	}
}


/******************************************************************************
 * \brief       get highest priority match and perform its action
 *
 * \return      return 0 in case of success, otherwise no match.
 */
static __always_inline
enum tc_action_rc tc_action_handle_highest_priority(ezframe_t __cmem * frame,
						    uint8_t *frame_base,
						    struct match_info *match_info,
						    struct iphdr  *ip_hdr)
{
	enum tc_action_rc	rc;
	uint32_t		filter_actions_index = match_info->best_match_array[match_info->highest_priority_index].filter_actions_index;

	do {
		anl_write_log(LOG_DEBUG, "handle filter actions index %d, action priority %d",
			      filter_actions_index,
			      match_info->best_match_array[match_info->highest_priority_index].priority);

		/************************************************/
		/* lookup filters action index                  */
		/************************************************/
		if (tc_filter_actions_index_lookup(filter_actions_index) != 0) {
			tc_discard_frame();
			anl_write_log(LOG_ERR, "fail lookup of filters action index %d", filter_actions_index);
			return TC_ACTION_RC_PACKET_STOLEN;
		}

		uint8_t	i = 0;

		uint32_t action_index = cmem_tc.filter_action_idx_res.action_index_array[i];

		do {

			anl_write_log(LOG_DEBUG, "handle action index %d",
				      action_index);

			/************************************************/
			/* lookup action index                          */
			/************************************************/
			if (tc_action_index_lookup(action_index) != 0) {
				tc_discard_frame();
				anl_write_log(LOG_ERR, "fail lookup of action index %d", action_index);
				return TC_ACTION_RC_PACKET_STOLEN;
			}

			anl_write_log(LOG_DEBUG, "match action index %d: type %s",
				      action_index,
				      (cmem_tc.action_res.action_type & TC_ACTION_FAMILY_MASK) == TC_ACTION_TYPE_GACT_FAMILY ? "GACT" :
				      (cmem_tc.action_res.action_type & TC_ACTION_FAMILY_MASK) == TC_ACTION_TYPE_MIRRED_FAMILY ? "MIRRED" :
				      (cmem_tc.action_res.action_type & TC_ACTION_FAMILY_MASK) == TC_ACTION_TYPE_PEDIT_FAMILY ? "PEDIT" : "other");

			/************************************************/
			/* common handle:                               */
			/* Update stat & timestamp                      */
			/************************************************/
			tc_utils_update_action_stats();
			tc_utils_update_action_timestamp(action_index);

			/************************************************/
			/* handle action                                */
			/************************************************/
			if ((cmem_tc.action_res.action_type & TC_ACTION_FAMILY_MASK) == TC_ACTION_TYPE_GACT_FAMILY) {
				rc = tc_action_handle_gact(cmem_tc.action_res.action_type);
				if (rc != TC_ACTION_RC_CHECK_NEXT_ACTION) {
					return rc;
				}
			} else if ((cmem_tc.action_res.action_type & TC_ACTION_FAMILY_MASK) == TC_ACTION_TYPE_MIRRED_FAMILY) {
				return tc_action_handle_mirred(frame,  frame_base);
			} else if ((cmem_tc.action_res.action_type & TC_ACTION_FAMILY_MASK) == TC_ACTION_TYPE_PEDIT_FAMILY) {
				anl_write_log(LOG_DEBUG, "PEDIT enter");
				rc = tc_action_handle_pedit(frame, frame_base, ip_hdr, cmem_tc.action_res.action_data.action_extra_info_index);
				if (rc != TC_ACTION_RC_PACKET_STOLEN) {
					/*handle optional control*/
					rc = tc_action_handle_gact(cmem_tc.action_res.control);
					if (rc != TC_ACTION_RC_CHECK_NEXT_ACTION) {
						return rc;
					}
				}
			} else {
				anl_write_log(LOG_DEBUG, "Other actions");
				rc = tc_action_handle_other_actions(frame,  frame_base);
				if (rc != TC_ACTION_RC_CHECK_NEXT_ACTION) {
					return rc;
				}
			}

			i++;
			action_index = cmem_tc.filter_action_idx_res.action_index_array[i];
		} while (action_index != TC_ACTION_INVALID_VALUE); /*continue to loop over all actions*/

		filter_actions_index = cmem_tc.filter_action_idx_res.next_filter_actions_index;
	} while (cmem_tc.filter_action_idx_res.is_next_filter_actions_index_valid);

	anl_write_log(LOG_DEBUG, "finish loop over all actions. rc = %d", rc);
	return TC_ACTION_RC_ACCEPT;
}

#endif /* TC_ACTION_H_ */
