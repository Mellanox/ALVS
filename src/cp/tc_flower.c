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
*  File:                tc_flower.c
*  Desc:                Implementation of tc (traffic control) internal functions API.
*
*/

#include "tc_action.h"
#include "tc_flower.h"
#include "tc_db.h"
#include "byteswap.h"
#include "nw_ops.h"
#include "index_pool.h"

/* Global pointer to the DB */
struct index_pool rules_list_index_pool;
struct index_pool filter_actions_index_pool;

/*******************************************************************************************/
/******************************** Public Functions *****************************************/
/*******************************************************************************************/
/*******************************************************************************************/

/*
 * \brief       Init internal DB
 *
 * \param[in]   none
 *
 * \return      true on success, false on failure
 */
bool tc_flower_init(void)
{
	if (index_pool_init(&rules_list_index_pool, TC_FILTER_RULES_TABLE_SIZE) == false) {
		write_log(LOG_CRIT, "Failed to init rules_list index pool.");
		return TC_API_DB_ERROR;
	}

	if (index_pool_init(&filter_actions_index_pool, TC_FILTER_ACTIONS_TABLE_SIZE) == false) {
		write_log(LOG_CRIT, "Failed to init rules_list index pool.");
		return TC_API_DB_ERROR;
	}

	return true;
}

/**************************************************************************//**
 * \brief       Destroy internal DB
 *
 * \param[in]   none
 *
 * \return      none
 */
void tc_flower_destroy(void)
{
	/* destroy action index pool */
	index_pool_destroy(&filter_actions_index_pool);
	index_pool_destroy(&rules_list_index_pool);
}

enum tc_api_rc tc_add_flower_filter(struct tc_filter *tc_filter_params_origin)
{
	struct tc_filter tc_filter_params;
	bool is_exist;

	/* copy all parameters to internal variable */
	TC_CHECK_ERROR(tc_filter_params_fill(tc_filter_params_origin, &tc_filter_params));

	/* check if filter already exist on system */
	TC_CHECK_ERROR(check_if_filter_exist_on_db(&tc_filter_params, &is_exist));

	if (is_exist) {
		/* if exist execute modify filter instead */
		write_log(LOG_DEBUG, "Filter exists, modify this flower filter");
		TC_CHECK_ERROR(tc_modify_flower_filter(&tc_filter_params));
	} else {
		/* if not exist execute add filter to system */
		write_log(LOG_DEBUG, "Filter not exist, add flower filter");
		TC_CHECK_ERROR(tc_int_add_flower_filter(&tc_filter_params));
	}

	return TC_API_OK;
}

enum tc_api_rc tc_delete_flower_filter(struct tc_filter *tc_filter_params_origin)
{

	struct tc_filter tc_filter_params;
	bool is_exist;

	TC_CHECK_ERROR(tc_filter_params_fill(tc_filter_params_origin, &tc_filter_params));

	/**************************************************************************************/
	/*****check if specific filter should be deleted (interface, priority, handle)*********/
	/*****or all filters on the specific priority should be deleted (interface, priority)*/
	/**************************************************************************************/
#ifndef NDEBUG
	print_flower_filter(&tc_filter_params);
#endif
	if (tc_filter_params.handle == 0) {
		write_log(LOG_DEBUG, "tc_delete_all_priority_flower_filters");
		TC_CHECK_ERROR(tc_delete_all_priority_flower_filters(&tc_filter_params));
	} else {
		TC_CHECK_ERROR(check_if_filter_exist_on_db(&tc_filter_params, &is_exist));
		if (is_exist) {
			TC_CHECK_ERROR(tc_int_delete_flower_filter(&tc_filter_params));
		}
	}

	return TC_API_OK;
}

enum tc_api_rc add_filter_actions(struct tc_filter *tc_filter_params, struct action_info *action_info_array)
{
	uint32_t i;

	for (i = 0; i < tc_filter_params->actions.num_of_actions; i++) {

		TC_CHECK_ERROR(tc_int_add_action(&tc_filter_params->actions.action[i], &action_info_array[i], false));
	}

	return TC_API_OK;
}

enum tc_api_rc tc_modify_flower_filter(struct tc_filter *tc_filter_params)
{
	struct tc_mask_info tc_mask_info;
	struct action_info new_action_info_array[MAX_NUM_OF_ACTIONS_IN_FILTER];
	struct action_id old_action_id_array[MAX_NUM_OF_ACTIONS_IN_FILTER];
	bool is_highest_handle, is_key_changed;
	uint32_t rules_list_size = 0, old_rules_list_size = 0, old_num_of_actions = 0;
	uint32_t second_rule_list_index = 0, old_second_rule_list_index = 0;
	struct tc_filter old_tc_filter_params;
	struct rules_list_item first_rule_list_item, old_first_rule_list_item;
	uint32_t new_filter_actions_index, old_filter_actions_index;

	memset(new_action_info_array, 0, sizeof(struct action_info)*MAX_NUM_OF_ACTIONS_IN_FILTER);
	memset(old_action_id_array, 0, sizeof(struct action_id)*MAX_NUM_OF_ACTIONS_IN_FILTER);
	memset(&tc_mask_info, 0, sizeof(struct tc_mask_info));
	memset(&old_tc_filter_params, 0, sizeof(struct tc_filter));
	memset(&first_rule_list_item, 0, sizeof(struct rules_list_item));

	/* check supported mask */
	if (create_mask_info(&tc_filter_params->flower_rule_policy, &tc_mask_info) == false) {
		write_log(LOG_NOTICE, "Unsupported Mask Filter");
		return TC_API_FAILURE;
	}

	/**************************************************************************************/
	/******************** add filter actions to DP table & DB (SQL) ***********************/
	/**************************************************************************************/
	TC_CHECK_ERROR(add_filter_actions(tc_filter_params, new_action_info_array));

	/*****************************************************************************************/
	/****************** add the filter actions to an array on filter actions table ***********/
	/*****************************************************************************************/
	TC_CHECK_ERROR(add_actions_to_filter_actions_table(new_action_info_array,
							   tc_filter_params->actions.num_of_actions,
							   &new_filter_actions_index));

	/**************************************************************************************/
	/************************ get old actions info indexes ********************************/
	/**************************************************************************************/
	TC_CHECK_ERROR(get_flower_filter_actions(tc_filter_params,
						 old_action_id_array,
						 &old_num_of_actions,
						 &old_filter_actions_index));

	/**************************************************************************************/
	/************************ get old actions info indexes ********************************/
	/**************************************************************************************/
	TC_CHECK_ERROR(get_old_filter_key(tc_filter_params, &old_tc_filter_params, &is_key_changed));

	/**************************************************************************************/
	/********** update filter on SQL DB - mask, priority & handle will not be changed *****/
	/**********     all other values will be changed (fields values, actions)         *****/
	/**************************************************************************************/
	TC_CHECK_ERROR(tc_modify_flower_filter_on_db(tc_filter_params,
						     new_action_info_array,
						     new_filter_actions_index));

