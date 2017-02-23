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
*  File:                tc_packet_processing.h
*  Desc:                packet processing of tc
*/

#ifndef TC_PACKET_PROCESSING_H_
#define TC_PACKET_PROCESSING_H_

#include <stdbool.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include "tc_defs.h"
#include "global_defs.h"
#include "nw_routing.h"
#include "tc_action.h"
#include "performance_trace.h"

extern uint32_t tc_bitmask_array[32];

/*prototypes*/
enum tc_processing_rc tc_packet_processing(ezframe_t __cmem * frame, uint8_t *frame_base, struct iphdr *ip_ptr, uint32_t logical_id) __fast_path_code;
enum tc_processing_rc tc_packet_process(ezframe_t __cmem * frame, uint8_t *frame_base, struct match_info *match_info, struct iphdr *ip_hdr, uint32_t logical_id) __fast_path_code;

enum tc_processing_rc tc_classification_perform_rule_wrapper(ezframe_t __cmem * frame,
							     uint8_t *frame_base,
							     struct iphdr  *ip_hdr,
							     uint32_t logical_id,
							     struct match_info *match_info,
							     bool is_flow_cached);
enum tc_processing_rc tc_handle_slow_classification_process_wrapper(ezframe_t __cmem * frame, uint8_t *frame_base,
								    struct match_info *match_info,
								    struct iphdr *ip_hdr, uint32_t logical_id,
								    bool is_continue_after_cache);

/******************************************************************************
 * \brief       tc_print_classifier_key - print classification key fields only debug
 *
 * \return      void
 */
static __always_inline
void tc_print_classifier_key(void)
{
	anl_write_log(LOG_DEBUG, "print classification key info:");
	anl_write_log(LOG_DEBUG, "logical id %d", cmem_tc.classifier_key.ingress_logical_id);
	anl_write_log(LOG_DEBUG, "ether type 0x%04x", cmem_tc.classifier_key.ether_type);
	anl_write_log(LOG_DEBUG, "dst mac %02x:%02x:%02x:%02x:%02x:%02x",
		      cmem_tc.classifier_key.dst_mac.ether_addr_octet[0],
		      cmem_tc.classifier_key.dst_mac.ether_addr_octet[1],
		      cmem_tc.classifier_key.dst_mac.ether_addr_octet[2],
		      cmem_tc.classifier_key.dst_mac.ether_addr_octet[3],
		      cmem_tc.classifier_key.dst_mac.ether_addr_octet[4],
		      cmem_tc.classifier_key.dst_mac.ether_addr_octet[5]);
	anl_write_log(LOG_DEBUG, "src mac %02x:%02x:%02x:%02x:%02x:%02x",
		      cmem_tc.classifier_key.src_mac.ether_addr_octet[0],
		      cmem_tc.classifier_key.src_mac.ether_addr_octet[1],
		      cmem_tc.classifier_key.src_mac.ether_addr_octet[2],
		      cmem_tc.classifier_key.src_mac.ether_addr_octet[3],
		      cmem_tc.classifier_key.src_mac.ether_addr_octet[4],
		      cmem_tc.classifier_key.src_mac.ether_addr_octet[5]);
	anl_write_log(LOG_DEBUG, "dst ip %d.%d.%d.%d",
		      *((uint8_t *)(&cmem_tc.classifier_key.dst_ip)),
		      *((uint8_t *)(&cmem_tc.classifier_key.dst_ip)+1),
		      *((uint8_t *)(&cmem_tc.classifier_key.dst_ip)+2),
		      *((uint8_t *)(&cmem_tc.classifier_key.dst_ip)+3));
	anl_write_log(LOG_DEBUG, "src ip %d.%d.%d.%d",
		      *((uint8_t *)(&cmem_tc.classifier_key.src_ip)),
		      *((uint8_t *)(&cmem_tc.classifier_key.src_ip)+1),
		      *((uint8_t *)(&cmem_tc.classifier_key.src_ip)+2),
		      *((uint8_t *)(&cmem_tc.classifier_key.src_ip)+3));
	anl_write_log(LOG_DEBUG, "ip protocol %d", cmem_tc.classifier_key.ip_proto);
	anl_write_log(LOG_DEBUG, "l4 dst port %d", cmem_tc.classifier_key.l4_dst_port);
	anl_write_log(LOG_DEBUG, "l4 src port %d", cmem_tc.classifier_key.l4_src_port);
	anl_write_log(LOG_DEBUG, "bitmap mask 0x%02x", *(uint8_t *)(&cmem_tc.classifier_key.tc_mask_info.mask_bitmap));
	anl_write_log(LOG_DEBUG, "ipv4 dst mask 0x%02x", cmem_tc.classifier_key.tc_mask_info.ipv4_dst_mask_val);
	anl_write_log(LOG_DEBUG, "ipv4 src mask 0x%02x", cmem_tc.classifier_key.tc_mask_info.ipv4_src_mask_val);

	anl_write_log(LOG_DEBUG, "classifier key raw data = 0x%08x%08x%08x%08x%08x%08x%08x%08x%08x",
		      cmem_tc.classifier_key.raw_data[0],
		      cmem_tc.classifier_key.raw_data[1],
		      cmem_tc.classifier_key.raw_data[2],
		      cmem_tc.classifier_key.raw_data[3],
		      cmem_tc.classifier_key.raw_data[4],
		      cmem_tc.classifier_key.raw_data[5],
		      cmem_tc.classifier_key.raw_data[6],
		      cmem_tc.classifier_key.raw_data[7],
		      cmem_tc.classifier_key.raw_data[8]);
}

