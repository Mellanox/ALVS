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
*  Project:             NPS400 TC application
*  File:                tc_actions.c
*  Desc:                Implementation of tc (traffic control) actions common API.
*
*/

#include "tc_action.h"
#include "byteswap.h"
#include "tc_db.h"
#include "infrastructure_utils.h"
#include "nw_ops.h"


struct index_pool action_index_pool;
struct index_pool action_extra_info_index_pool;

/******************************************************************************
 * \brief    Add action or modify action if action exist
 *
 * param[in] tc_action_params - reference to action configuration
 *
 * \return   enum tc_api_rc - TC_API_OK       - function succeed
 *                            TC_API_FAILURE  - function failed due to wrong configuration
 *                            TC_API_DB_ERROR - function failed due to problem on DB or NPS
 */
enum tc_api_rc tc_action_add(struct tc_action *tc_action_params)
{
	struct action_info action_info;
	bool is_action_exist;

	/* check if action exists */
	TC_CHECK_ERROR(check_if_tc_action_exist(tc_action_params, &is_action_exist));

	if (is_action_exist == true) {
		return tc_action_modify(tc_action_params);
	}

	TC_CHECK_ERROR(tc_int_add_action(tc_action_params,
					 &action_info,
					 true));

	return TC_API_OK;
}

/******************************************************************************
 * \brief    Delete action
 *
 * param[in] tc_action_params - reference to action configuration
 *
 * \return   enum tc_api_rc - TC_API_OK       - function succeed
 *                            TC_API_FAILURE  - function failed due to wrong configuration
 *                            TC_API_DB_ERROR - function failed due to problem on DB or NPS
 */
enum tc_api_rc tc_action_delete(struct tc_action *tc_action_params)
{
	struct action_info action_info;
	bool is_action_exists;

	write_log(LOG_INFO, "Delete Action (family type %d, index %d)",
		  tc_action_params->general.family_type,
		  tc_action_params->general.index);

	TC_CHECK_ERROR(get_tc_action_from_db(tc_action_params, &is_action_exists, &action_info, NULL, NULL));

	/* if action is not exist return an error */
	if (is_action_exists == false) {
		write_log(LOG_NOTICE, "Action not exist");
		return TC_API_FAILURE;
	}

	write_log(LOG_DEBUG, "Action (index %d) bind value is %d",
		  tc_action_params->general.index,
		  tc_action_params->general.bindcnt);

	if (tc_action_params->general.bindcnt > 1) {
		write_log(LOG_INFO, "cannot delete action, action is binded to other filters");
		return TC_API_FAILURE;
	}

	TC_CHECK_ERROR(delete_tc_action_from_nps(&action_info));

	TC_CHECK_ERROR(delete_tc_action_from_db(tc_action_params));

	write_log(LOG_INFO, "Action (family type %d, index %d) was deleted successfully",
		  tc_action_params->general.family_type,
		  tc_action_params->general.index);

	return TC_API_OK;
}

/******************************************************************************
 * \brief    Modify action
 *
 * param[in] tc_action_params - reference to action configuration
 *
 * \return   enum tc_api_rc - TC_API_OK       - function succeed
 *                            TC_API_FAILURE  - function failed due to wrong configuration
 *                            TC_API_DB_ERROR - function failed due to problem on DB or NPS
 */
enum tc_api_rc tc_action_modify(struct tc_action *tc_action_params)
{
	struct action_info action_info;
	bool is_action_exists;

	write_log(LOG_INFO, "Modify Action (family type %d, index %d)",
		  tc_action_params->general.family_type,
		  tc_action_params->general.index);

	TC_CHECK_ERROR(get_tc_action_from_db(tc_action_params, &is_action_exists, &action_info, NULL, NULL));

	/* if action is not exist return an error */
	if (is_action_exists == false) {
		write_log(LOG_NOTICE, "Action not exist");
		return TC_API_FAILURE;
	}