	/**************************************************************************************/
	/********** check if it is the highest handle in the same key & mask & priority *******/
	/**************************************************************************************/
	TC_CHECK_ERROR(check_if_handle_is_highest(tc_filter_params, &is_highest_handle));
	if (is_highest_handle == true) {

		/*****************************************************************************************/
		/************************* insert to filter_rules DP table *******************************/
		/* create a list of all the same key&mask (different priorities) order by priority value */
		/*****************************************************************************************/
		TC_CHECK_ERROR(update_rules_list_entries(tc_filter_params,
							 &first_rule_list_item,
							 &second_rule_list_index,
							 &rules_list_size));

		if (is_key_changed == true) {
			write_log(LOG_DEBUG, "filter of key changed, update the old rules list");

			/* if the key changed on modify we need to update the list for the new key and list for the old key */
			TC_CHECK_ERROR(update_rules_list_entries(&old_tc_filter_params,
								 &old_first_rule_list_item,
								 &old_second_rule_list_index,
								 &old_rules_list_size));
		}
		/**************************************************************************************/
		/************* update the new entry on classifier table (with the new key) ************/
		/**************************************************************************************/
		TC_CHECK_ERROR(add_entry_to_classifier_table(tc_filter_params,
							     tc_mask_info,
							     &first_rule_list_item,
							     second_rule_list_index,
							     true));

		/**************************************************************************************/
		/************* update the old entry on classifier table (with the old key) ************/
		/**************************************************************************************/
		if (is_key_changed == true) {
			write_log(LOG_DEBUG, "The old rules list size is %d", old_rules_list_size);
			if (old_rules_list_size == 0) {
				/* remove entry from classifier table */
				union tc_classifier_key nps_classifier_key;

				build_nps_tc_classifier_key(&old_tc_filter_params,
							    &tc_mask_info,
							    &nps_classifier_key);

				write_log(LOG_DEBUG, "nps_classifier_key.dst_ip = %08x", nps_classifier_key.dst_ip);
				if (infra_delete_entry(STRUCT_ID_TC_CLASSIFIER,
						    &nps_classifier_key,
						    sizeof(union tc_classifier_key)) == false) {
					write_log(LOG_CRIT, "Failed to delete from classifier table.");
					return TC_API_DB_ERROR;
				}
				write_log(LOG_DEBUG, "The old classifier entry was deleted");

			} else {
				/* if the key changed on modify we need to modify the  */
				TC_CHECK_ERROR(add_entry_to_classifier_table(&old_tc_filter_params,
									     tc_mask_info,
									     &old_first_rule_list_item,
									     old_second_rule_list_index,
									     true));
			}
		}
	}

	/**************************************************************************************/
	/******************** remove tc filter_actions_index & entry **************************/
	/**************************************************************************************/
	TC_CHECK_ERROR(delete_and_free_filter_actions_entry(old_filter_actions_index));

	/**************************************************************************************/
	/******************** remove entries from action table ********************************/
	/**************************************************************************************/
	TC_CHECK_ERROR(delete_old_filter_actions(old_num_of_actions,
						 old_action_id_array,
						 tc_filter_params->actions.num_of_actions,
						 new_action_info_array));

	return TC_API_OK;
}

enum tc_api_rc tc_api_get_filters_list(uint32_t interface,
				       uint32_t priority,
				       struct tc_filter_id *filters_array,
				       uint32_t *num_of_filters)
{
	int nps_interface;

	/* use nps interface index instead of linux interface index */
	nps_interface = if_lookup_by_index(interface);
	if (nps_interface < 0) {
		write_log(LOG_NOTICE, "Linux ifindex is not supported");
		return TC_API_FAILURE;
	}

	/* get the number of filters that we found */
	TC_CHECK_ERROR(get_num_of_flower_filters(nps_interface, priority, num_of_filters));

	/* allocate an array - this array will hold all the filter IDs */
	filters_array = (struct tc_filter_id *)malloc((*num_of_filters)*sizeof(struct tc_filter_id));
	if (filters_array == NULL) {
		write_log(LOG_ERR, "failed to allocate memory");
		return TC_API_FAILURE;
	}

	/* fill the array with all the actions indexes related to this type */
	TC_CHECK_ERROR(get_flower_filters_id_from_db(nps_interface, priority, filters_array, *num_of_filters));

	return TC_API_OK;
}

enum tc_api_rc tc_api_get_filter_info(struct tc_filter_id *tc_filter_id, struct tc_filter *tc_filter_params)
{
	bool is_filter_exists;
	int nps_interface_index;

	/* use nps interface index instead of linux interface index */
	nps_interface_index = if_lookup_by_index(tc_filter_id->ifindex);
	if (nps_interface_index < 0) {
		write_log(LOG_NOTICE, "Linux ifindex is not supported");
		return TC_API_FAILURE;
	}
	tc_filter_params->ifindex = nps_interface_index;
	tc_filter_params->priority = tc_filter_id->priority;
	tc_filter_params->handle = tc_filter_id->handle;

	/* get filter details from database */
	TC_CHECK_ERROR(get_flower_filter_from_db(tc_filter_params, &is_filter_exists));

	if (is_filter_exists == false) {
		write_log(LOG_NOTICE, "filter (interface %d, priority %d, handle %d) is not exist on database",
			  tc_filter_params->ifindex,
			  tc_filter_params->priority,
			  tc_filter_params->handle);

		return TC_API_FAILURE;
	}

	return TC_API_OK;
}

#if 0
enum tc_api_rc get_flower_filter_statistics(struct tc_filter *tc_filter_params,
					    struct action_stats  *action_stats_array)
{
	uint32_t i, num_of_actions = 0, created_timestamp;
	uint64_t counter_values[TC_NUM_OF_ACTION_ON_DEMAND_STATS];
	struct action_info action_info[MAX_NUM_OF_ACTIONS_IN_FILTER];
	struct tc_filter tc_filter_params;
	uint32_t counter_index;

	/* set the key for the filters_table sql table (handle, priority and interface) */
	tc_filter_params.handle   = handle;
	tc_filter_params.priority = priority;
	tc_filter_params.ifindex  = ingress_interface;

	TC_CHECK_ERROR(get_flower_filter_action_info(&tc_filter_params, action_info, &num_of_actions));

	TC_CHECK_ERROR(get_filter_created_timestamp(&tc_filter_params, &created_timestamp));

	for (i = 0; i < num_of_actions; i++) {
		/*TODO alla - I think it should be * TC_NUM_OF_ACTION_ON_DEMAND_STATS*/

		/* get statistic counters */
		counter_index = TC_ACTION_STATS_ON_DEMAND_OFFSET +
			(action_info[i].action_index * TC_NUM_OF_ACTION_ON_DEMAND_STATS);

		if (infra_get_long_counters(counter_index,
					    TC_NUM_OF_ACTION_ON_DEMAND_STATS,
					    counter_values) == false) {
			return TC_API_DB_ERROR;
		}

		action_stats_array[i].in_packets        = counter_values[TC_ACTION_IN_PACKET];
		action_stats_array[i].in_bytes          = counter_values[TC_ACTION_IN_BYTES];

		/* get timestamp */
		action_stats_array[i].created_timestamp = created_timestamp;
		TC_CHECK_ERROR(get_last_used_action_timestamp(action_info[i].action_index,
						       &action_stats_array[i].last_used_timestamp));

		/* get action type */
		action_stats_array[i].action_type = action_info[i].action_type;
	}

	return TC_API_OK;
}
#endif

/********************************************************************************************/
/*******************************          Functions            ******************************/
/********************************************************************************************/