/******************************************************************************
 * \brief       tc_build_classifier_key - build classification key according to
 *		current mask index
 *
 * \return      false - looking for fields not present in packet, true otherwise
 */
static __always_inline
bool tc_build_classifier_key(struct ether_header *l2_hdr, struct iphdr  *ip_hdr, struct l4_hdr *l4_hdr)
{
	anl_write_log(LOG_DEBUG, "mask bitmap 0x%04x", cmem_tc.mask_res.tc_mask_info.mask_bitmap.raw_data);

	/*fill vlan id key*/
	if (cmem_tc.mask_res.tc_mask_info.mask_bitmap.is_eth_type_set) {
		anl_write_log(LOG_DEBUG, "is_eth_type_set = set");
		cmem_tc.classifier_key.ether_type = l2_hdr->ether_type;
	} else {
		cmem_tc.classifier_key.ether_type = 0;
	}

	/*fill vlan id key*/
	if (cmem_tc.mask_res.tc_mask_info.mask_bitmap.is_vlan_id_set) {
		anl_write_log(LOG_DEBUG, "is_vlan_id_set = set");
		if (packet_meta_data.number_of_tags == 0) {
			anl_write_log(LOG_DEBUG, "packet is without vlan");
			return false;
		}
		cmem_tc.classifier_key.vlan_id = *((uint16_t *)((uint8_t *)&(l2_hdr->ether_type) + sizeof(uint16_t))) & 0xfff0;
	} else {
		cmem_tc.classifier_key.vlan_id = 0;
	}

	/*fill vlan prio key*/
	if (cmem_tc.mask_res.tc_mask_info.mask_bitmap.is_vlan_prio_set) {
		anl_write_log(LOG_DEBUG, "is_vlan_prio_set = set");
		if (packet_meta_data.number_of_tags == 0) {
			anl_write_log(LOG_DEBUG, "packet is without vlan");
			return false;
		}
		cmem_tc.classifier_key.vlan_prio = *((uint16_t *)((uint8_t *)&(l2_hdr->ether_type) + sizeof(uint16_t))) & 0xe;
	} else {
		cmem_tc.classifier_key.vlan_prio = 0;
	}

	/*fill eth_dst key*/
	if (cmem_tc.mask_res.tc_mask_info.mask_bitmap.is_eth_dst_set) {
		anl_write_log(LOG_DEBUG, "is_eth_dst_set = set");
		ezdp_mem_copy(&cmem_tc.classifier_key.dst_mac, l2_hdr->ether_dhost, ETH_ALEN);
	} else {
		ezdp_mem_set(cmem_tc.classifier_key.dst_mac.ether_addr_octet, 0, ETH_ALEN);
	}

	/*fill eth_src key*/
	if (cmem_tc.mask_res.tc_mask_info.mask_bitmap.is_eth_src_set) {
		anl_write_log(LOG_DEBUG, "is_eth_src_set = set");
		ezdp_mem_copy(&cmem_tc.classifier_key.src_mac, l2_hdr->ether_shost, ETH_ALEN);
	} else {
		ezdp_mem_set(cmem_tc.classifier_key.src_mac.ether_addr_octet, 0, ETH_ALEN);
	}

	/* fill ipv4_src key if mask_bit_map of this field was set */
	/* if !IPV4 return FALSE - impossible to get ipv4_src from frame */
	if (cmem_tc.mask_res.tc_mask_info.mask_bitmap.is_ipv4_src_set) {
		if (!ip_hdr) {
			anl_write_log(LOG_DEBUG, "mask is ipv4 src is set, but frame is not IP");
			return false;
		}
		anl_write_log(LOG_DEBUG, "is_ipv4_src_set = set");
		cmem_tc.classifier_key.src_ip = ip_hdr->saddr &	tc_bitmask_array[cmem_tc.mask_res.tc_mask_info.ipv4_src_mask_val];
		cmem_tc.classifier_key.tc_mask_info.ipv4_src_mask_val = cmem_tc.mask_res.tc_mask_info.ipv4_src_mask_val;
	} else {
		cmem_tc.classifier_key.src_ip = 0;
		cmem_tc.classifier_key.tc_mask_info.ipv4_src_mask_val = 0;
	}

	/*fill ipv4_dst key if mask_bit_map of this field was set - */
	/*if !IPV4 return FALSE - impossible to get ipv4_dst from frame */
	if (cmem_tc.mask_res.tc_mask_info.mask_bitmap.is_ipv4_dst_set) {
		if (!ip_hdr) {
			anl_write_log(LOG_DEBUG, "mask is ipv4 dst is set, but frame is not IP");
			return false;
		}
		anl_write_log(LOG_DEBUG, "is_ipv4_dst_set = set");
		cmem_tc.classifier_key.dst_ip = ip_hdr->daddr &	tc_bitmask_array[cmem_tc.mask_res.tc_mask_info.ipv4_dst_mask_val];
		cmem_tc.classifier_key.tc_mask_info.ipv4_dst_mask_val = cmem_tc.mask_res.tc_mask_info.ipv4_dst_mask_val;
	} else {
		cmem_tc.classifier_key.dst_ip = 0;
		cmem_tc.classifier_key.tc_mask_info.ipv4_dst_mask_val = 0;
	}

	/*fill ipv4_proto key if mask_bit_map of this field was set - */
	/*if !IPV4 return FALSE - impossible to get ipv4_proto from frame */
	if (cmem_tc.mask_res.tc_mask_info.mask_bitmap.is_ip_proto_set) {
		if (!ip_hdr) {
			anl_write_log(LOG_DEBUG, "mask is ipv4 proto is set, but frame is not IP");
			return false;
		}
		anl_write_log(LOG_DEBUG, "is_ip_proto_set = set");
		cmem_tc.classifier_key.ip_proto = ip_hdr->protocol;
	} else {
		cmem_tc.classifier_key.ip_proto = 0;
	}

	/*fill l4_dst key if mask_bit_map of this field was set -*/
	/* if !IPV4 return FALSE - impossible to get l4_dst from frame*/
	if (cmem_tc.mask_res.tc_mask_info.mask_bitmap.is_l4_dst_set) {
		if (!l4_hdr) {
			return false;
		}
		anl_write_log(LOG_DEBUG, "is_ip_proto_set = set");
		cmem_tc.classifier_key.l4_dst_port = l4_hdr->l4_dst;
	} else {
		cmem_tc.classifier_key.l4_dst_port = 0;
	}

	/*fill l4_src key if mask_bit_map of this field was set -*/
	/* if !IPV4 return FALSE - impossible to get l4_src from frame*/
	if (cmem_tc.mask_res.tc_mask_info.mask_bitmap.is_l4_src_set) {
		if (!l4_hdr) {
			return false;
		}
		anl_write_log(LOG_DEBUG, "is_ip_proto_set = set");
		cmem_tc.classifier_key.l4_src_port = l4_hdr->l4_src;
	} else {
		cmem_tc.classifier_key.l4_src_port = 0;
	}

	ezdp_mem_copy(&cmem_tc.classifier_key.tc_mask_info.mask_bitmap, &cmem_tc.mask_res.tc_mask_info.mask_bitmap, sizeof(union tc_mask_bitmap));
	tc_print_classifier_key();

	return true;
}