	/* set action id values */
	action_info.action_id.action_family_type = tc_action_params->general.family_type;
	action_info.action_id.linux_action_index = tc_action_params->general.index;
	action_info.action_type = tc_action_params->general.type;
	if (tc_action_params->general.type == TC_ACTION_TYPE_PEDIT_FAMILY) {
		struct tc_pedit_action_data old_pedit_action_data;

		memcpy(&old_pedit_action_data, &action_info.action_data.pedit, sizeof(old_pedit_action_data));

		TC_CHECK_ERROR(prepare_pedit_action_info(tc_action_params, &action_info));

		TC_CHECK_ERROR(add_pedit_action_list_to_table(&action_info.action_data.pedit));

		write_log(LOG_INFO, "delete all pedit data that related to action index %d", action_info.nps_index);
		TC_CHECK_ERROR(delete_and_free_pedit_action_entries(&old_pedit_action_data));

	} else {
		memcpy(&action_info.action_data, &tc_action_params->action_data, sizeof(action_info.action_data));
	}

	TC_CHECK_ERROR(modify_tc_action_on_db(tc_action_params, &action_info));

	TC_CHECK_ERROR(get_tc_action_from_db(tc_action_params, &is_action_exists, &action_info, NULL, NULL));

	TC_CHECK_ERROR(modify_tc_action_on_nps(&action_info));

	write_log(LOG_INFO, "Action (family type %d, index %d) was modified successfully",
		  tc_action_params->general.family_type,
		  tc_action_params->general.index);

	return TC_API_OK;
}


/******************************************************************************
 * \brief    Get actions array of actions type
 *
 * param[in]  type           - type of the action
 * param[out] action_indexes - reference to action indexes array
 * param[out] num_of_actions - reference to number of actions on array
 *
 * \return   enum tc_api_rc - TC_API_OK       - function succeed
 *                            TC_API_FAILURE  - function failed due to wrong configuration
 *                            TC_API_DB_ERROR - function failed due to problem on DB or NPS
 */
enum tc_api_rc tc_get_actions_list(enum tc_action_type  type,
				       uint32_t           **actions_array,
				       uint32_t            *num_of_actions)

{
	/* get number of actions from this type */
	TC_CHECK_ERROR(get_type_num_of_actions_from_db(type, num_of_actions));

	/* allocate an array - this array will hold all the actions indexes */
	*actions_array = (uint32_t *)malloc((*num_of_actions) * sizeof(uint32_t));
	if (*actions_array == NULL) {
		write_log(LOG_ERR, "failed to allocate memory");
		return TC_API_FAILURE;
	}

	/* fill the array with all the actions indexes related to this type */
	TC_CHECK_ERROR(get_type_action_indexes_from_db(type, *actions_array, *num_of_actions));

	return TC_API_OK;
}

/* get statistic counters */
enum tc_api_rc tc_get_action_stats(uint32_t   nps_index,
				   uint64_t  *in_packets,
				   uint64_t  *in_bytes)
{
	if (infra_get_double_counters(TC_ACTION_STATS_ON_DEMAND_OFFSET+nps_index*TC_NUM_OF_ACTION_ON_DEMAND_STATS,
				      1,
				      in_packets,
				      in_bytes) == false) {
		write_log(LOG_ERR, "error while reading double counters of action index %d", nps_index);
		return TC_API_FAILURE;
	}

	return TC_API_OK;
}

/******************************************************************************
 * \brief    Get action data, statistics and timestamps
 *
 * param[in]  type             - type of the action
 * param[in] action_indexe     - action index
 * param[out] tc_action        - reference to action information (statistics, timestamp, configuration)
 * param[out] is_action_exists - reference to bool variable that indicates if action was found on DB
 *
 * \return   enum tc_api_rc - TC_API_OK       - function succeed
 *                            TC_API_FAILURE  - function failed due to wrong configuration
 *                            TC_API_DB_ERROR - function failed due to problem on DB or NPS
 */