void print_flower_filter(struct tc_filter *tc_filter_handle)
{

	struct tc_flower_rule_policy *flower_rule_policy = &tc_filter_handle->flower_rule_policy;

	write_log(LOG_INFO, "prio 0x%x, Proto 0x%x, handle 0x%x, ifindex 0x%x, flags 0x%x, type %s",
	  tc_filter_handle->priority,
	  bswap_16(tc_filter_handle->protocol),
	  tc_filter_handle->handle,
	  tc_filter_handle->ifindex,
	  tc_filter_handle->flags,
	  (tc_filter_handle->type == TC_FILTER_FLOWER) ? "flower" : "NOT SUPPORTED");

	if (tc_filter_handle->flower_rule_policy.mask_bitmap.is_eth_dst_set == 1) {
		write_log(LOG_INFO, "key_eth_dst %02x:%02x:%02x:%02x:%02x:%02x",
			  flower_rule_policy->key_eth_dst[0],
			  flower_rule_policy->key_eth_dst[1],
			  flower_rule_policy->key_eth_dst[2],
			  flower_rule_policy->key_eth_dst[3],
			  flower_rule_policy->key_eth_dst[4],
			  flower_rule_policy->key_eth_dst[5]);
		write_log(LOG_INFO, "mask_eth_dst %02x:%02x:%02x:%02x:%02x:%02x",
			  flower_rule_policy->mask_eth_dst[0],
			  flower_rule_policy->mask_eth_dst[1],
			  flower_rule_policy->mask_eth_dst[2],
			  flower_rule_policy->mask_eth_dst[3],
			  flower_rule_policy->mask_eth_dst[4],
			  flower_rule_policy->mask_eth_dst[5]);
	}


	if (tc_filter_handle->flower_rule_policy.mask_bitmap.is_eth_src_set == 1) {
		write_log(LOG_INFO, "key_eth_src %02x:%02x:%02x:%02x:%02x:%02x",
			  flower_rule_policy->key_eth_src[0],
			  flower_rule_policy->key_eth_src[1],
			  flower_rule_policy->key_eth_src[2],
			  flower_rule_policy->key_eth_src[3],
			  flower_rule_policy->key_eth_src[4],
			  flower_rule_policy->key_eth_src[5]);
		write_log(LOG_INFO, "mask_eth_src %02x:%02x:%02x:%02x:%02x:%02x",
			  flower_rule_policy->mask_eth_src[0],
			  flower_rule_policy->mask_eth_src[1],
			  flower_rule_policy->mask_eth_src[2],
			  flower_rule_policy->mask_eth_src[3],
			  flower_rule_policy->mask_eth_src[4],
			  flower_rule_policy->mask_eth_src[5]);
	}

	if (flower_rule_policy->mask_bitmap.is_eth_type_set == 1) {
		write_log(LOG_INFO, "key_eth_type  0x%x", (flower_rule_policy->key_eth_type));
		write_log(LOG_INFO, "key_eth_mask  0x%x", (flower_rule_policy->mask_eth_type));
	}

	if (flower_rule_policy->mask_bitmap.is_ip_proto_set == 1) {
		write_log(LOG_INFO, "key_ip_proto  0x%x", flower_rule_policy->key_ip_proto);
		write_log(LOG_INFO, "mask_ip_proto  0x%x", flower_rule_policy->mask_ip_proto);
	}

	if (flower_rule_policy->mask_bitmap.is_ipv4_src_set == 1) {
		write_log(LOG_INFO, "key_ipv4_src  0x%x", (flower_rule_policy->key_ipv4_src));
		write_log(LOG_INFO, "mask_ipv4_src  0x%x", (flower_rule_policy->mask_ipv4_src));
	}

	if (flower_rule_policy->mask_bitmap.is_ipv4_dst_set == 1) {
		write_log(LOG_INFO, "key_ipv4_src  0x%x", (flower_rule_policy->key_ipv4_dst));
		write_log(LOG_INFO, "mask_ipv4_src  0x%x", (flower_rule_policy->mask_ipv4_dst));
	}

	if (flower_rule_policy->mask_bitmap.is_l4_src_set == 1) {
		write_log(LOG_INFO, "key_ipv4_src  0x%x",
			  (flower_rule_policy->key_l4_src));
		write_log(LOG_INFO, "mask_ipv4_src  0x%x",
			  (flower_rule_policy->mask_l4_src));
	}

	if (flower_rule_policy->mask_bitmap.is_l4_dst_set == 1) {
		write_log(LOG_INFO, "key_ipv4_dst  0x%x",
			  bswap_16(flower_rule_policy->key_l4_dst));
		write_log(LOG_INFO, "mask_ipv4_dst  0x%x",
			  bswap_16(flower_rule_policy->mask_l4_dst));

	}
}

enum tc_api_rc tc_filter_params_fill(struct tc_filter *tc_filter_params_origin, struct tc_filter *tc_filter_params)
{
	int nps_port_index;

	memcpy(tc_filter_params, tc_filter_params_origin, sizeof(struct tc_filter));
	nps_port_index = if_lookup_by_index(tc_filter_params->ifindex);
	if (nps_port_index < 0) {
		write_log(LOG_NOTICE, "Linux ifindex is not supported");
		return TC_API_FAILURE;
	}
	write_log(LOG_NOTICE, "nps_port_index %d", nps_port_index);
	tc_filter_params->ifindex = (uint32_t)nps_port_index;

	return TC_API_OK;
}

bool get_ip_mask_value(uint32_t ip_mask, uint8_t *mask_value)
{
	uint32_t i;
	uint8_t temp_mask_value = 0;

	if (ip_mask == 0xFFFFFFFF) {
		*mask_value = 0; /* full mask is set to 0 due to DP implementation */
		return true;
	}

	for (i = 0; i < 32; i++) {
		if (ip_mask & 0x80000000) {
			temp_mask_value++;
			ip_mask = ip_mask<<1;
		} else if (ip_mask == 0) {
			*mask_value = temp_mask_value;
			return true;
		} else {
			return false;
		}
	}

	return false;
}

bool is_mask_valid(struct tc_flower_rule_policy *flower_rule_policy)
{
	uint8_t full_mask[ETH_ALEN];
	uint8_t zero_mask[ETH_ALEN];

	memset(full_mask, 0xFF, sizeof(full_mask));
	memset(zero_mask, 0, sizeof(zero_mask));

	if (flower_rule_policy->mask_bitmap.is_eth_dst_set) {
		if (memcmp(flower_rule_policy->mask_eth_dst, full_mask, ETH_ALEN) != 0) {
			write_log(LOG_ERR, "Given Mask is not full");
			return false;
		}
	} else if (memcmp(flower_rule_policy->mask_eth_dst, zero_mask, ETH_ALEN) != 0) {
		write_log(LOG_ERR, "Mask bitmap is not the same as mask field");
		return false;
	}

	if (flower_rule_policy->mask_bitmap.is_eth_src_set) {
		if (memcmp(flower_rule_policy->mask_eth_src, full_mask, ETH_ALEN) != 0) {
			write_log(LOG_ERR, "Given Mask is not full");
			return false;
		}
	} else if (memcmp(flower_rule_policy->mask_eth_src, zero_mask, ETH_ALEN) != 0) {
		write_log(LOG_ERR, "Mask bitmap is not the same as mask field");
		return false;
	}

	if (flower_rule_policy->mask_bitmap.is_eth_type_set) {
		if (memcmp(&flower_rule_policy->mask_eth_type, full_mask, sizeof(flower_rule_policy->mask_eth_type)) != 0) {
			write_log(LOG_ERR, "Given Mask is not full");
			return false;
		}
	} else if (memcmp(&flower_rule_policy->mask_eth_type, zero_mask, sizeof(flower_rule_policy->mask_eth_type)) != 0) {
		write_log(LOG_ERR, "Mask bitmap is not the same as mask field");
		return false;
	}

	if (flower_rule_policy->mask_bitmap.is_ip_proto_set) {
		if (memcmp(&flower_rule_policy->mask_ip_proto, full_mask, sizeof(flower_rule_policy->mask_ip_proto)) != 0) {
			write_log(LOG_ERR, "Given Mask is not full");
			return false;
		}
	} else if (memcmp(&flower_rule_policy->mask_ip_proto, zero_mask, sizeof(flower_rule_policy->mask_ip_proto)) != 0) {
		write_log(LOG_ERR, "Mask bitmap is not the same as mask field");
		return false;
	}

	if (flower_rule_policy->mask_bitmap.is_l4_dst_set) {
		if (memcmp(&flower_rule_policy->mask_l4_dst, full_mask, sizeof(flower_rule_policy->mask_l4_dst)) != 0) {
			write_log(LOG_ERR, "Given Mask is not full");
			return false;
		}
	} else if (memcmp(&flower_rule_policy->mask_l4_dst, zero_mask, sizeof(flower_rule_policy->mask_l4_dst)) != 0) {
		write_log(LOG_ERR, "Mask bitmap is not the same as mask field");
		return false;
	}

	if (flower_rule_policy->mask_bitmap.is_l4_src_set) {
		if (memcmp(&flower_rule_policy->mask_l4_src, full_mask, sizeof(flower_rule_policy->mask_l4_src)) != 0) {
			write_log(LOG_ERR, "Given Mask is not full");
			return false;
		}
	} else if (memcmp(&flower_rule_policy->mask_l4_src, zero_mask, sizeof(flower_rule_policy->mask_l4_src)) != 0) {
		write_log(LOG_ERR, "Mask bitmap is not the same as mask field");
		return false;
	}

	return true;
}