#if 0
static __always_inline
void tc_build_flow_cache_classifier_key(struct ether_header *l2_hdr, struct iphdr  *ip_hdr, struct tcphdr *tcp_hdr)
{
	cmem_tc.flow_cache_classifier_key.zero_rsrv1 = 0;
	ezdp_mem_copy(&cmem_tc.flow_cache_classifier_key.dst_mac, l2_hdr->ether_dhost, ETH_ALEN);
	ezdp_mem_copy(&cmem_tc.flow_cache_classifier_key.src_mac, l2_hdr->ether_shost, ETH_ALEN);
	cmem_tc.flow_cache_classifier_key.src_ip = ip_hdr->saddr;
	cmem_tc.flow_cache_classifier_key.dst_ip = ip_hdr->daddr;
	cmem_tc.flow_cache_classifier_key.ip_proto = ip_hdr->protocol;
	cmem_tc.flow_cache_classifier_key.l4_dst_port = tcp_hdr->dest;
	cmem_tc.flow_cache_classifier_key.l4_src_port = tcp_hdr->source;
	cmem_tc.flow_cache_classifier_key.zero_rsrv2 = 0;
}
#endif

/******************************************************************************
 * \brief       lookup in the bitmask according to ingress port and protocol
 *
 * \return      return 0 in case of success, otherwise no match.
 */