enum tc_api_rc tc_get_action_info(enum tc_action_type family_type,
				      uint32_t action_index,
				      struct tc_action *tc_action,
				      bool *is_action_exist)
{
	struct action_info  action_info;
	bool                rc;
	struct tc_action    tc_action_params;
	uint32_t            last_used_timestamp;
	uint32_t            rtc_seconds;


	tc_action_params.general.index       = action_index;
	tc_action_params.general.family_type = family_type;

	/* fill Linux info */
	TC_CHECK_ERROR(get_tc_action_from_db(&tc_action_params, is_action_exist, &action_info, NULL, tc_action));

	if (*is_action_exist != true) {
		return TC_API_OK;
	}
	/* fill statistic counters */
	TC_CHECK_ERROR(tc_get_action_stats(action_info.nps_index,
					   &tc_action->action_statistics.packet_statictics,
					   &tc_action->action_statistics.bytes_statictics));

	/* fill used timestamp */
	rc = infra_read_real_time_clock_seconds(&rtc_seconds);
	if (rc != true) {
		write_log(LOG_CRIT, "Failed to read RTC.");
		return TC_API_FAILURE;
	}

	TC_CHECK_ERROR(get_last_used_action_timestamp(action_info.nps_index, &last_used_timestamp));
	tc_action->action_statistics.created_timestamp = rtc_seconds - tc_action->action_statistics.created_timestamp;
	tc_action->action_statistics.use_timestamp     = rtc_seconds - last_used_timestamp;

	return TC_API_OK;
}

/******************************************************************************
 * \brief    init actions index pools
 *
 * \return   bool
 */
bool tc_action_init(void)
{
	/* init index pools */
	if (index_pool_init(&action_index_pool, TC_NUM_OF_ACTIONS) == false) {
		write_log(LOG_CRIT, "Failed to init action index pool.");
		return TC_API_DB_ERROR;
	}

	/* init action_extra_data_index_pool for extra parameters of action */
	if (index_pool_init(&action_extra_info_index_pool, TC_NUM_OF_ACTIONS) == false) {
		write_log(LOG_CRIT, "Failed to init action_extra_data_index_pool.");
		return TC_API_DB_ERROR;
	}

	return true;
}

/******************************************************************************
 * \brief    close action (index pools)
 *
 * \return   bool
 */
void tc_action_destroy(void)
{
	/* destroy action index pool */
	index_pool_destroy(&action_index_pool);
	index_pool_destroy(&action_extra_info_index_pool);
}

void build_nps_action_info_key(struct action_info *action_info, struct tc_action_key *nps_action_key)
{
	memset(nps_action_key, 0, sizeof(struct tc_action_key));
	nps_action_key->action_index = bswap_32(action_info->nps_index);
}

void build_nps_action_info_result(struct action_info *action_info,
				  struct tc_action_result *nps_action_result)
{
	uint32_t family_action_type;

	memset(nps_action_result, 0, sizeof(struct tc_action_result));
	nps_action_result->action_stats_addr = bswap_32(action_info->statistics_base.raw_data);
	nps_action_result->action_type = (uint8_t)action_info->action_type;

	family_action_type = action_info->action_id.action_family_type;

	switch (family_action_type) {
	case (TC_ACTION_TYPE_MIRRED_FAMILY):
		nps_action_result->action_data.mirred.output_if     = action_info->action_data.mirred.ifindex;
		nps_action_result->action_data.mirred.is_output_lag = false;
	break;
	case (TC_ACTION_TYPE_PEDIT_FAMILY):
		if (action_info->action_data.pedit.num_of_keys) {
			nps_action_result->action_data.action_extra_info_index =
					bswap_32(action_info->action_data.pedit.key_data[0].key_index);
			nps_action_result->control = action_info->action_data.pedit.control_action_type;
		} else {
			nps_action_result->action_data.action_extra_info_index = NOT_VALID_ACTION_INDEX;
		}
	break;
	default:
		nps_action_result->action_data.action_extra_info_index = NOT_VALID_ACTION_INDEX;
	break;
	}
}

enum tc_api_rc add_pedit_action_list_to_table(struct tc_pedit_action_data *pedit)
{
	int i;
	struct tc_action_key nps_action_key;

	write_log(LOG_INFO, "PEDIT action - num_of_keys %d", pedit->num_of_keys);