bool create_mask_info(struct tc_flower_rule_policy *flower_rule_policy, struct tc_mask_info *mask_info)
{

	/* copy given mask bitmap */
	memset(mask_info, 0, sizeof(struct tc_mask_info));
	mask_info->mask_bitmap.is_eth_dst_set = flower_rule_policy->mask_bitmap.is_eth_dst_set;
	mask_info->mask_bitmap.is_eth_src_set = flower_rule_policy->mask_bitmap.is_eth_src_set;
	mask_info->mask_bitmap.is_eth_type_set = flower_rule_policy->mask_bitmap.is_eth_type_set;
	mask_info->mask_bitmap.is_ip_proto_set = flower_rule_policy->mask_bitmap.is_ip_proto_set;
	mask_info->mask_bitmap.is_ipv4_dst_set = flower_rule_policy->mask_bitmap.is_ipv4_dst_set;
	mask_info->mask_bitmap.is_ipv4_src_set = flower_rule_policy->mask_bitmap.is_ipv4_src_set;
	mask_info->mask_bitmap.is_l4_dst_set = flower_rule_policy->mask_bitmap.is_l4_dst_set;
	mask_info->mask_bitmap.is_l4_src_set = flower_rule_policy->mask_bitmap.is_l4_src_set;
	mask_info->mask_bitmap.is_vlan_eth_set = flower_rule_policy->mask_bitmap.is_vlan_eth_set;
	mask_info->mask_bitmap.is_vlan_id_set = flower_rule_policy->mask_bitmap.is_vlan_id_set;
	mask_info->mask_bitmap.is_vlan_prio_set = flower_rule_policy->mask_bitmap.is_vlan_prio_set;

	write_log(LOG_DEBUG, "mask bitmap flower_rule_policy is 0x%08x", *(uint16_t *)&flower_rule_policy->mask_bitmap);
	write_log(LOG_DEBUG, "mask bitmap mask_info is 0x%08x", *(uint16_t *)&mask_info->mask_bitmap);

	/* check fields for full mask given (except ipv4)*/
	if (is_mask_valid(flower_rule_policy) == false) {
		write_log(LOG_ERR, "Mask is not full");
		return false;
	}

	/* only ipv4 masks can be not full */
	/* set dsp ip mask value */
	if (mask_info->mask_bitmap.is_ipv4_dst_set) {
		if (get_ip_mask_value(flower_rule_policy->mask_ipv4_dst, &mask_info->ipv4_dst_mask_val) == false) {
			write_log(LOG_ERR, "Ipv4 dst mask is not supported");
			return false;
		}
	} else if (flower_rule_policy->mask_ipv4_dst != 0) {
		write_log(LOG_ERR, "Mask bitmap is not the same as mask field");
		return false;
	}

	/* set src ip mask value */
	if (mask_info->mask_bitmap.is_ipv4_src_set) {
		if (get_ip_mask_value(flower_rule_policy->mask_ipv4_src, &mask_info->ipv4_src_mask_val) == false) {
			write_log(LOG_ERR, "Ipv4 src mask is not supported");
			return false;
		}
	} else if (flower_rule_policy->mask_ipv4_src != 0) {
		write_log(LOG_ERR, "Mask bitmap is not the same as mask field");
		return false;
	}

	write_log(LOG_DEBUG, "Received Mask is Valid and Supported");
	write_log(LOG_DEBUG, "ipv4 src mask value is %d", mask_info->ipv4_src_mask_val);
	write_log(LOG_DEBUG, "ipv4 dst mask value is %d", mask_info->ipv4_dst_mask_val);
	write_log(LOG_DEBUG, "mask bitmap is 0x%08x", *(uint16_t *)&mask_info->mask_bitmap);
	return true;
}

enum tc_api_rc add_rules_list_entries_to_table(struct rules_list_item *rules_list, uint32_t rules_list_size)
{
	struct tc_rules_list_key nps_rules_list_key;
	struct tc_rules_list_result nps_rules_list_result;
	uint32_t i, next_rule_index;

	write_log(LOG_DEBUG, "Writing to rules_list table %d entries, starting from the last entry in the list", rules_list_size);
	/* start to copy from last to first --> will prevent race condition on DP */
	/* todo start from last to first */
	/*TODO - Alla - it was here i>1 - why? didn't work*/
	/* don't insert the first item to the RULES LIST table, the first item will be added to classifier table */
	for (i = rules_list_size-1; i > 0; i--) {

		build_nps_rules_list_key(&rules_list[i], &nps_rules_list_key);
		write_log(LOG_INFO, "build_nps_rules_list_key i = %d", i);
		write_log(LOG_INFO, "rule_index %d", nps_rules_list_key.rule_index);

		if (i == rules_list_size-1) {
			next_rule_index = NOT_VALID_RULE_INDEX;
		} else {
			next_rule_index = rules_list[i+1].rules_list_index;
		}

		build_nps_rules_list_result(&rules_list[i], next_rule_index, &nps_rules_list_result);
		write_log(LOG_INFO, "action index %d", rules_list[i].filter_actions_index);
		write_log(LOG_INFO, "priority %d", rules_list[i].priority);
		write_log(LOG_INFO, "next_rule_index %d", nps_rules_list_result.next_rule_index);
		write_log(LOG_INFO, "is_next %d", nps_rules_list_result.is_next_rule_valid);
		if (infra_modify_entry(STRUCT_ID_TC_RULES_LIST,
				    &nps_rules_list_key,
				    sizeof(struct tc_rules_list_key),
				    &nps_rules_list_result,
				    sizeof(struct tc_rules_list_result)) == false) {
			write_log(LOG_CRIT, "Failed to add rule list item to rules list table.");
			return TC_API_DB_ERROR;
		}

		write_log(LOG_DEBUG, "Rules_list Entry %d was modified successfully to: priority %d, action index 0x%08x, next rule_index 0x%08x",
			  rules_list[i].rules_list_index,
			  rules_list[i].priority,
			  rules_list[i].filter_actions_index,
			  next_rule_index);
	}

	return TC_API_OK;
}

enum tc_api_rc create_and_add_rules_list(struct tc_filter *tc_filter_params,
					 struct rules_list_item *rules_list,
					 uint32_t rules_list_size)
{
	TC_CHECK_ERROR(prepare_rules_list_items(tc_filter_params, rules_list, rules_list_size));
	write_log(LOG_DEBUG, "prepare_rules_list_items rules_list_size %d.", rules_list_size);

	if (rules_list_size > 1) {
		write_log(LOG_DEBUG, "There is more than one priority with this key&mask, adding to rules list table");

		TC_CHECK_ERROR(add_rules_list_entries_to_table(rules_list, rules_list_size));

	} else {
		write_log(LOG_DEBUG, "No need to update rules list table, there is only one priority with this key&mask");
	}

	return TC_API_OK;
}