static __always_inline
uint32_t tc_lookup_classifier_bitmask(uint16_t mask_index)
{
	return ezdp_lookup_table_entry(&shared_cmem_tc.tc_mask_bitmap_struct_desc,
				       mask_index, &cmem_tc.mask_res,
				       sizeof(struct tc_mask_result), 0);
}

/******************************************************************************
 * \brief
 *
 * \return      return 0 in case of success, otherwise no match.
 */
#if 0
static __always_inline
uint32_t tc_lookup_flow_cache_idx(uint32_t flow_index)
{
	return ezdp_lookup_table_entry(&shared_cmem_tc.tc_flow_cache_idx_struct_desc,
				       flow_index, &cmem_tc.flow_cache_idx_res,
				       sizeof(struct tc_flow_cache_idx_result), 0);
}
#endif
/******************************************************************************
 * \brief       lookup in the rules list table
 *
 * \return      return 0 in case of success, otherwise no match.
 */
static __always_inline
uint32_t tc_lookup_rules_list_table(uint16_t rules_list_index)
{
	return ezdp_lookup_table_entry(&shared_cmem_tc.tc_rules_list_struct_desc,
				       rules_list_index, &cmem_tc.rules_list_res,
				       sizeof(struct tc_rules_list_result), 0);

}

/******************************************************************************
 *\brief	tc_save_classifier_result_best_match - save current match entry
 *		in best match entry array. the array can contain up to 8 matches
 *		with the highest priorities.
 *		in case no more space in match array, remove lowest priority match
 *		entry and insert new entry.
 *		the array is not ordered but the cmem always save the index of the highest
 *		priority in the array.
 *
 *\return	void
 */