	for (i = 0; i < pedit->num_of_keys; i++) {
		/* build key for action DP table */

		struct tc_pedit_action_info_result pedit_action_info_result;
		struct pedit_key_data *key_data = &pedit->key_data[i];

		write_log(LOG_INFO, "key_index  START 0x%x", pedit->key_data[i].key_index);
		/*build key for action_data_extra_params*/
		memset(&nps_action_key, 0, sizeof(struct tc_action_key));
		nps_action_key.action_index = bswap_32(key_data->key_index);
		write_log(LOG_INFO, "key_index  0x%x", key_data->key_index);

		memset(&pedit_action_info_result, 0, sizeof(struct tc_pedit_action_info_result));
		pedit_action_info_result.mask = key_data->pedit_key_data.mask;
		pedit_action_info_result.val = key_data->pedit_key_data.val;
		pedit_action_info_result.off = bswap_32(key_data->pedit_key_data.off);
		pedit_action_info_result.at = key_data->pedit_key_data.at;
		pedit_action_info_result.offmask = key_data->pedit_key_data.offmask;
		pedit_action_info_result.shift = key_data->pedit_key_data.shift;


		if ((i+1) == pedit->num_of_keys) {
			pedit_action_info_result.next_key_index = NOT_VALID_ACTION_INDEX;
			pedit_action_info_result.is_next_key_valid = false;

		} else {
			pedit_action_info_result.next_key_index = bswap_32(pedit->key_data[i+1].key_index);
			pedit_action_info_result.is_next_key_valid = true;
		}

		write_log(LOG_DEBUG, "Adding action info to table, index 0x%08x, next_action_index 0x%08x, "
			"mask 0x%08x, val 0x%08x, off 0x%08x, at 0x%08x, offmask 0x%08x, shift 0x%08x",
			  nps_action_key.action_index,
			  pedit_action_info_result.next_key_index,
			  key_data->pedit_key_data.mask,
			  key_data->pedit_key_data.val,
			  key_data->pedit_key_data.off,
			  key_data->pedit_key_data.at,
			  key_data->pedit_key_data.offmask,
			  key_data->pedit_key_data.shift);

		/* add entry to action DP table */
		if (infra_add_entry(STRUCT_ID_TC_ACTION_EXTRA_INFO,
				    &nps_action_key,
				    sizeof(struct tc_action_key),
				    &pedit_action_info_result,
				    sizeof(struct tc_pedit_action_info_result)) == false) {
			write_log(LOG_CRIT, "Failed to add Pedit action info entry.");
			return TC_API_DB_ERROR;
		}
	}

	return TC_API_OK;



}

enum tc_api_rc add_tc_action_to_nps_table(struct action_info *action_info)
{
	struct tc_action_key        nps_action_key;
	struct tc_action_result     nps_action_result;
	struct tc_timestamp_key     nps_timestamp_key;
	struct tc_timestamp_result  nps_timestamp_result;
	uint32_t                    family_action_type;

	family_action_type = action_info->action_id.action_family_type;
	switch (family_action_type) {
	case (TC_ACTION_TYPE_PEDIT_FAMILY):
		write_log(LOG_INFO, "key_index 2 = 0x%x", action_info->action_data.pedit.key_data[0].key_index);
		if (add_pedit_action_list_to_table(&action_info->action_data.pedit) != TC_API_OK) {
			write_log(LOG_CRIT, "Failed to add pedit action info.");
			return TC_API_DB_ERROR;
		}
	break;
	default:
	break;
	}

	/* build key for action DP table */
	build_nps_action_info_key(action_info,
				  &nps_action_key);
	write_log(LOG_INFO, "actions entry key is %d", action_info->action_id.linux_action_index);

	/* build result for action DP table */
	build_nps_action_info_result(action_info,
				     &nps_action_result);

	/* add entry to action DP table */
	if (infra_add_entry(STRUCT_ID_TC_ACTION_INFO,
			    &nps_action_key,
			    sizeof(struct tc_action_key),
			    &nps_action_result,
			    sizeof(struct tc_action_result)) == false) {
		write_log(LOG_CRIT, "Failed to add action entry.");
		return TC_API_DB_ERROR;
	}