enum tc_api_rc allocate_rule_list_indexes(struct tc_filter *tc_filter_params,
					  uint32_t filter_actions_index,
					  uint32_t *rule_list_index,
					  struct action_info *action_info)
{
	struct tc_rules_list_key nps_rules_list_key;
	struct tc_rules_list_result nps_rules_list_result;

	/* allocate index for filter rule */
	if (index_pool_alloc(&rules_list_index_pool, rule_list_index) == false) {
		write_log(LOG_ERR, "Can't add filter rule. Reached maximum.");
		delete_and_free_action_info_entries(tc_filter_params->actions.num_of_actions, action_info);
		return TC_API_FAILURE;
	}
	write_log(LOG_DEBUG, "Allocated filter rule index = %d", *rule_list_index);

	/* add entry on rules_list table */
	/*TOMER - clear NPS_RULES_LIST_RESULT*/
	memset(&nps_rules_list_key, 0, sizeof(struct tc_rules_list_key));
	nps_rules_list_key.rule_index = bswap_32(*rule_list_index);
	write_log(LOG_INFO, "nps_rules_list_key.rule_index %d", nps_rules_list_key.rule_index);

	memset(&nps_rules_list_result, 0, sizeof(struct tc_rules_list_result));
	nps_rules_list_result.filter_actions_index = bswap_32(filter_actions_index);
	nps_rules_list_result.priority = bswap_16((uint16_t)tc_filter_params->priority);
	nps_rules_list_result.is_next_rule_valid = false;
	nps_rules_list_result.next_rule_index = NOT_VALID_RULE_INDEX;

	if (infra_add_entry(STRUCT_ID_TC_RULES_LIST,
			    &nps_rules_list_key,
			    sizeof(struct tc_rules_list_key),
			    &nps_rules_list_result,
			    sizeof(struct tc_rules_list_result)) == false) {
		write_log(LOG_CRIT, "Failed to add rules list item to table.");
		/* release indexes */
		index_pool_release(&rules_list_index_pool, (uint32_t)*rule_list_index);
		return TC_API_DB_ERROR;
	}

	return TC_API_OK;
}

enum tc_api_rc add_entry_to_classifier_table(struct tc_filter *tc_filter_params,
					     struct tc_mask_info tc_mask_info,
					     struct rules_list_item *first_rule_list_item,
					     uint32_t second_rule_list_index,
					     bool modify_entry)
{
	union tc_classifier_key nps_classifier_key;
	struct tc_classifier_result nps_classifier_result;

	/* build key */
	build_nps_tc_classifier_key(tc_filter_params, &tc_mask_info, &nps_classifier_key);

	/* building result for classifier */
	memset(&nps_classifier_result, 0, sizeof(struct tc_classifier_result));
	build_nps_tc_classifier_result(first_rule_list_item->priority,
				       first_rule_list_item->filter_actions_index,
				       second_rule_list_index,
				       &nps_classifier_result);

	/* check if the second index of the rules list is valid, */
	/* if valid it means that we already have an entry on classifier table */
	/* and we need to modify the entry, if not add a new entry to classifier table */
	if (second_rule_list_index != NOT_VALID_RULE_INDEX || modify_entry == true) {
		if (infra_modify_entry(STRUCT_ID_TC_CLASSIFIER,
				    &nps_classifier_key,
				    sizeof(union tc_classifier_key),
				    &nps_classifier_result,
				    sizeof(struct tc_classifier_result)) == false) {
			write_log(LOG_CRIT, "Failed to modify filter on classifier table.");
			return TC_API_DB_ERROR;
		}
		write_log(LOG_DEBUG, "Modified a classifier entry for mask bitmap 0x%02x, result priority %d, action index 0x%08x, rule_list_index 0x%08x",
			  *(uint8_t *)&tc_mask_info.mask_bitmap,
			  first_rule_list_item->priority,
			  first_rule_list_item->filter_actions_index,
			  second_rule_list_index);
	} else {
		if (infra_add_entry(STRUCT_ID_TC_CLASSIFIER,
				    &nps_classifier_key,
				    sizeof(union tc_classifier_key),
				    &nps_classifier_result,
				    sizeof(struct tc_classifier_result)) == false) {
			write_log(LOG_CRIT, "Failed to add filter to classifier table.");
			return TC_API_DB_ERROR;
		}
		write_log(LOG_DEBUG, "Add a classifier entry for mask bitmap 0x%02x, result priority %d, action index 0x%08x, rule_list_index 0x%08x",
			  *(uint8_t *)&tc_mask_info.mask_bitmap,
			  first_rule_list_item->priority,
			  first_rule_list_item->filter_actions_index,
			  second_rule_list_index);
	}

	return TC_API_OK;
}

enum tc_api_rc update_rules_list_entries(struct tc_filter *tc_filter_params,
					 struct rules_list_item *first_rule_list_item,
					 uint32_t *second_rule_list_index,
					 uint32_t *rules_list_size)
{
	enum tc_api_rc rc;
	uint32_t list_size;
	struct rules_list_item *rules_list;

	/* get size of rules list */
	TC_CHECK_ERROR(get_rules_list_size(tc_filter_params, &list_size));

	write_log(LOG_DEBUG, "The number of filters with the same key&mask is %d (counting only the highest handle from each priority)", list_size);

	/* in this array we will save all the rules list items */
	rules_list = (struct rules_list_item *)malloc(sizeof(struct rules_list_item) * list_size);
	if (rules_list == NULL) {
		write_log(LOG_ERR, "Failed to allocate memory for rules list, allocated size was %lu", sizeof(struct rules_list_item) * list_size);
		return false;
	}
	memset(rules_list, 0, sizeof(struct rules_list_item) * list_size);

	/* add the new list to DP rules list table */
	rc = create_and_add_rules_list(tc_filter_params, rules_list, list_size);
	if (rc != TC_API_OK) {
		free(rules_list);
	}

	first_rule_list_item->filter_actions_index = rules_list[0].filter_actions_index;
	first_rule_list_item->priority = rules_list[0].priority;

	if (list_size > 1) {
		*second_rule_list_index = rules_list[1].rules_list_index;
	} else {
		*second_rule_list_index = NOT_VALID_RULE_INDEX;
	}

	*rules_list_size = list_size;

	return TC_API_OK;
}

enum tc_api_rc add_actions_to_filter_actions_table(struct action_info *action_info,
							  uint32_t num_of_actions,
							  uint32_t *filter_actions_index)
{
	struct tc_filter_actions_key nps_filter_actions_key;
	struct tc_filter_actions_result nps_filter_actions_result;

	memset(&nps_filter_actions_result, 0, sizeof(struct tc_filter_actions_result));
	memset(&nps_filter_actions_key, 0, sizeof(struct tc_filter_actions_key));

	/* allocate action index */
	if (index_pool_alloc(&filter_actions_index_pool, filter_actions_index) == false) {
		write_log(LOG_ERR, "Can't add action. Reached maximum.");
		delete_and_free_action_info_entries(num_of_actions, action_info);
		return TC_API_FAILURE;
	}

	/* building key */
	nps_filter_actions_key.filter_actions_index = bswap_32(*filter_actions_index);

	/* building result */
	build_nps_tc_filter_actions_result(&nps_filter_actions_result, num_of_actions, action_info);

	if (infra_add_entry(STRUCT_ID_TC_FILTER_ACTIONS,
			    &nps_filter_actions_key,
			    sizeof(struct tc_filter_actions_key),
			    &nps_filter_actions_result,
			    sizeof(struct tc_filter_actions_result)) == false) {
		write_log(LOG_CRIT, "Failed to add filter actions item to table.");

		return TC_API_DB_ERROR;
	}

	write_log(LOG_DEBUG, "added filter actions entry on index %d", *filter_actions_index);

	return TC_API_OK;
}

