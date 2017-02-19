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
*  File:                tc_flower.h
*  Desc:                Implementation of tc (traffic control) internal functions API.
*
*/

#ifndef SRC_CP_TC_FLOWER_H_
#define SRC_CP_TC_FLOWER_H_

#include "tc_action.h"

#define NOT_VALID_RULE_INDEX         0xFFFFFFFF

struct rules_list_item {
	uint32_t priority;
	uint32_t filter_actions_index;
	uint32_t rules_list_index;
};

void print_flower_filter(struct tc_filter *tc_filter_handle);

bool tc_flower_init(void);
void tc_flower_destroy(void);
enum tc_api_rc tc_flower_filter_add(struct tc_filter *tc_filter_params_origin);
enum tc_api_rc tc_flower_filter_delete(struct tc_filter *tc_filter_params_origin);
enum tc_api_rc tc_flower_filter_modify(struct tc_filter *tc_filter_params);
bool get_ip_mask_value(uint32_t ip_mask, uint8_t *mask_value);
bool is_mask_valid(struct tc_flower_rule_policy *flower_rule_policy);
bool create_mask_info(struct tc_flower_rule_policy *flower_rule_policy, struct tc_mask_info *mask_info);
enum tc_api_rc tc_filter_params_fill(struct tc_filter *tc_filter_params_origin, struct tc_filter *tc_filter_params);


/* classifier table */
enum tc_api_rc add_entry_to_classifier_table(struct tc_filter *tc_filter_params,
					     struct tc_mask_info tc_mask_info,
					     struct rules_list_item *first_rule_list_item,
					     uint32_t second_rule_list_index,
					     bool modify_entry,
					     uint32_t nps_port_index);
/* filter actions table */
enum tc_api_rc add_filter_actions(struct tc_filter *tc_filter_params, struct action_info *action_info_array);
enum tc_api_rc add_actions_to_filter_actions_table(struct action_info *action_info,
							  uint32_t num_of_actions,
							  uint32_t *filter_actions_index);
enum tc_api_rc modify_actions_to_filter_actions_table(struct action_info *action_info,
						      uint32_t num_of_actions,
						      uint32_t filter_actions_index);
enum tc_api_rc delete_and_free_filter_actions_entry(uint32_t filter_actions_index);

/* rules list table */
enum tc_api_rc add_rules_list_entries_to_table(struct rules_list_item *rules_list, uint32_t rules_list_size);
enum tc_api_rc delete_and_free_rule_list_entry(uint32_t rule_list_index);
enum tc_api_rc update_rules_list_entries(struct tc_filter *tc_filter_params,
					 struct rules_list_item *first_rule_list_item,
					 uint32_t *second_rule_list_index,
					 uint32_t *rules_list_size);
enum tc_api_rc allocate_rule_list_indexes(struct tc_filter *tc_filter_params,
					  uint32_t filter_actions_index,
					  uint32_t *rule_list_index,
					  struct action_info *action_info);
enum tc_api_rc create_and_add_rules_list(struct tc_filter *tc_filter_params,
					 struct rules_list_item *rules_list,
					 uint32_t rules_list_size);

/* masks table */
enum tc_api_rc add_filter_to_mask_table(struct tc_filter *tc_filter_params, struct tc_mask_info tc_mask_info, uint32_t nps_port_index);
enum tc_api_rc get_the_last_mask_entry(struct tc_filter *tc_filter_params, uint32_t *last_mask_entry_index);
enum tc_api_rc remove_mask_from_db_and_dp_table(struct tc_filter *tc_filter_params, uint32_t nps_port_index);

/* build nps results & keys */
void build_nps_tc_filter_actions_key(struct tc_filter_actions_key *nps_filter_actions_key,
				     uint32_t filter_actions_index);
void build_nps_tc_filter_actions_result(struct tc_filter_actions_result *nps_filter_actions_result,
					uint32_t num_of_actions,
					struct action_info *action_info);
void build_nps_tc_masks_result(struct tc_mask_info *tc_mask_info,
			       struct tc_mask_result *nps_masks_result);
void build_nps_tc_masks_key(uint32_t new_mask_index,
			    struct tc_mask_key *nps_masks_key,
			    uint32_t nps_port_index);
void build_nps_rules_list_result(struct rules_list_item *rules_list,
				 uint32_t next_rule_item,
				 struct tc_rules_list_result *rules_list_result);
void build_nps_rules_list_key(struct rules_list_item *rules_list, struct tc_rules_list_key *rules_list_key);
void build_nps_tc_classifier_result(uint32_t highest_priority_value,
				    uint32_t highest_priority_action_index,
				    uint32_t rules_list,
				    struct tc_classifier_result *nps_classifier_result);
void build_nps_tc_classifier_key(struct tc_filter *tc_filter_params,
				 struct tc_mask_info *tc_mask_info,
				 union tc_classifier_key *nps_classifier_key,
				 uint32_t nps_port_index);

/* add/delete internal functions */
enum tc_api_rc tc_int_add_flower_filter(struct tc_filter *tc_filter_params);
enum tc_api_rc tc_int_delete_flower_filter(struct tc_filter *tc_filter_params);

#endif /* SRC_CP_TC_FLOWER_H_ */