	/* prepare timestamp key & result */
	nps_timestamp_key.action_index        = bswap_32(action_info->nps_index);
	nps_timestamp_result.tc_timestamp_val = bswap_32(action_info->created_timestamp);

	/* add entry to action DP table */
	if (infra_add_entry(STRUCT_ID_TC_TIMESTAMPS,
			    &nps_timestamp_key,
			    sizeof(nps_timestamp_key),
			    &nps_timestamp_result,
			    sizeof(nps_timestamp_result)) == false) {
		write_log(LOG_CRIT, "Failed to add timestamp entry. key %d",
			  nps_timestamp_key.action_index);
		return TC_API_DB_ERROR;
	}

	return TC_API_OK;
}

/**************************************************************************//**
 * \brief       convert mirred action information from tc action
 *
 * \param[in]   action		- action received by tc manager layer
 * \param[out]  action_info	- converted action information
 *
 * \return      TC_API_FAILURE	- Failed to prepare info
 *				  action is not supported
 *              TC_API_OK	- prepare successfully
 */
enum tc_api_rc prepare_mirred_action_info(struct tc_action    *action,
					  struct action_info  *action_info)
{
	int32_t  output_if;

	switch (action->general.type) {
	case TC_ACTION_TYPE_MIRRED_EGR_REDIRECT:

		output_if = if_lookup_by_index(action->action_data.mirred.ifindex);
		if (output_if < 0) {
			write_log(LOG_ERR, "if_lookup_by_index failed. if_index %d",
				  action->action_data.mirred.ifindex);
			return TC_API_FAILURE;
		}
		action_info->action_data.mirred.ifindex = (uint8_t)output_if;
		break;

	case TC_ACTION_TYPE_MIRRED_EGR_MIRROR:
		write_log(LOG_ERR, "EGR_MIRROR is not supported");
		return TC_API_FAILURE;

	default:
		write_log(LOG_ERR, "unknown mirred action type %d", action->general.type);
		return TC_API_FAILURE;
	}

	return TC_API_OK;
}

enum tc_api_rc prepare_pedit_action_info(struct tc_action    *action,
					 struct action_info  *action_info)
{

	int i = 0;

	if (action->action_data.pedit.num_of_keys > MAX_NUM_OF_KEYS_IN_PEDIT_DATA) {
		write_log(LOG_ERR, "The num_of_keys = %d, exceeds the allowed maximum %d",
			  action->action_data.pedit.num_of_keys,
			  MAX_NUM_OF_KEYS_IN_PEDIT_DATA);
		return TC_API_FAILURE;
	}
	memset(&action_info->action_data.pedit,
	       0,
	       sizeof(struct tc_pedit_action_data));

	action_info->action_data.pedit.num_of_keys = action->action_data.pedit.num_of_keys;
	action_info->action_data.pedit.control_action_type = action->action_data.pedit.control_action_type;

	for (i = 0; i < action_info->action_data.pedit.num_of_keys; i++) {
		struct pedit_key_data *key_data = &action_info->action_data.pedit.key_data[i];

		/* allocate action_extra_index index for action_extra_params*/
		if (index_pool_alloc(&action_extra_info_index_pool, &key_data->key_index) == false) {
			write_log(LOG_ERR, "Can't add action_extra_index. Reached maximum.");
			return TC_API_FAILURE;
		}
		write_log(LOG_DEBUG, "Allocated action_extra_index = %d", key_data->key_index);
		memcpy(&key_data->pedit_key_data, &action->action_data.pedit.key_data[i].pedit_key_data, sizeof(struct tc_pedit_key_data));
	}

	for (; i < MAX_NUM_OF_KEYS_IN_PEDIT_DATA; i++) {
		struct pedit_key_data  *key_data = &action_info->action_data.pedit.key_data[i];

		key_data->key_index = NOT_VALID_ACTION_INDEX;
	}

	return TC_API_OK;
}

enum tc_api_rc prepare_action_info(struct tc_action *tc_action_params, struct action_info *action_info)
{
	enum tc_api_rc  tc_rc;
	uint32_t        family_action_type;

	write_log(LOG_DEBUG, "2 creating action type %d, family %d, index %d",
		  tc_action_params->general.type,
		  tc_action_params->general.family_type,
		  tc_action_params->general.index);