enum tc_api_rc add_filter_to_mask_table(struct tc_filter *tc_filter_params, struct tc_mask_info tc_mask_info)
{
	enum tc_api_rc rc;
	bool is_mask_exist;

	rc = check_if_mask_already_exist(tc_filter_params, &is_mask_exist);
	if (rc != TC_API_OK) {
		return rc;
	}

	/* add the mask only if this mask not exist on the DP table */
	if (is_mask_exist == false) {
		struct tc_mask_key nps_masks_key;
		struct tc_mask_result nps_masks_result;
		uint32_t new_mask_index;

		memset(&nps_masks_key, 0, sizeof(nps_masks_key));
		memset(&nps_masks_result, 0, sizeof(nps_masks_result));

		write_log(LOG_DEBUG, "The filter mask is not exist on mask table, adding this mask to mask table");
		/* check how many masks on db */
		TC_CHECK_ERROR(get_new_mask_index(tc_filter_params, &new_mask_index));

		write_log(LOG_DEBUG, "New mask index for interface %d and eth_type %d is %d",
			  tc_filter_params->ifindex,
			  tc_filter_params->flower_rule_policy.key_eth_type,
			  new_mask_index);

		/* check if we reached to the maximum number of masks supported for an interface and eth type */
		if (new_mask_index >= TC_MAX_NUM_OF_MASKS_SUPPORTED) {
			write_log(LOG_ERR,
				  "Reached to the maximum number of masks (%d) on an interface and eth type",
				  TC_MAX_NUM_OF_MASKS_SUPPORTED);
			return TC_API_FAILURE;
		}

		/* create masks table key */
		build_nps_tc_masks_key(tc_filter_params,
				      new_mask_index,
				      &nps_masks_key);

		/* create masks table result */
		build_nps_tc_masks_result(&tc_mask_info,
					  &nps_masks_result);

		/* add the new mask to internal DB masks table */
		rc = add_mask_to_mask_table_db(tc_filter_params,
					       new_mask_index,
					       tc_mask_info);
		if (rc != TC_API_OK) {
			return rc;
		}

		/* add entry to DP table */
		if (infra_add_entry(STRUCT_ID_TC_MASK_BITMAP,
				    &nps_masks_key,
				    sizeof(struct tc_mask_key),
				    &nps_masks_result,
				    sizeof(struct tc_mask_result)) == false) {
			write_log(LOG_CRIT, "Failed to add filter to masks table.");
			return TC_API_DB_ERROR;
		}
	}

	return TC_API_OK;
}

enum tc_api_rc get_the_last_mask_entry(struct tc_filter *tc_filter_params, uint32_t *last_mask_entry_index)
{
	uint32_t new_mask_index;

	/* check how many masks on db */
	TC_CHECK_ERROR(get_new_mask_index(tc_filter_params, &new_mask_index));

	/* the last mask entry is the new mask index - 1, */
	/* the last mask entry will replace the mask that we need to delete */
	*last_mask_entry_index = new_mask_index-1;

	return TC_API_OK;
}

enum tc_api_rc remove_mask_from_db_and_dp_table(struct tc_filter *tc_filter_params)
{
	uint32_t mask_index_to_delete = 0, last_mask_index = 0;
	struct tc_mask_info result_mask_info;
	struct tc_mask_key nps_masks_key;
	struct tc_mask_result nps_masks_result;

	memset(&nps_masks_key, 0, sizeof(nps_masks_key));
	memset(&nps_masks_result, 0, sizeof(nps_masks_result));

	/* get the mask index to delete */
	TC_CHECK_ERROR(get_mask_index_to_delete(tc_filter_params, &mask_index_to_delete));

	/* get the last mask entry index from db */
	TC_CHECK_ERROR(get_the_last_mask_entry(tc_filter_params, &last_mask_index));

	write_log(LOG_DEBUG, "last_mask_index = 0x%x, mask_index_to_delete = 0x%x", last_mask_index, mask_index_to_delete);

	/* delete mask from db */
	TC_CHECK_ERROR(delete_mask_entry_from_db(tc_filter_params, mask_index_to_delete));

	if (last_mask_index != mask_index_to_delete) {

		/* change the last mask index to replace the deleted mask */
		TC_CHECK_ERROR(get_result_mask_bitmap(tc_filter_params, last_mask_index, &result_mask_info));

		/* change the last mask index to replace the deleted mask */
		TC_CHECK_ERROR(replace_deleted_entry_with_last_mask_entry(tc_filter_params,
									  last_mask_index,
									  mask_index_to_delete));

		/************************************************************************************/
		/************* replace the deleted entry with the last entry on NPS table ***********/
		/************************************************************************************/
		/* create masks table key */
		build_nps_tc_masks_key(tc_filter_params,
				       mask_index_to_delete,
				       &nps_masks_key);

		/* create masks table result */
		build_nps_tc_masks_result(&result_mask_info,
					  &nps_masks_result);

		if (infra_modify_entry(STRUCT_ID_TC_MASK_BITMAP,
				       &nps_masks_key,
				       sizeof(struct tc_mask_key),
				       &nps_masks_result,
				       sizeof(struct tc_mask_result)) == false) {
			write_log(LOG_CRIT, "Failed to modify mask entry in NPS.");
			return TC_API_DB_ERROR;
		}
	}
	/************************************************************************************/
	/********************** delete the last entry on NPS table **************************/
	/************************************************************************************/
	/* create masks table key */
	build_nps_tc_masks_key(tc_filter_params,
			       last_mask_index,
			       &nps_masks_key);

	if (infra_delete_entry(STRUCT_ID_TC_MASK_BITMAP,
			       &nps_masks_key,
			       sizeof(struct tc_mask_key)) == false) {
		write_log(LOG_CRIT, "Failed to delete mask entry in NPS.");
		return TC_API_DB_ERROR;
	}

	return TC_API_OK;
}

enum tc_api_rc delete_and_free_rule_list_entry(uint32_t rule_list_index)
{
	struct tc_rules_list_key nps_rules_list_key;

	index_pool_release(&rules_list_index_pool, rule_list_index);

	/* remove from nps rules_list table */
	nps_rules_list_key.rule_index = bswap_32(rule_list_index);

	if (infra_delete_entry(STRUCT_ID_TC_RULES_LIST,
			       &nps_rules_list_key,
			       sizeof(struct tc_rules_list_key)) == false) {
		write_log(LOG_CRIT, "Failed to delete rule list entry on NPS.");
		return TC_API_DB_ERROR;
	}

	write_log(LOG_DEBUG, "Released rule list index = %d and entry was deleted", rule_list_index);

	return TC_API_OK;
}

enum tc_api_rc delete_and_free_filter_actions_entry(uint32_t filter_actions_index)
{
	struct tc_filter_actions_key nps_filter_actions_key;

	index_pool_release(&filter_actions_index_pool, filter_actions_index);

	/* remove from nps rules_list table */
	nps_filter_actions_key.filter_actions_index = bswap_32(filter_actions_index);

	if (infra_delete_entry(STRUCT_ID_TC_FILTER_ACTIONS,
			       &nps_filter_actions_key,
			       sizeof(struct tc_filter_actions_key)) == false) {
		write_log(LOG_CRIT, "Failed to delete filter actions entry on NPS.");
		return TC_API_DB_ERROR;
	}

	write_log(LOG_DEBUG, "Released filter actions index = %d and entry was deleted", filter_actions_index);

	return TC_API_OK;
}

