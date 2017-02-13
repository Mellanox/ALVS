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
*  File:                tc_db.h
*  Desc:                Implementation of TC (traffic control) Database API.
*
*/


#ifndef SRC_CP_TC_DB_H_
#define SRC_CP_TC_DB_H_

#include "tc_flower.h"

bool tc_init_db(void);
bool tc_destroy_db(void);

/* action table functions */
enum tc_api_rc add_tc_action_to_db(struct tc_action *tc_action_params, struct action_info *action_info);
enum tc_api_rc delete_tc_action_from_db(struct tc_action *tc_action_params);
enum tc_api_rc get_tc_action_from_db(struct tc_action *tc_action_params,
				     bool *is_action_exist,
				     struct action_info *action_info,
				     uint32_t *bind_count);
enum tc_api_rc check_if_tc_action_exist(struct tc_action *tc_action_params, bool *is_action_exist);
enum tc_api_rc modify_tc_action_on_db(struct tc_action *tc_action_params);
enum tc_api_rc get_type_num_of_actions_from_db(enum tc_action_type type, uint32_t *num_of_actions);
enum tc_api_rc get_type_action_indexes_from_db(enum tc_action_type type, uint32_t *actions_array, uint32_t num_of_actions);

/* flower filter table functions */
enum tc_api_rc tc_add_flower_filter_to_db(struct tc_filter *tc_filter_params,
					  uint32_t filter_actions_index,
					  uint16_t rule_list_index);
enum tc_api_rc tc_modify_flower_filter_on_db(struct tc_filter *tc_filter_params,
					     struct action_info *action_info_array,
					     uint32_t filter_actions_index);
enum tc_api_rc check_if_handle_is_highest(struct tc_filter *tc_filter_params, bool *result);
enum tc_api_rc get_rules_list_size(struct tc_filter *tc_filter_params, uint32_t *rules_list_size);
enum tc_api_rc prepare_rules_list_items(struct tc_filter *tc_filter_params,
					struct rules_list_item *rules_list,
					uint32_t rules_list_size);
enum tc_api_rc remove_filter_from_db(struct tc_filter *tc_filter_params);
enum tc_api_rc check_if_filter_exist_on_db(struct tc_filter *tc_filter_params, bool *is_exist);
enum tc_api_rc get_filter_rule_from_db(struct tc_filter *tc_filter_params, uint16_t *filter_rule_index);
enum tc_api_rc check_if_filters_registered_on_mask(struct tc_filter *tc_filter_params, bool *is_mask_exist);
enum tc_api_rc get_old_filter_key(struct tc_filter *tc_filter_params, struct tc_filter *old_tc_filter_params, bool *is_key_changed);
enum tc_api_rc tc_delete_all_flower_filters_on_interface(uint32_t interface);
enum tc_api_rc tc_delete_all_priority_flower_filters(struct tc_filter *tc_filter_params);
enum tc_api_rc get_flower_filter_actions(struct tc_filter *tc_filter_params,
					 struct action_id *action_info,
					 uint32_t *num_of_actions,
					 uint32_t *filter_actions_index);
enum tc_api_rc get_num_of_flower_filters(uint32_t interface, uint32_t priority, uint32_t *num_of_filters);
enum tc_api_rc get_flower_filters_id_from_db(uint32_t interface,
					     uint32_t priority,
					     struct tc_filter_id *filters_array,
					     uint32_t num_of_filters);
enum tc_api_rc get_flower_filter_from_db(struct tc_filter *tc_filter_params, bool *is_filter_exists);
/* masks table functions */
enum tc_api_rc check_if_mask_already_exist(struct tc_filter *tc_filter_params, bool *is_mask_exist);
enum tc_api_rc get_new_mask_index(struct tc_filter *tc_filter_params, uint32_t *new_mask_index);
enum tc_api_rc add_mask_to_mask_table_db(struct tc_filter *tc_filter_params,
					 uint32_t new_mask_index,
					 struct tc_mask_info tc_mask_info);
enum tc_api_rc get_mask_index_to_delete(struct tc_filter *tc_filter_params, uint32_t *mask_index_to_delete);
enum tc_api_rc delete_mask_entry_from_db(struct tc_filter *tc_filter_params, uint32_t mask_index_to_delete);
enum tc_api_rc replace_deleted_entry_with_last_mask_entry(struct tc_filter *tc_filter_params,
							  uint32_t last_mask_index,
							  uint32_t mask_index_to_delete);
enum tc_api_rc get_result_mask_bitmap(struct tc_filter *tc_filter_params,
				      uint32_t mask_index,
				      struct tc_mask_info *result_mask_info);


#endif /* SRC_CP_TC_DB_H_ */