	/* allocate action index */
	if (index_pool_alloc(&action_index_pool, &action_info->nps_index) == false) {
		write_log(LOG_ERR, "Can't add action. Reached maximum.");
		return TC_API_FAILURE;
	}

	write_log(LOG_DEBUG, "Allocated action index = %d", action_info->nps_index);

	family_action_type = action_info->action_id.action_family_type;
	switch (family_action_type) {
	case TC_ACTION_TYPE_MIRRED_FAMILY:
		tc_rc = prepare_mirred_action_info(tc_action_params, action_info);
		if (tc_rc != TC_API_OK) {
			write_log(LOG_ERR, "prepare_mirred_action_info failed");
			return TC_API_FAILURE;
		}

		break;
	case TC_ACTION_TYPE_PEDIT_FAMILY:
		tc_rc = prepare_pedit_action_info(tc_action_params, action_info);
		if (tc_rc != TC_API_OK) {
			write_log(LOG_ERR, "prepare_general_action_info failed");
			return TC_API_FAILURE;
		}
		break;
	case TC_ACTION_TYPE_GACT_FAMILY:
	case TC_ACTION_TYPE_OTHER_FAMILY:
	default:
#if 0
	tc_rc = prepare_general_action_info(action_info);
	if (tc_rc != TC_API_OK) {
		write_log(LOG_ERR, "prepare_general_action_info failed");
		return TC_API_FAILURE;
	}
#endif
	break;
	}


	/* create statistics base */
	action_info->statistics_base.raw_data = BUILD_SUM_ADDR(EZDP_EXTERNAL_MS,
								 TC_ON_DEMAND_STATS_MSID,
								 TC_ACTION_STATS_ON_DEMAND_OFFSET + action_info->nps_index * TC_NUM_OF_ACTION_ON_DEMAND_STATS);
	write_log(LOG_DEBUG, "Statistics base is 0x%08x", action_info->statistics_base.raw_data);

	/* clear statistics */
	if (infra_clear_double_counters(TC_ACTION_STATS_ON_DEMAND_OFFSET + action_info->nps_index * TC_NUM_OF_ACTION_ON_DEMAND_STATS,
				      TC_NUM_OF_ACTION_ON_DEMAND_STATS) == false) {
		write_log(LOG_ERR, "ERROR, failed to clear statistics");
		return TC_API_FAILURE;
	}

	return TC_API_OK;
}

enum tc_api_rc delete_and_free_pedit_action_entries(struct tc_pedit_action_data *pedit)
{
	int i;

	for (i = 0; i < pedit->num_of_keys; i++) {
		if (pedit->key_data[i].key_index != NOT_VALID_ACTION_INDEX) {
			struct tc_action_key nps_action_key;

			memset(&nps_action_key, 0, sizeof(nps_action_key));
			nps_action_key.action_index = bswap_32(pedit->key_data[i].key_index);

			/* remove action from action DP table */
			if (infra_delete_entry(STRUCT_ID_TC_ACTION_EXTRA_INFO,
					       &nps_action_key,
					       sizeof(struct tc_action_key)) == false) {
				write_log(LOG_CRIT, "Failed to delete extra_action_info.");
				return TC_API_DB_ERROR;
			}

			write_log(LOG_DEBUG, "removed nps entry (key %d) from extra info action table", pedit->key_data[i].key_index);

			/* release action index */
			index_pool_release(&action_extra_info_index_pool, pedit->key_data[i].key_index);
			write_log(LOG_DEBUG, "released index %d back to action_extra_info_index_pool", pedit->key_data[i].key_index);
		}
	}
	return TC_API_OK;
}

#if 0
enum tc_api_rc delete_and_free_action_entry(struct action_info *action_info)
{
	uint32_t family_action_type;

	struct tc_action_key nps_action_key;

	memset(&nps_action_key, 0, sizeof(nps_action_key));
	nps_action_key.action_index = bswap_32(action_info->nps_index);