enum tc_api_rc tc_int_delete_flower_filter(struct tc_filter *tc_filter_params)
{
	union tc_classifier_key nps_classifier_key;
	bool is_filters_registered_on_mask;
	bool is_highest_handle;
	uint16_t rule_list_index;
	struct tc_mask_info tc_mask_info;
	uint32_t rules_list_size = 0, second_rule_list_index = 0;
	struct action_id actions_id_array[MAX_NUM_OF_ACTIONS_IN_FILTER];
	struct rules_list_item first_rule_list_item;
	struct tc_filter  current_tc_filter_params;
	uint32_t filter_actions_index, num_of_actions;
	bool is_key_changed;

	memset(&tc_mask_info, 0, sizeof(tc_mask_info));
	memset(&first_rule_list_item, 0, sizeof(struct rules_list_item));
	memset(&current_tc_filter_params, 0, sizeof(struct tc_filter));
	memset(actions_id_array, 0, sizeof(struct action_id));

	/**************************************************************************************/
	/******************* get filter parameters *************************/
	/**************************************************************************************/
	TC_CHECK_ERROR(get_old_filter_key(tc_filter_params, &current_tc_filter_params, &is_key_changed));

	if (create_mask_info(&current_tc_filter_params.flower_rule_policy, &tc_mask_info) == false) {
		write_log(LOG_NOTICE, "Unsupported Mask Filter");
		return TC_API_FAILURE;
	}
	/**************************************************************************************/
	/******************************** get old actions  ************************************/
	/**************************************************************************************/
	TC_CHECK_ERROR(get_flower_filter_actions(tc_filter_params,
						 actions_id_array,
						 &num_of_actions,
						 &filter_actions_index));


	/**************************************************************************************/
	/***************************** get rule list index from db ****************************/
	/**************************************************************************************/
	TC_CHECK_ERROR(get_filter_rule_from_db(&current_tc_filter_params, &rule_list_index));

	/**************************************************************************************/
	/*************************** remove filter from db ************************************/
	/**************************************************************************************/
	TC_CHECK_ERROR(remove_filter_from_db(&current_tc_filter_params));

	/**************************************************************************************/
	/********* check if it is the highest handle in the same key & mask & priority. *******/
	/******* if it is not the highest handle we don't need to remove from DP tables *******/
	/**************************************************************************************/
	TC_CHECK_ERROR(check_if_handle_is_highest(&current_tc_filter_params, &is_highest_handle));

	if (is_highest_handle == true) {

		/*****************************************************************************************/
		/************************* insert to filter_rules DP table *******************************/
		/* create a list of all the same key&mask (different priorities) order by priority value */
		/*****************************************************************************************/
		TC_CHECK_ERROR(update_rules_list_entries(&current_tc_filter_params,
							 &first_rule_list_item,
							 &second_rule_list_index,
							 &rules_list_size));

		/* if rules list size is more than 0 --> */
		/* it means that if we will remove our filter we have more filters on our system with the same */
		/* key&mask so we need to update the classifier entry (of this key&mask), the rules list table */
		/* was updated earlier on update_rules_list_entries function */

		if (rules_list_size > 0) {
			/**************************************************************************************/
			/************************** update classifier DP table ********************************/
			/**************************************************************************************/
			TC_CHECK_ERROR(add_entry_to_classifier_table(&current_tc_filter_params,
								     tc_mask_info,
								     &first_rule_list_item,
								     second_rule_list_index,
								     true));
		} else {

			/**************************************************************************************/
			/**************** remove entry from mask DP table & mask table db *********************/
			/**************************************************************************************/
			TC_CHECK_ERROR(check_if_filters_registered_on_mask(&current_tc_filter_params,
									   &is_filters_registered_on_mask));

			if (is_filters_registered_on_mask == false) {
				write_log(LOG_DEBUG, "the deleted filter is the only filter on this mask --> deleting this mask too");
				/* remove entry from mask table only if it is the only filter that use this mask */
				TC_CHECK_ERROR(remove_mask_from_db_and_dp_table(&current_tc_filter_params));
			}

			/**************************************************************************************/
			/******************** remove entry from classifier table ******************************/
			/**************************************************************************************/
			build_nps_tc_classifier_key(&current_tc_filter_params,
						    &tc_mask_info,
						    &nps_classifier_key);

			if (infra_delete_entry(STRUCT_ID_TC_CLASSIFIER,
					    &nps_classifier_key,
					    sizeof(union tc_classifier_key)) == false) {
				write_log(LOG_CRIT, "Failed to delete from classifier table.");
				return TC_API_DB_ERROR;
			}
		}
	}


	/**************************************************************************************/
	/******************** remove tc filter_rule_index & entry *****************************/
	/**************************************************************************************/
	TC_CHECK_ERROR(delete_and_free_rule_list_entry(rule_list_index));

	/**************************************************************************************/
	/******************** remove tc filter_actions_index & entry **************************/
	/**************************************************************************************/
	TC_CHECK_ERROR(delete_and_free_filter_actions_entry(filter_actions_index));

	/**************************************************************************************/
	/******************** remove entries from action table ********************************/
	/**************************************************************************************/
	TC_CHECK_ERROR(delete_and_free_action_entries(num_of_actions, actions_id_array));

	return TC_API_OK;

}

enum tc_api_rc tc_int_add_flower_filter(struct tc_filter *tc_filter_params)
{
	enum tc_api_rc rc;
	struct action_info action_info[MAX_NUM_OF_ACTIONS_IN_FILTER];
	uint32_t rules_list_size = 0;
	struct rules_list_item first_rule_list_item;
	uint32_t current_rule_list_index, second_rule_list_index;
	bool is_highest_handle;
	struct tc_mask_info tc_mask_info;
	uint32_t filter_actions_index = 0;

	memset(action_info, 0, sizeof(struct action_info)*MAX_NUM_OF_ACTIONS_IN_FILTER);
	memset(&tc_mask_info, 0, sizeof(tc_mask_info));
	memset(&first_rule_list_item, 0, sizeof(struct rules_list_item));

	if (create_mask_info(&tc_filter_params->flower_rule_policy, &tc_mask_info) == false) {
		write_log(LOG_NOTICE, "Unsupported Mask Filter");
		return TC_API_FAILURE;
	}
	if (tc_filter_params->actions.num_of_actions > MAX_NUM_OF_ACTIONS_IN_FILTER) {
		write_log(LOG_NOTICE, "Num of actions is not supported, supporting up to %d only", MAX_NUM_OF_ACTIONS_IN_FILTER);
		return TC_API_FAILURE;
	}

	/**************************************************************************************/
	/******************** add filter actions to DP table & DB (SQL) ***********************/
	/**************************************************************************************/
	TC_CHECK_ERROR(add_filter_actions(tc_filter_params, action_info));

	/*****************************************************************************************/
	/****************** add the filter actions to an array on filter actions table ***********/
	/*****************************************************************************************/
	TC_CHECK_ERROR(add_actions_to_filter_actions_table(action_info,
							   tc_filter_params->actions.num_of_actions,
							   &filter_actions_index));

	/**************************************************************************************/
	/****************** allocate filter indexes (actions & rule_list entry) ***************/
	/**************************************************************************************/
	TC_CHECK_ERROR(allocate_rule_list_indexes(tc_filter_params,
						  filter_actions_index,
						  &current_rule_list_index,
						  action_info));

	/**************************************************************************************/
	/**************************** add filter to SQL DB ************************************/
	/**************************************************************************************/
	TC_CHECK_ERROR(tc_add_flower_filter_to_db(tc_filter_params, filter_actions_index, current_rule_list_index));

	/**************************************************************************************/
	/********** check if it is the highest handle in the same key & mask & priority *******/
	/**************************************************************************************/
	TC_CHECK_ERROR(check_if_handle_is_highest(tc_filter_params, &is_highest_handle));
	if (is_highest_handle == false) {
		/* exit function, no need to add to DP tables */
		write_log(LOG_DEBUG, "The filter is not with the highest handle (with the same priority & key & mask)");
		return TC_API_OK;
	}

