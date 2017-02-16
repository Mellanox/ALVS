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
*  File:                tc_cp_init.c
*  Desc:                Implementation of TC initialization API.
*
*/

#include "tc_init.h"

#include <EZapiStat.h>

#include "log.h"
#include "infrastructure_utils.h"
#include "infrastructure.h"
#include "application_search_defs.h"
#include "tc_conf.h"

/**************************************************************************//**
 * \brief       Initialize all statistics counter to be zero
 *
 * \return      bool - success or failure
 */
bool tc_initialize_statistics(void)
{
	EZstatus ret_val;
	EZapiStat_DoubleCounterConfig double_counter_config;

	/* Set on demand statistics values to be 0 */
	memset(&double_counter_config, 0, sizeof(double_counter_config));
	double_counter_config.pasCounters = (EZapiStat_DoubleCounter *)malloc(sizeof(EZapiStat_DoubleCounter));
	if (double_counter_config.pasCounters == NULL) {
		write_log(LOG_CRIT, "infra_initialize_statistics: double_counter_config malloc failed.");
		return false;
	}
	memset(double_counter_config.pasCounters, 0, sizeof(EZapiStat_DoubleCounter));

	double_counter_config.uiPartition = 0;
	double_counter_config.bRange = TRUE;
	double_counter_config.uiStartCounter = TC_ACTION_STATS_ON_DEMAND_OFFSET;
	double_counter_config.uiNumCounters = TC_TOTAL_ON_DEMAND_STATS;
	double_counter_config.uiRangeStep = 1;

	double_counter_config.pasCounters[0].uiByteValue = 0;
	double_counter_config.pasCounters[0].uiByteValueMSB = 0;
	double_counter_config.pasCounters[0].uiFrameValue = 0;
	double_counter_config.pasCounters[0].uiFrameValueMSB = 0;
	double_counter_config.pasCounters[0].bEnableThresholdMsg = TRUE;
	double_counter_config.pasCounters[0].uiThresholdByte = 50;
	double_counter_config.pasCounters[0].uiThresholdFrame = 44;

	ret_val = EZapiStat_Config(0, EZapiStat_ConfigCmd_SetDoubleCounters, &double_counter_config);
	free(double_counter_config.pasCounters);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Config: EZapiStat_ConfigCmd_SetDoubleCounters failed.");
		return false;
	}

	return true;
}

/******************************************************************************
 * \brief    Constructor function for all TC data bases.
 *           This function is called not from the network thread but from the
 *           main thread on NPS configuration bringup.
 *
 * \return   bool
 */