	/* remove action from action DP table */
	if (infra_delete_entry(STRUCT_ID_TC_ACTION_INFO,
			       &nps_action_key,
			       sizeof(struct tc_action_key)) == false) {
		write_log(LOG_CRIT, "Failed to delete from classifier table.");
		return TC_API_DB_ERROR;
	}

	/* release action index */
	index_pool_release(&action_index_pool, action_info->nps_index);

	family_action_type = action_info->action_id.action_type & TC_ACTION_FAMILY_MASK;
	switch (family_action_type) {
	case (TC_ACTION_TYPE_PEDIT_FAMILY):
		delete_and_free_pedit_action_entries(&action_info->action_data.pedit);
	break;
	default:
	break;
	}

	write_log(LOG_DEBUG, "Action index %d was deleted successfully from nps action table and index was released to actions pool", action_info->nps_index);

	return TC_API_OK;
}
#endif

enum tc_api_rc delete_tc_action_from_nps(struct action_info *action_info)
{
	struct tc_action_key nps_action_key;
	uint32_t family_action_type;

	/* build key for action DP table */
	build_nps_action_info_key(action_info,
				  &nps_action_key);

	/* remove action from action DP table */
	if (infra_delete_entry(STRUCT_ID_TC_ACTION_INFO,
			       &nps_action_key,
			       sizeof(struct tc_action_key)) == false) {
		write_log(LOG_CRIT, "Failed to delete from classifier table.");
		return TC_API_DB_ERROR;
	}

	/* release action index */
	index_pool_release(&action_index_pool, action_info->nps_index);

	family_action_type = action_info->action_id.action_family_type;
	switch (family_action_type) {
	case TC_ACTION_TYPE_PEDIT_FAMILY:
		write_log(LOG_INFO, "delete all pedit data that related to action index %d", action_info->nps_index);
		TC_CHECK_ERROR(delete_and_free_pedit_action_entries(&action_info->action_data.pedit));
		break;
	default:
		break;
	}

	write_log(LOG_DEBUG, "Action index %d was deleted successfully from nps action table and index was released to actions pool", action_info->nps_index);

	return TC_API_OK;
}

enum tc_api_rc get_last_used_action_timestamp(uint32_t  action_index,
				       uint32_t  *last_used_timestamp)
{
	struct tc_timestamp_key     timestamp_key;
	struct tc_timestamp_result  timestamp_result;
	bool                        rc;

	timestamp_key.action_index = bswap_32(action_index);
	rc = infra_get_entry(STRUCT_ID_TC_TIMESTAMPS,
			&timestamp_key,
			sizeof(timestamp_key),
			&timestamp_result,
			sizeof(timestamp_result));
	if (rc == false) {
		return TC_API_DB_ERROR;
	}

	*last_used_timestamp = bswap_32(timestamp_result.tc_timestamp_val);

	return TC_API_OK;
}

enum tc_api_rc modify_tc_action_on_nps(struct action_info *action_info)
{
	struct tc_action_key nps_action_key;
	struct tc_action_result nps_action_result;

	build_nps_action_info_key(action_info, &nps_action_key);
	build_nps_action_info_result(action_info, &nps_action_result);

	if (infra_modify_entry(STRUCT_ID_TC_ACTION_INFO,
			    &nps_action_key,
			    sizeof(struct tc_action_key),
			    &nps_action_result,
			    sizeof(struct tc_action_result)) == false) {
		write_log(LOG_CRIT, "Failed to modify action on actions table.");
		return TC_API_DB_ERROR;
	}

	return TC_API_OK;
}

enum tc_api_rc tc_int_add_action(struct tc_action *tc_action_params,
				 struct action_info *action_info,
				 bool independent_action)
{
	write_log(LOG_INFO, "Add Action (family type %d, index %d)",
		  tc_action_params->general.family_type,
		  tc_action_params->general.index);

	/* set action id values */
	action_info->action_id.action_family_type = tc_action_params->general.family_type;
	action_info->action_id.linux_action_index = tc_action_params->general.index;
	action_info->action_type = tc_action_params->general.type;

	/* allocate indexes for actions */
	action_info->independent_action = independent_action;

	TC_CHECK_ERROR(prepare_action_info(tc_action_params, action_info));

	/* add action to SQL DB */
	TC_CHECK_ERROR(add_tc_action_to_db(tc_action_params, action_info));

	/* build key for action DP table */
	TC_CHECK_ERROR(add_tc_action_to_nps_table(action_info));

	write_log(LOG_INFO, "Action (family type %d, index %d) was added successfully",
		  tc_action_params->general.family_type,
		  tc_action_params->general.index);

	return TC_API_OK;
}

