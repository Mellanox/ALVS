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

/******************************************************************************
 * \brief	init ATC DB
 *
 *
 * \return	bool
 *              true -  function succeed
 *              false - function failed
 */
bool tc_init_db(void);

/******************************************************************************
 * \brief	close ATC DB
 *
 *
 * \return	bool
 *              true -  function succeed
 *              false - function failed
 */
bool tc_destroy_db(void);

/* action table functions */

/******************************************************************************
 * \brief	add action to DB
 *
 * \param[in]   tc_action_params    - reference to action configuration (received from netlink)
 * \param[in]   action_info         - reference to action extra information (NPS data, entry index, statistics)
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc add_tc_action_to_db(struct tc_action *tc_action_params, struct action_info *action_info);

/******************************************************************************
 * \brief	delete action to DB
 *
 * \param[in]   tc_action_params    - action configuration
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc delete_tc_action_from_db(struct tc_action *tc_action_params);

/******************************************************************************
 * \brief	get action from DB
 *
 * \param[in]   tc_action_params  - reference to holds action index and family type
 * \param[out]  is_action_exist   - reference to bool, set to true if action exist on DB
 * \param[out]  action_info       - reference to action extra information (NPS data, entry index, statistics)
 * \param[out]  bind_count        - reference to action bind cound value
 * \param[out]  tc_action_from_db - reference to action configuration received from DB
 *
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_tc_action_from_db(struct tc_action *tc_action_params,
				     bool *is_action_exist,
				     struct action_info *action_info,
				     uint32_t *bind_count,
				     struct tc_action   *tc_action_from_db);

/******************************************************************************
 * \brief	check if TC action exist on DB
 *
 * \param[in]   tc_action_params  - reference to holds action index and family type
 * \param[out]  is_action_exist   - reference to bool, set to true if action exist on DB
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc check_if_tc_action_exist(struct tc_action *tc_action_params, bool *is_action_exist);

/******************************************************************************
 * \brief	modify tc action on DB
 *
 * \param[in]   tc_action_params  - reference to action index and family type
 * \param[in]   action_info       - reference to action extra information (NPS data, entry index, statistics)
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc modify_tc_action_on_db(struct tc_action *tc_action_params, struct action_info *action_info);

/******************************************************************************
 * \brief	get number of actions on DB from a specific family type
 *
 * \param[in]   type           - family type of actions
 * \param[out]  num_of_actions - reference to number of actions that we found
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_type_num_of_actions_from_db(enum tc_action_type type, uint32_t *num_of_actions);

/******************************************************************************
 * \brief	get the array of actions from DB that belong to this family type
 *
 * \param[in]   type           - family type of actions
 * \param[out]  actions_array  - array (size num_of_actions) of the action that we found
 * \param[in]   num_of_actions - number of actions that we found
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_type_action_indexes_from_db(enum tc_action_type type, uint32_t *actions_array, uint32_t num_of_actions);

/* flower filter table functions */