	/*****************************************************************************************/
	/************************* insert to filter_rules DP table *******************************/
	/* create a list of all the same key&mask (different priorities) order by priority value */
	/*****************************************************************************************/
	TC_CHECK_ERROR(update_rules_list_entries(tc_filter_params,
						 &first_rule_list_item,
						 &second_rule_list_index,
						 &rules_list_size));

	/**************************************************************************************/
	/************************* insert to classifier DP table ******************************/
	/**************************************************************************************/
	TC_CHECK_ERROR(add_entry_to_classifier_table(tc_filter_params,
						     tc_mask_info,
						     &first_rule_list_item,
						     second_rule_list_index,
						     false));

	/**************************************************************************************/
	/************************* insert to masks DP table ***********************************/
	/**************************************************************************************/
	rc = add_filter_to_mask_table(tc_filter_params, tc_mask_info);
	if (rc != TC_API_OK) {
		delete_and_free_rule_list_entry(current_rule_list_index);
		remove_filter_from_db(tc_filter_params);
		delete_and_free_action_info_entries(tc_filter_params->actions.num_of_actions, action_info);
		return rc;
	}

	return TC_API_OK;

}

void build_nps_tc_classifier_key(struct tc_filter *tc_filter_params,
				 struct tc_mask_info *tc_mask_info,
				 union tc_classifier_key *nps_classifier_key)
{
	memset(nps_classifier_key, 0, sizeof(union tc_classifier_key));

	nps_classifier_key->tc_mask_info.ipv4_dst_mask_val = tc_mask_info->ipv4_dst_mask_val;
	nps_classifier_key->tc_mask_info.ipv4_src_mask_val = tc_mask_info->ipv4_src_mask_val;

	memcpy(&nps_classifier_key->tc_mask_info.mask_bitmap, (void *)&tc_mask_info->mask_bitmap, sizeof(union tc_mask_bitmap));

	nps_classifier_key->src_ip = tc_filter_params->flower_rule_policy.key_ipv4_src &
					      tc_filter_params->flower_rule_policy.mask_ipv4_src;
	nps_classifier_key->dst_ip = tc_filter_params->flower_rule_policy.key_ipv4_dst &
					      tc_filter_params->flower_rule_policy.mask_ipv4_dst;
	nps_classifier_key->ether_type = tc_filter_params->flower_rule_policy.key_eth_type &
						tc_filter_params->flower_rule_policy.mask_eth_type;
	nps_classifier_key->ip_proto = tc_filter_params->flower_rule_policy.key_ip_proto;
	nps_classifier_key->ingress_logical_id = bswap_32(tc_filter_params->ifindex) >> 24;	/* todo what to put here */
	nps_classifier_key->l4_dst_port = tc_filter_params->flower_rule_policy.key_l4_dst;
	nps_classifier_key->l4_src_port = tc_filter_params->flower_rule_policy.key_l4_src;
	memcpy(nps_classifier_key->dst_mac.ether_addr_octet, tc_filter_params->flower_rule_policy.key_eth_dst, ETH_ALEN);
	memcpy(nps_classifier_key->src_mac.ether_addr_octet, tc_filter_params->flower_rule_policy.key_eth_src, ETH_ALEN);

	write_log(LOG_DEBUG, "nps_tc classifier key raw data 0x%08x%08x%08x%08x%08x%08x%08x%08x%08x",
		  bswap_32(nps_classifier_key->raw_data[0]),
		  bswap_32(nps_classifier_key->raw_data[1]),
		  bswap_32(nps_classifier_key->raw_data[2]),
		  bswap_32(nps_classifier_key->raw_data[3]),
		  bswap_32(nps_classifier_key->raw_data[4]),
		  bswap_32(nps_classifier_key->raw_data[5]),
		  bswap_32(nps_classifier_key->raw_data[6]),
		  bswap_32(nps_classifier_key->raw_data[7]),
		  bswap_32(nps_classifier_key->raw_data[8]));

}

void build_nps_tc_classifier_result(uint32_t highest_priority_value,
				    uint32_t highest_priority_action_index,
				    uint32_t rules_list,
				    struct tc_classifier_result *nps_classifier_result)
{
	nps_classifier_result->priority = bswap_32(highest_priority_value);
	nps_classifier_result->filter_actions_index = bswap_32(highest_priority_action_index);
	nps_classifier_result->rules_list_index = bswap_32(rules_list);
	if (rules_list != NOT_VALID_RULE_INDEX) {
		nps_classifier_result->is_rules_list_valid = 1;
	}
}

void build_nps_rules_list_key(struct rules_list_item *rules_list, struct tc_rules_list_key *rules_list_key)
{
	memset(rules_list_key, 0, sizeof(struct tc_rules_list_key));
	rules_list_key->rule_index = bswap_32(rules_list->rules_list_index);
}

void build_nps_rules_list_result(struct rules_list_item *rules_list,
				 uint32_t next_rule_item,
				 struct tc_rules_list_result *rules_list_result)
{
	memset(rules_list_result, 0, sizeof(struct tc_rules_list_result));
	rules_list_result->filter_actions_index = bswap_32(rules_list->filter_actions_index);
	rules_list_result->priority = bswap_32(rules_list->priority);
	rules_list_result->next_rule_index = bswap_32(next_rule_item);
	if (next_rule_item == NOT_VALID_RULE_INDEX) {
		rules_list_result->is_next_rule_valid = false;
	} else {
		rules_list_result->is_next_rule_valid = true;
	}
}

void build_nps_tc_masks_key(struct tc_filter *tc_filter_params,
			   uint32_t new_mask_index,
			   struct tc_mask_key *nps_masks_key)
{
	nps_masks_key->interface = tc_filter_params->ifindex; /* todo need to swap? */
	nps_masks_key->mask_index = new_mask_index;

	write_log(LOG_DEBUG, "New mask key for new entry is 0x%08x", *(uint8_t *)nps_masks_key);
}

void build_nps_tc_masks_result(struct tc_mask_info *tc_mask_info,
			       struct tc_mask_result *nps_masks_result)
{
	memcpy(&nps_masks_result->tc_mask_info, tc_mask_info, sizeof(struct tc_mask_info));

	write_log(LOG_DEBUG, "result for the new mask entry is is 0x%04x", *(uint32_t *)&nps_masks_result->tc_mask_info);
}

void build_nps_tc_filter_actions_result(struct tc_filter_actions_result *nps_filter_actions_result,
					uint32_t num_of_actions,
					struct action_info *action_info)
{
	uint32_t i;

	/* initialize all values to not-valid action index */
	nps_filter_actions_result->next_filter_actions_index = NOT_VALID_FILTER_ACTIONS_INDEX;
	for (i = 0; i < MAX_NUM_OF_ACTIONS_IN_FILTER; i++) {
		nps_filter_actions_result->action_index_array[i] = bswap_32(NOT_VALID_ACTION_INDEX);
	}

	/* set the actions index on the array */
	for (i = 0; i < num_of_actions && i < MAX_NUM_OF_ACTIONS_IN_FILTER; i++) {
		nps_filter_actions_result->action_index_array[i] = bswap_32(action_info[i].nps_index);
		write_log(LOG_DEBUG, "The result for the new filter actions entry is (action number %d) 0x%08x",
			  i, nps_filter_actions_result->action_index_array[i]);
	}
}

void build_nps_tc_filter_actions_key(struct tc_filter_actions_key *nps_filter_actions_key,
				     uint32_t filter_actions_index)
{
	nps_filter_actions_key->filter_actions_index = filter_actions_index;

	write_log(LOG_DEBUG, "The key for the new filter actions entry is 0x%08x", nps_filter_actions_key->filter_actions_index);
}