static __always_inline
void tc_save_classifier_result_best_match(uint16_t priority, uint32_t filter_actions_index, struct match_info *best_match)
{
	uint8_t		i;
	uint8_t		lowest_index = 0;
	uint16_t	lowest_priority;
	uint8_t		match_num;

	anl_write_log(LOG_DEBUG, "try to add priority %d, filter action index %d",
		      priority,
		      filter_actions_index);

	/*check if array is empty*/
	if (best_match->match_array_valid_bitmap == 0) {
		best_match->best_match_array[0].priority = priority;
		best_match->best_match_array[0].filter_actions_index = filter_actions_index;
		best_match->match_array_valid_bitmap |= 0x1;
		best_match->highest_priority_index = 0;
		anl_write_log(LOG_DEBUG, "add priority %d, filter actions index %d to valid bitmap. current valid bitmap 0x%02x",
			      priority,
			      filter_actions_index,
			      best_match->match_array_valid_bitmap);
		return;
	}

	/*check if array is full*/
	if (best_match->match_array_valid_bitmap == TC_MAX_VALID_BIT) {
		anl_write_log(LOG_DEBUG, "valid bitmap match is full - check if need to replace with lowest");
		lowest_priority = best_match->best_match_array[0].priority;
		/*find lowest priority rule and replace it with new one*/
		for (i = 1 ; i < TC_MAX_CONTINUE_RESULTS; i++) {
			if (best_match->best_match_array[i].priority > lowest_priority) {
				lowest_priority = best_match->best_match_array[i].priority;
				lowest_index = i;
			}
		}

		if (lowest_priority > priority) {
			anl_write_log(LOG_DEBUG, "replace lowest priority %d with priority %d in valid bitmap", lowest_priority, priority);
			best_match->best_match_array[lowest_index].priority = priority;
			best_match->best_match_array[lowest_index].filter_actions_index = filter_actions_index;
			/*check if new entry is the highest*/
			if (priority < best_match->best_match_array[best_match->highest_priority_index].priority) {
				best_match->highest_priority_index = lowest_index;
			}
			anl_write_log(LOG_DEBUG, "add priority %d, filter actions index %d to valid bitmap. current valid bitmap 0x%02x",
				      priority,
				      filter_actions_index,
				      best_match->match_array_valid_bitmap);
		}
		return;
	}

	/*insert new match entry*/
	/*get the number of match in best match array*/
	match_num = ezdp_count_bits(best_match->match_array_valid_bitmap, 0, TC_MAX_CONTINUE_RESULTS);

	best_match->best_match_array[match_num].priority = priority;
	best_match->best_match_array[match_num].filter_actions_index = filter_actions_index;
	best_match->match_array_valid_bitmap |= (1 << match_num);

	/*check if highest*/
	/*TODO - Alla - i think it should be priority < ... */
	if (priority < best_match->best_match_array[best_match->highest_priority_index].priority) {
		best_match->highest_priority_index = match_num;
	}

	anl_write_log(LOG_DEBUG, "add priority %d, filter actions index %d to valid bitmap. current valid bitmap 0x%02x",
		      priority,
		      filter_actions_index,
		      best_match->match_array_valid_bitmap);
}

/******************************************************************************
 *\brief	tc_get_next_highest_priority_entry - called only in case of continue
 *		action. set of the match bit of current highest entry and look for the
 *		next highest entry that need to be handled
 *
 *\return	bool. true if more entries to handle. false otherwise.
 */
static __always_inline
bool tc_get_next_highest_priority_entry(struct match_info *match_info)
{
	uint8_t i = 0;
	uint32_t temp_prio = 0xffffffff;

	anl_write_log(LOG_DEBUG, "get next highest priority entry. index %d current highest priority %d",
		      match_info->highest_priority_index,
		      match_info->best_match_array[match_info->highest_priority_index].priority);

	/*clear current highest valid bit*/
	match_info->match_array_valid_bitmap &= ~(1 << match_info->highest_priority_index);

	anl_write_log(LOG_DEBUG, "match valid bitmap 0x%02x", match_info->match_array_valid_bitmap);

	/*update new highest index*/
	if (match_info->match_array_valid_bitmap != 0) {
		do {
			if (match_info->match_array_valid_bitmap & (1 << i)) {
				if (match_info->best_match_array[i].priority < temp_prio) {
					temp_prio = match_info->best_match_array[i].priority;
					match_info->highest_priority_index = i;
				}
			}
			i++;
		} while (i < TC_MAX_CONTINUE_RESULTS);
	} else {
		anl_write_log(LOG_DEBUG, "no more entries in match array");
		return false;
	}

	anl_write_log(LOG_DEBUG, "next highest priority %d", match_info->best_match_array[match_info->highest_priority_index].priority);
	return true;
}