/******************************************************************************
 * \brief	add flower filter to DB
 *
 * \param[in]   tc_filter_params     - reference to filter configurations
 * \param[in]   filter_actions_index - filter actions index of this filter
 * \param[in]   rule_list_index      - rule list index of this filter
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc tc_add_flower_filter_to_db(struct tc_filter *tc_filter_params,
					  uint32_t filter_actions_index,
					  uint16_t rule_list_index);

/******************************************************************************
 * \brief	modify flower filter on DB
 *
 * \param[in]   tc_filter_params     - reference to filter configurations
 * \param[in]   action_info_array    - array of all the actions on this filter (each element has action information)
 * \param[in]   filter_actions_index - filter actions index of this filter
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc tc_modify_flower_filter_on_db(struct tc_filter *tc_filter_params,
					     struct action_info *action_info_array,
					     uint32_t filter_actions_index);

/******************************************************************************
 * \brief	check if the handle of the filter is the highest in priority
 *
 * \param[in]   tc_filter_params - reference to filter configurations
 * \param[in]   result           - reference to boolean variable, true if the handle is the highest or false if not
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc check_if_handle_is_highest(struct tc_filter *tc_filter_params, bool *result);

/******************************************************************************
 * \brief	get the size of the rule list (rule list are all the filters that have the same key&mask and different priority)
 *
 * \param[in]   tc_filter_params - reference to filter configurations
 * \param[out]  rules_list_size  - reference to rule list size (uint32)
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_rules_list_size(struct tc_filter *tc_filter_params, uint32_t *rules_list_size);

/******************************************************************************
 * \brief	prepare the rule list items on an array
 *
 * \param[in]   tc_filter_params - reference to filter configurations
 * \param[in]   rules_list_size  - reference to rule list array
 * \param[in]   rules_list_size  - reference to rule list size (uint32)
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc prepare_rules_list_items(struct tc_filter *tc_filter_params,
					struct rules_list_item *rules_list,
					uint32_t rules_list_size);

/******************************************************************************
 * \brief	remove filter from DB
 *
 * \param[in]   tc_filter_params - reference to filter configurations
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc remove_filter_from_db(struct tc_filter *tc_filter_params);

/******************************************************************************
 * \brief	check if filter exist on DB
 *
 * \param[in]   tc_filter_params - reference to filter configurations
 * \param[out]  is_exist - reference to boolean, return true if exist, false if not exist
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc check_if_filter_exist_on_db(struct tc_filter *tc_filter_params, bool *is_exist);

/******************************************************************************
 * \brief	get filter rule list index
 *
 * \param[in]   tc_filter_params - reference to filter configurations
 * \param[out]  filter_rule_index - reference to filter rule index
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_filter_rule_from_db(struct tc_filter *tc_filter_params, uint16_t *filter_rule_index);

/******************************************************************************
 * \brief	check if filter mask is already exist on DB
 *
 * \param[in]   tc_filter_params - reference to filter configurations
 * \param[out]  is_mask_exist - reference to boolean, true if exist
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc check_if_filters_registered_on_mask(struct tc_filter *tc_filter_params, bool *is_mask_exist);

/******************************************************************************
 * \brief	get the key&mask of the old filter (used this function on modify - modify can also change key&mask of filter)
 *
 * \param[in]   tc_filter_params     - reference to filter configurations
 * \param[out]  old_tc_filter_params - reference to the old tc filter configuration (key & mask)
 * \param[out]  is_key_changed       - reference to boolean, true if changed
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_old_filter_key(struct tc_filter *tc_filter_params, struct tc_filter *old_tc_filter_params, bool *is_key_changed);

/******************************************************************************
 * \brief	tc_delete_all_flower_filters_on_interface -
 *		Delete all filters with given interface. (all handles will be deleted)
 *
 * \param[in]   interface    - interface port of these filters
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc tc_delete_all_flower_filters_on_interface(uint32_t interface);

/******************************************************************************
 * \brief	tc_delete_all_priority_flower_filters -
 *		Delete all filters with given priority & interface. (all handles will be deleted)
 *
 * \param[in]   interface    - interface port of these filters
 *              priority     - priority of these filters
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc tc_delete_all_priority_flower_filters(struct tc_filter *tc_filter_params);

/******************************************************************************
 * \brief	get all actions of the flower filter
 *
 * \param[in]   tc_filter_params - reference to the filter configuration
 * \param[out]  action_info - reference to array of elements of action info
 * \param[out]  num_of_actions - reference to the number if actions that we found (uint32)
 * \param[out]  filter_actions_index - reference to the filter actions index of the filter
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_flower_filter_actions(struct tc_filter *tc_filter_params,
					 struct action_id *action_info,
					 uint32_t *num_of_actions,
					 uint32_t *filter_actions_index);

/******************************************************************************
 * \brief	get the number of the flower filters that we found on DB (query by interface & priority)
 *
 * \param[in]   interface - interface index (linux index)
 * \param[out]  priority - priority value, if equal to 0 get all filters from this interface (all priorities)
 * \param[out]  num_of_filters - reference to the number if actions that we found (uint32)
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_num_of_flower_filters(uint32_t interface, uint32_t priority, uint32_t *num_of_filters);

/******************************************************************************
 * \brief	get array of the flower filters that we found on DB (query by interface & priority)
 *
 * \param[in]   interface - interface index (linux index)
 * \param[out]  priority - priority value, if equal to 0 get all filters from this interface (all priorities)
 * \param[out]  filters_array - reference of the array of the filters that we found
 * \param[in]   num_of_filters - the number if actions that we can store on the array (filters_array)
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_flower_filters_id_from_db(uint32_t interface,
					     uint32_t priority,
					     struct tc_filter_id *filters_array,
					     uint32_t num_of_filters);

/******************************************************************************
 * \brief	get the flower filter info from DB
 *
 * \param[in/out] tc_filter_params - reference to the filter configuration
 * \param[out]    is_filter_exists - reference to boolean, true if filter exist
 *
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_flower_filter_from_db(struct tc_filter *tc_filter_params, bool *is_filter_exists);

/* masks table functions */