enum tc_api_rc delete_and_free_action_info_entries(uint32_t num_of_actions, struct action_info *action_info)
{
	uint32_t i;

	for (i = 0; i < num_of_actions; i++) {
		struct tc_action tc_action_params;

		/* delete action on nps */
		TC_CHECK_ERROR(delete_tc_action_from_nps(&action_info[i]));

		tc_action_params.general.index = action_info[i].action_id.linux_action_index;
		 tc_action_params.general.family_type = action_info[i].action_id.action_family_type;

		/* delete action from db */
		TC_CHECK_ERROR(delete_tc_action_from_db(&tc_action_params));
	}

	return TC_API_OK;
}

enum tc_api_rc delete_old_filter_actions(uint32_t old_num_of_actions,
					 struct action_id *old_action_id_array,
					 uint32_t new_num_of_actions,
					 struct action_info *new_action_info_array)
{
	uint32_t i, j;

	for (i = 0; i < old_num_of_actions; i++) {
		bool found_on_new_actions = false;

		/* check this "old" action is part of the new actions */
		for (j = 0; j < new_num_of_actions; j++) {
			if (old_action_id_array[i].linux_action_index == new_action_info_array[j].action_id.linux_action_index &&
				old_action_id_array[i].action_family_type == new_action_info_array[j].action_id.action_family_type) {
				found_on_new_actions = true;
				break;
			}
		}

		if (found_on_new_actions == false) {
			struct tc_action tc_action;

			tc_action.general.index = old_action_id_array[i].linux_action_index;
			tc_action.general.family_type = old_action_id_array[i].action_family_type;
			TC_CHECK_ERROR(tc_unbind_action_from_filter(&tc_action));
		}
	}

	return TC_API_OK;
}

enum tc_api_rc delete_and_free_action_entries(uint32_t num_of_actions, struct action_id *action_id_array)
{
	uint32_t i;

	for (i = 0; i < num_of_actions; i++) {
		struct tc_action tc_action;

		tc_action.general.index = action_id_array[i].linux_action_index;
		tc_action.general.family_type = action_id_array[i].action_family_type;
		TC_CHECK_ERROR(tc_unbind_action_from_filter(&tc_action));
	}

	return TC_API_OK;
}

enum tc_api_rc tc_unbind_action_from_filter(struct tc_action *tc_action_params)
{
	struct action_info action_info;
	bool is_action_exists;
	int bind_count;

	/* get action info from database */
	TC_CHECK_ERROR(get_tc_action_from_db(tc_action_params, &is_action_exists, &action_info, NULL, NULL));

	if (is_action_exists == false) {
		write_log(LOG_ERR, "deleted action is not exist on db");
		return TC_API_FAILURE;
	}

	write_log(LOG_DEBUG, "UnBind count for action (index %d, family type %d) is %d",
		  tc_action_params->general.index,
		  tc_action_params->general.family_type,
		  bind_count);

	/* reduce bind count by one and update on DB */
	TC_CHECK_ERROR(decrement_tc_action_bind_value(tc_action_params, &bind_count));

	/* if this action is independent (was created by seperatly action add api) return */
	if (action_info.independent_action == true) {
		write_log(LOG_INFO, "action independent - not deleting action");
		return TC_API_OK;
	}

	/* if this action was created inside tc filter add (not independent) */
	/* check if action is binded to other filters */
	if (bind_count == 0) {

		TC_CHECK_ERROR(delete_tc_action_from_nps(&action_info));

		TC_CHECK_ERROR(delete_tc_action_from_db(tc_action_params));
	} else {
		write_log(LOG_INFO, "cannot delete action, action is binded to other filters");
	}

	return TC_API_OK;
}