/******************************************************************************
 *\brief tc_scan_rules_list_index - scan all rules list and save its entries
 *	  in best match array
 *
 *\return	void
 */
static __always_inline
void tc_scan_rules_list_index(uint32_t rules_list_index, struct match_info *match_info)
{
	anl_write_log(LOG_DEBUG, "scanning rules list index. rule index %d", rules_list_index);
	do {
		if (tc_lookup_rules_list_table(rules_list_index) != 0) {
			anl_write_log(LOG_DEBUG, "no match in rules list for index %d", rules_list_index);
			return;
		}

		tc_save_classifier_result_best_match(cmem_tc.rules_list_res.priority,
						     cmem_tc.rules_list_res.filter_actions_index,
						     match_info);

		rules_list_index = cmem_tc.rules_list_res.next_rule_index;

	} while (cmem_tc.rules_list_res.is_next_rule_valid);
}

/******************************************************************************
 *\brief	tc_classification_perform_rule - perform highest priority match actions
 *		and return the outcome of the action to to the classification process
 *		for further processing
 *
 *\return	stolen or accept. stolen in case packet was drop or handled by TC
 *		accept in case packet needs to be handled by NW layer.
 */
static __always_inline
enum tc_processing_rc tc_classification_perform_rule(ezframe_t __cmem * frame,
						     uint8_t *frame_base,
						     struct iphdr  *ip_hdr,
						     uint32_t logical_id,
						     struct match_info *match_info,
						     bool is_flow_cached __unused)
{
	enum tc_action_rc	rc;
#if 0
	if (!is_flow_cached) {
		tc_learn_new_flow_cache();
	}
#endif
	anl_write_log(LOG_DEBUG, "perform classification match");
	switch (rc = tc_action_handle_highest_priority(frame, frame_base, match_info, ip_hdr)) {
	case TC_ACTION_RC_ACCEPT:
	case TC_ACTION_RC_PACKET_STOLEN:
	{
		anl_write_log(LOG_DEBUG, "got rc code from action %s", rc == TC_ACTION_RC_ACCEPT ? "ACCEPT":"STOLEN");
		return (enum tc_action_rc)rc;
	}
	case TC_ACTION_RC_RECLASSIFY:
	{
		anl_write_log(LOG_DEBUG, "call re-classification of packet");
		return tc_packet_process(frame, frame_base, match_info, ip_hdr, logical_id);
	}
	case TC_ACTION_RC_CONTINUE:
	{
		anl_write_log(LOG_DEBUG, "got continue action - go to next highest priority match");
#if 0
		if (is_flow_cached) {
			/*in case we arrived from cache flow path - need to do full classification process*/
			/*first check current filter rules list*/
			tc_get_next_highest_priority_entry();
			return tc_handle_slow_classification_process_wrapper(frame, frame_base, true);
		}
#endif
		if (tc_get_next_highest_priority_entry(match_info)) {
			return tc_classification_perform_rule_wrapper(frame,
								      frame_base,
								      ip_hdr,
								      logical_id,
								      match_info,
								      false);
		}

		return TC_ACTION_RC_ACCEPT;
	}
	default:
	{
		anl_write_log(LOG_ERR, "got unknown rc = %d", rc);
		tc_discard_frame();
		return TC_ACTION_RC_PACKET_STOLEN;
	}
	}

}