/******************************************************************************
 * \brief	check if this mask is already exist on mask table on DB
 *
 * \param[in]   tc_filter_params - reference to the filter configuration
 * \param[out]  is_mask_exist - reference to boolean, true if mask exist
 *
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc check_if_mask_already_exist(struct tc_filter *tc_filter_params, bool *is_mask_exist);

/******************************************************************************
 * \brief	get the next index in available on the mask table
 *		mask table entries should be sequence and without empty entries
 *
 * \param[in]   tc_filter_params - reference to the filter configuration
 * \param[out]  new_mask_index - reference to the new mask index that we found
 *
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_new_mask_index(struct tc_filter *tc_filter_params, uint32_t *new_mask_index);

/******************************************************************************
 * \brief	add mask to mask table
 *
 * \param[in]   tc_filter_params - reference to the filter configuration
 * \param[in]   new_mask_index - new mask index of the entry that we want to write
 * \param[in]   tc_mask_info - the mask info that we want to save
 *
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc add_mask_to_mask_table_db(struct tc_filter *tc_filter_params,
					 uint32_t new_mask_index,
					 struct tc_mask_info tc_mask_info);

/******************************************************************************
 * \brief	get mask index of a specific mask entry (used on delete mask)
 *
 * \param[in]   tc_filter_params - reference to the filter configuration
 * \param[out]  mask_index_to_delete - reference to the mask index that we found
 *
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_mask_index_to_delete(struct tc_filter *tc_filter_params, uint32_t *mask_index_to_delete);

/******************************************************************************
 * \brief	Delete mask entry from DB
 *
 * \param[in]   tc_filter_params - reference to the filter configuration
 * \param[out]  mask_index_to_delete - mask entry to delete
 *
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc delete_mask_entry_from_db(struct tc_filter *tc_filter_params, uint32_t mask_index_to_delete);

/******************************************************************************
 * \brief	repalce deleted entry on mask table with the last entry in mask table
 *		on delete we replace the last entry with the deleted entry (used only when we want to delete not the last entry)
 *
 * \param[in]   tc_filter_params - reference to the filter configuration
 * \param[in]   last_mask_index - mask entry of the last entry
 * \param[in]   mask_index_to_delete - mask entry to delete
 *
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc replace_deleted_entry_with_last_mask_entry(struct tc_filter *tc_filter_params,
							  uint32_t last_mask_index,
							  uint32_t mask_index_to_delete);

/******************************************************************************
 * \brief	get the mask info from mask table
 *
 * \param[in]   tc_filter_params - reference to the filter configuration
 * \param[in]   mask_index - mask index that we want to find
 * \param[out]  result_mask_info - reference of the mask info that we found on DB
 *
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_result_mask_bitmap(struct tc_filter *tc_filter_params,
				      uint32_t mask_index,
				      struct tc_mask_info *result_mask_info);


#endif /* SRC_CP_TC_DB_H_ */