bool tc_db_constructor(void)
{
	struct infra_hash_params hash_params;
	struct infra_table_params table_params;
	bool retcode;

	memset(&hash_params, 0, sizeof(struct infra_hash_params));

	write_log(LOG_DEBUG, "Creating tc classification table.");
	hash_params.key_size = sizeof(union tc_classifier_key);
	hash_params.result_size = sizeof(struct tc_classifier_result);
	hash_params.max_num_of_entries = TC_CLASSIFICATION_TABLE_SIZE;
	hash_params.hash_size = 0;
	hash_params.updated_from_dp = false;
	hash_params.is_external = true;
	hash_params.single_cycle = true;
	hash_params.main_table_search_mem_heap = TC_EMEM_SEARCH_0_HEAP;
	hash_params.sig_table_search_mem_heap = TC_EMEM_SEARCH_0_HEAP;
	hash_params.res_table_search_mem_heap = TC_EMEM_SEARCH_0_HEAP;
	retcode = infra_create_hash(STRUCT_ID_TC_CLASSIFIER, &hash_params);
	if (retcode == false) {
		write_log(LOG_CRIT, "Failed to create tc classification hash");
		return false;
	}

	write_log(LOG_DEBUG, "Creating masks table.");
	memset(&table_params, 0, sizeof(struct infra_table_params));
	table_params.key_size = sizeof(struct tc_mask_key);
	table_params.result_size = sizeof(struct tc_mask_result);
	table_params.max_num_of_entries = TC_MASKS_TABLE_SIZE;
	table_params.updated_from_dp = false;
	table_params.is_external = false;
	table_params.search_mem_heap = INFRA_1_CLUSTER_SEARCH_HEAP;
	retcode = infra_create_table(STRUCT_ID_TC_MASK_BITMAP, &table_params);
	if (retcode == false) {
		write_log(LOG_CRIT, "Failed to create tc masks table.");
		return false;
	}

	write_log(LOG_DEBUG, "Creating filter rules table.");
	memset(&table_params, 0, sizeof(struct infra_table_params));
	table_params.key_size = sizeof(struct tc_rules_list_key);
	table_params.result_size = sizeof(struct tc_rules_list_result);
	table_params.max_num_of_entries = TC_FILTER_RULES_TABLE_SIZE;
	table_params.updated_from_dp = false;
	table_params.is_external = true;
	table_params.search_mem_heap = TC_EMEM_SEARCH_0_HEAP;
	retcode = infra_create_table(STRUCT_ID_TC_RULES_LIST, &table_params);
	if (retcode == false) {
		write_log(LOG_CRIT, "Failed to create tc filter rules table.");
		return false;
	}

	write_log(LOG_DEBUG, "Creating actions table.");
	memset(&table_params, 0, sizeof(struct infra_table_params));
	table_params.key_size = sizeof(struct tc_action_key);
	table_params.result_size = sizeof(struct tc_action_result);
	table_params.max_num_of_entries = TC_ACTIONS_TABLE_SIZE;
	table_params.updated_from_dp = false;
	table_params.is_external = true;
	table_params.search_mem_heap = TC_EMEM_SEARCH_0_HEAP;
	retcode = infra_create_table(STRUCT_ID_TC_ACTION_INFO, &table_params);
	if (retcode == false) {
		write_log(LOG_CRIT, "Failed to create tc actions table.");
		return false;
	}

	write_log(LOG_DEBUG, "Creating actions extra info table.");
	memset(&table_params, 0, sizeof(struct infra_table_params));
	table_params.key_size = sizeof(struct tc_action_key);
	table_params.result_size = sizeof(struct tc_pedit_action_info_result);
	table_params.max_num_of_entries = TC_ACTIONS_EXTRA_INFO_TABLE_SIZE;
	table_params.updated_from_dp = false;
	table_params.is_external = true;
	table_params.search_mem_heap = TC_EMEM_SEARCH_0_HEAP;
	retcode = infra_create_table(STRUCT_ID_TC_ACTION_EXTRA_INFO, &table_params);
	if (retcode == false) {
		write_log(LOG_CRIT, "Failed to create tc actions table.");
		return false;
	}

	write_log(LOG_DEBUG, "Creating filter actions table.");
	memset(&table_params, 0, sizeof(struct infra_table_params));
	table_params.key_size = sizeof(struct tc_filter_actions_key);
	table_params.result_size = sizeof(struct tc_filter_actions_result);
	table_params.max_num_of_entries = TC_FILTER_ACTIONS_TABLE_SIZE;
	table_params.updated_from_dp = false;
	table_params.is_external = true;
	table_params.search_mem_heap = TC_EMEM_SEARCH_0_HEAP;
	retcode = infra_create_table(STRUCT_ID_TC_FILTER_ACTIONS, &table_params);
	if (retcode == false) {
		write_log(LOG_CRIT, "Failed to create tc filter actions table");
		return false;
	}

	write_log(LOG_DEBUG, "Creating action timestamp table.");
	memset(&table_params, 0, sizeof(struct infra_table_params));
	table_params.key_size = sizeof(struct tc_timestamp_key);
	table_params.result_size = sizeof(struct tc_timestamp_result);
	table_params.max_num_of_entries = TC_ACTION_TIMESTAMP_TABLE_SIZE;
	table_params.updated_from_dp = true;
	table_params.is_external = true;
	table_params.search_mem_heap = TC_EMEM_SEARCH_1_HEAP;
	retcode = infra_create_table(STRUCT_ID_TC_TIMESTAMPS, &table_params);
	if (retcode == false) {
		write_log(LOG_CRIT, "Failed to create tc action timestamp table");
		return false;
	}



	return true;
}