/******************************************************************************
 *\brief	tc_handle_slow_classification_process - perform full flow TC classification
 *		process for incoming packet. go over all classifier & rules list table and
 *		save best 8 highest priority entries. (max 8 actions of continue)
 *		in case of no match return to nw process.
 *		in case of match handle entries actions.
 *
 *\return      accept or stolen. stolen means the packet was handled by TC and will
 *		not continue to NW process.
 *		accept means the packet should continue to regualr NW processing.
 */
static __always_inline
enum tc_processing_rc tc_handle_slow_classification_process(ezframe_t  __cmem   * frame,
							    uint8_t		*frame_base,
							    struct match_info	*match_info,
							    struct iphdr	*ip_hdr,
							    uint32_t		logical_id,
							    bool		is_continue_after_cache __unused)
{
	struct ezdp_lookup_retval	retval;
	struct  tc_classifier_result	*classifier_res;
	uint16_t			mask_index, base_mask_index;
	struct ether_header		*l2_hdr = (struct ether_header *)frame_base;
	/*in case of reclassify - user must update metadata before*/
	struct l4_hdr			*l4_hdr = NULL;
	uint32_t			max_tries = EZDP_LOOKUP_MAX_TRIES;
#if 0
	uint8_t match_num;
#endif

	anl_write_log(LOG_DEBUG, "TC start slow classification process");

	match_info->match_array_valid_bitmap = 0;

	if (packet_meta_data.ip_next_protocol.tcp ||
		packet_meta_data.ip_next_protocol.udp) {
		l4_hdr = (struct l4_hdr *)((uint8_t *)ip_hdr + (ip_hdr->ihl << 2));
	}

#if 0
	if (!is_continue_after_cache)
#endif

	cmem_tc.classifier_key.ingress_logical_id = logical_id;
	cmem_tc.classifier_key.zero_rsv1 = 0;
	cmem_tc.classifier_key.zero_rsv2 = 0;
	anl_write_log(LOG_DEBUG, "ingress_logical_id %d", cmem_tc.classifier_key.ingress_logical_id);

	mask_index = TC_CALC_MASK_BASE_INDEX(cmem_tc.classifier_key.ingress_logical_id);
	base_mask_index = mask_index;

	anl_write_log(LOG_DEBUG, "base mask_index %d", base_mask_index);

	while (mask_index < (base_mask_index + TC_MASKS_NUM_PER_PORT_AND_PROTO) && tc_lookup_classifier_bitmask(mask_index) == 0) {
		if (tc_build_classifier_key(l2_hdr, ip_hdr, l4_hdr) == true) {
			ezdp_hashed_key_t hashed_key = ezdp_prm_hash_bulk_key((uint8_t *)&cmem_tc.classifier_key, sizeof(union tc_classifier_key));

			do {
				retval.raw_data = ezdp_prm_lookup_hash_entry(shared_cmem_tc.tc_classifier_struct_desc.main_table_addr_desc.raw_data,
									     true,
									     sizeof(union tc_classifier_key),
									     sizeof(struct tc_classifier_result),
									     sizeof(struct classifier_entry),
									     hashed_key,
									     (void *)&cmem_tc.classifier_key,
									     (void *)&cmem_tc.class_entry,
									     cmem_wa.tc_wa.classifier_prm_hash_wa,
									     sizeof(cmem_wa.tc_wa.classifier_prm_hash_wa));
				if (likely(retval.success)) {

					classifier_res = &cmem_tc.class_entry.result;
					anl_write_log(LOG_DEBUG, "Match found in classifier table");
					tc_save_classifier_result_best_match(classifier_res->priority,
									     classifier_res->filter_actions_index,
									     match_info);
					if (classifier_res->is_rules_list_valid) {
						anl_write_log(LOG_DEBUG, "classifier match has more entries in rule list. rule index = %d", classifier_res->rules_list_index);
						tc_scan_rules_list_index(classifier_res->rules_list_index, match_info);
					}
					anl_write_log(LOG_DEBUG, "current valid bitmap array - 0x%02x", match_info->match_array_valid_bitmap);
					break;
				} else if (likely(!retval.mem_error)) {
					/* no match */
					anl_write_log(LOG_DEBUG, "No Match found in classifier table");
					break;
				}

				max_tries--;
			} while (unlikely(max_tries > 0));
		}
		mask_index++;
	}

	anl_write_log(LOG_DEBUG, "valid bitmap array after classification process - 0x%02x", match_info->match_array_valid_bitmap);

	if (match_info->match_array_valid_bitmap != 0) {
#if 0
		if (is_continue_after_cache) {

			/*get the number of match in best match array*/
			match_num = ezdp_count_bits(cmem_tc.match_array_valid_bitmap, 0, TC_MAX_CONTINUE_RESULTS);


			/* in this special case we arrived to continue after a match in cache flow. */
			/* in case we only have match in bitmap this means this is the same match */
			/* as before so don't perform and go back. */
			/* otherwise, remove the highest and perform the 2nd best match ==> continue */
			if (match_num == 1) {
				/* no continue match */
				return;
			}
			tc_get_next_highest_priority_entry();
		}
#endif
		return tc_classification_perform_rule(frame, frame_base, ip_hdr, logical_id, match_info, false);
	}

	anl_write_log(LOG_DEBUG, "No match in classifier entry found for incoming packet - accept packet");
	/*no match in TC rule table - continue to standard routing*/
	return TC_PROCESSING_RC_ACCEPT;
}

/******************************************************************************
 * \brief
 *
 * \return
 */
#if 0
static __always_inline
void tc_handle_flow_cache_process(ezframe_t __cmem * frame, uint8_t *frame_base)
{
	uint32_t rc;
	uint32_t found_result_size;
	struct  tc_flow_cache_classifier_result *flow_cache_classifier_result_ptr;
	struct ether_header *l2_hdr = (struct ether_header *)frame_base;
	struct iphdr  *ip_hdr  = (struct iphdr *)(frame_base + packet_meta_data.ip_offset);
	struct tcphdr *tcp_hdr = (struct tcphdr *)((uint8_t *)ip_hdr + (ip_hdr->ihl << 2));

	cmem_tc.flow_cache_classifier_key.ingress_logical_id = ezframe_get_logical_id(frame);
	cmem_tc.flow_cache_classifier_key.ether_type = l2_hdr->ether_type;

	tc_build_flow_cache_classifier_key(l2_hdr, ip_hdr, tcp_hdr);

	rc = ezdp_lookup_hash_entry(&shared_cmem_tc.tc_flow_cache_classifier_struct_desc,
				    (void *)&cmem_tc.flow_cache_classifier_key,
				    sizeof(struct tc_flow_cache_classifier_key),
				    (void **)&flow_cache_classifier_result_ptr,
				    &found_result_size, 0,
				    cmem_wa.tc_wa.flow_cache_classifier_hash_wa,
				    sizeof(cmem_wa.tc_wa.flow_cache_classifier_hash_wa));

	if (rc == 0) {
		if (tc_lookup_flow_cache_idx(flow_cache_classifier_result_ptr->flow_cache_index) == 0) {
			cmem_tc.highest_priority_index   = 0;
			cmem_tc.match_array_valid_bitmap = 1;
			cmem_tc.best_match_array[cmem_tc.highest_priority_index].action_index = cmem_tc.flow_cache_idx_res.action_index;
			cmem_tc.best_match_array[cmem_tc.highest_priority_index].priority = cmem_tc.flow_cache_idx_res.priority;
			cmem_tc.best_match_array[cmem_tc.highest_priority_index].rules_list_index = cmem_tc.flow_cache_idx_res.rules_list_index;
			tc_classification_perform_rule(frame, frame_base, ip_hdr, true);
		} else {
			/*this is error scenario - packet should be dropped. */
			tc_discard_frame();
		}
	} else {
		/*no TC rule match in flow cache  - do slow TC classification process*/
		tc_handle_slow_classification_process(frame, frame_base, false);
	}
}
#endif
#endif /* TC_PACKET_PROCESSING_H_ */
