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
*  File:                alvs_init.c
*  Desc:                Implementation of ALVS initialization API.
*
*/

#include "alvs_init.h"

#include <EZapiStat.h>
#include <EZapiChannel.h>

#include "log.h"
#include "infrastructure_utils.h"
#include "infrastructure.h"
#include "application_search_defs.h"
#include "alvs_conf.h"



bool alvs_create_index_pools(void)
{
	EZstatus ret_val;
	EZapiChannel_IndexPoolParams index_pool_params;

	/* configure 2 pools for search */
	memset(&index_pool_params, 0, sizeof(index_pool_params));
	index_pool_params.uiPool = ALVS_CONN_HASH_SIG_PAGE_POOL_ID;

	ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetIndexPoolParams, &index_pool_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiChannel_Status: EZapiChannel_StatCmd_GetIndexPoolParams failed.");
		return false;
	}

	index_pool_params.bEnable = true;
	index_pool_params.bSearch = true;

	ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetIndexPoolParams, &index_pool_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiChannel_Config: EZapiChannel_ConfigCmd_SetIndexPoolParams failed.");
		return false;
	}


	memset(&index_pool_params, 0, sizeof(index_pool_params));
	index_pool_params.uiPool = ALVS_CONN_HASH_RESULT_POOL_ID;

	ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetIndexPoolParams, &index_pool_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiChannel_Status: EZapiChannel_StatCmd_GetIndexPoolParams failed.");
		return false;
	}

	index_pool_params.bEnable = true;
	index_pool_params.bSearch = true;

	ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetIndexPoolParams, &index_pool_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiChannel_Config: EZapiChannel_ConfigCmd_SetIndexPoolParams failed.");
		return false;
	}

	memset(&index_pool_params, 0, sizeof(index_pool_params));
	index_pool_params.uiPool = ALVS_CONN_TABLE_POOL_ID;

	ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetIndexPoolParams, &index_pool_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiChannel_Status: EZapiChannel_StatCmd_GetIndexPoolParams failed.");
		return false;
	}

	index_pool_params.bEnable = true;
	index_pool_params.uiNumIndexes = ALVS_CONN_MAX_ENTRIES;

	ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetIndexPoolParams, &index_pool_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiChannel_Status: EZapiChannel_ConfigCmd_SetIndexPoolParams failed.");
		return false;
	}

	return true;
}


bool alvs_create_timers(void)
{
	EZstatus ret_val;
	EZapiChannel_PMUTimerParams pmu_timer_params;

	pmu_timer_params.uiSide = 0;
	pmu_timer_params.uiTimer = 0;
	ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetPMUTimerParams, &pmu_timer_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiChannel_Status: EZapiChannel_StatCmd_GetPMUTimerParams failed.");
		return false;
	}

	pmu_timer_params.bEnable = true;
	pmu_timer_params.uiLogicalID = ALVS_TIMER_LOGICAL_ID;
	pmu_timer_params.uiPMUQueue = 0;   /* TODO - need a dedicated queue for timers */
	pmu_timer_params.uiNumJobs = 30*1024*1024;    /* TODO - define */
	pmu_timer_params.uiNanoSecPeriod = 0;
	pmu_timer_params.uiSecPeriod = 960;  /* TODO - define */

	ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetPMUTimerParams, &pmu_timer_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiChannel_Config: EZapiChannel_ConfigCmd_SetPMUTimerParams failed.");
		return false;
	}

	return true;
}


bool alvs_initialize_statistics(void)
{
	EZstatus ret_val;
	EZapiStat_PostedCounterConfig posted_counter_config;
	EZapiStat_LongCounterConfig long_counter_config;

	/* Set on demand statistics values to be 0 */
	memset(&long_counter_config, 0, sizeof(long_counter_config));
	long_counter_config.pasCounters = malloc(sizeof(EZapiStat_LongCounter));
	if (long_counter_config.pasCounters == NULL) {
		write_log(LOG_CRIT, "infra_initialize_statistics: long_counter_config malloc failed.");
		return false;
	}
	memset(long_counter_config.pasCounters, 0, sizeof(EZapiStat_LongCounter));

	long_counter_config.uiPartition = 0;
	long_counter_config.bRange = TRUE;
	long_counter_config.uiStartCounter = ALVS_SERVER_STATS_ON_DEMAND_OFFSET;
	long_counter_config.uiNumCounters = ALVS_SERVERS_MAX_ENTRIES * ALVS_NUM_OF_SERVER_ON_DEMAND_STATS;
	long_counter_config.uiRangeStep = 1;
	long_counter_config.pasCounters[0].uiValue = 0;
	long_counter_config.pasCounters[0].uiValueMSB = 0;
	long_counter_config.pasCounters[0].bEnableThresholdMsg = FALSE;
	long_counter_config.pasCounters[0].uiThreshold = 58;

	ret_val = EZapiStat_Config(0, EZapiStat_ConfigCmd_SetLongCounters, &long_counter_config);
	free(long_counter_config.pasCounters);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Config: EZapiStat_ConfigCmd_SetLongCounters failed.");
		return false;
	}

	/* Set posted statistics values to be 0 */
	memset(&posted_counter_config, 0, sizeof(posted_counter_config));
	posted_counter_config.pasCounters = malloc(sizeof(EZapiStat_PostedCounter));
	if (posted_counter_config.pasCounters == NULL) {
		write_log(LOG_CRIT, "infra_initialize_statistics: EZapiStat_PostedCounter malloc failed.");
		return false;
	}
	memset(posted_counter_config.pasCounters, 0, sizeof(EZapiStat_PostedCounter));
	posted_counter_config.uiPartition = 0;
	posted_counter_config.bRange = true;
	posted_counter_config.uiStartCounter = ALVS_ERROR_STATS_POSTED_OFFSET;
	posted_counter_config.uiNumCounters = ALVS_TOTAL_POSTED_STATS;
	posted_counter_config.pasCounters[0].uiByteValue = 0;
	posted_counter_config.pasCounters[0].uiByteValueMSB = 0;
	posted_counter_config.pasCounters[0].uiFrameValue = 0;
	posted_counter_config.pasCounters[0].uiFrameValueMSB = 0;

	ret_val = EZapiStat_Config(0, EZapiStat_ConfigCmd_SetPostedCounters, &posted_counter_config);
	free(posted_counter_config.pasCounters);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Config: EZapiStat_ConfigCmd_SetPostedCounters failed.");
		return false;
	}

	return true;
}

bool alvs_db_constructor(void)
{
	struct infra_hash_params hash_params;
	struct infra_table_params table_params;
	bool retcode;

	write_log(LOG_DEBUG, "Creating service classification table.");
	hash_params.key_size = sizeof(struct alvs_service_classification_key);
	hash_params.result_size = sizeof(struct alvs_service_classification_result);
	hash_params.max_num_of_entries = ALVS_SERVICES_MAX_ENTRIES;
	hash_params.hash_size = 0;
	hash_params.updated_from_dp = false;
	hash_params.is_external = true;
	hash_params.main_table_search_mem_heap = ALVS_EMEM_SEARCH_0_HEAP;
	hash_params.sig_table_search_mem_heap = ALVS_EMEM_SEARCH_0_HEAP;
	hash_params.res_table_search_mem_heap = ALVS_EMEM_SEARCH_1_HEAP;
	retcode = infra_create_hash(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION, &hash_params);
	if (retcode == false) {
		write_log(LOG_CRIT, "Failed to create alvs service classification hash.");
		return false;
	}

	write_log(LOG_DEBUG, "Creating service info table.");
	table_params.key_size = sizeof(struct alvs_service_info_key);
	table_params.result_size = sizeof(struct alvs_service_info_result);
	table_params.max_num_of_entries = ALVS_SERVICES_MAX_ENTRIES;
	table_params.updated_from_dp = false;
	table_params.is_external = false;
	table_params.search_mem_heap = INFRA_4_CLUSTER_SEARCH_HEAP;
	retcode = infra_create_table(STRUCT_ID_ALVS_SERVICE_INFO, &table_params);
	if (retcode == false) {
		write_log(LOG_CRIT, "Failed to create alvs service info table.");
		return false;
	}

	write_log(LOG_DEBUG, "Creating scheduling info table.");
	table_params.key_size = sizeof(struct alvs_sched_info_key);
	table_params.result_size = sizeof(struct alvs_sched_info_result);
	table_params.max_num_of_entries = ALVS_SCHED_MAX_ENTRIES;
	table_params.updated_from_dp = false;
	table_params.is_external = true;
	table_params.search_mem_heap = ALVS_EMEM_SEARCH_2_HEAP;
	retcode = infra_create_table(STRUCT_ID_ALVS_SCHED_INFO, &table_params);
	if (retcode == false) {
		write_log(LOG_CRIT, "Failed to create alvs scheduling info table.");
		return false;
	}

	write_log(LOG_DEBUG, "Creating server info table.");
	table_params.key_size = sizeof(struct alvs_server_info_key);
	table_params.result_size = sizeof(struct alvs_server_info_result);
	table_params.max_num_of_entries = ALVS_SERVERS_MAX_ENTRIES;
	table_params.updated_from_dp = false;
	table_params.is_external = true;
	table_params.search_mem_heap = ALVS_EMEM_SEARCH_2_HEAP;
	retcode = infra_create_table(STRUCT_ID_ALVS_SERVER_INFO, &table_params);
	if (retcode == false) {
		write_log(LOG_CRIT, "Failed to create alvs server info table.");
		return false;
	}

	write_log(LOG_DEBUG, "Creating connection classification table.");
	hash_params.key_size = sizeof(struct alvs_conn_classification_key);
	hash_params.result_size = sizeof(struct alvs_conn_classification_result);
	hash_params.max_num_of_entries = ALVS_CONN_MAX_ENTRIES;
	hash_params.hash_size = 26;
	hash_params.updated_from_dp = true;
	hash_params.sig_pool_id = ALVS_CONN_HASH_SIG_PAGE_POOL_ID;
	hash_params.result_pool_id = ALVS_CONN_HASH_RESULT_POOL_ID;
	hash_params.is_external = true;
	hash_params.main_table_search_mem_heap = ALVS_EMEM_SEARCH_0_HEAP;
	hash_params.sig_table_search_mem_heap = ALVS_EMEM_SEARCH_0_HEAP;
	hash_params.res_table_search_mem_heap = ALVS_EMEM_SEARCH_1_HEAP;
	retcode = infra_create_hash(STRUCT_ID_ALVS_CONN_CLASSIFICATION, &hash_params);
	if (retcode == false) {
		write_log(LOG_CRIT, "Failed to create alvs conn classification hash.");
		return false;
	}

	write_log(LOG_DEBUG, "Creating connection info table.");
	table_params.key_size = sizeof(struct alvs_conn_info_key);
	table_params.result_size = sizeof(struct alvs_conn_info_result);
	table_params.max_num_of_entries = ALVS_CONN_MAX_ENTRIES;
	table_params.updated_from_dp = true;
	table_params.is_external = true;
	table_params.search_mem_heap = ALVS_EMEM_SEARCH_1_HEAP;
	retcode = infra_create_table(STRUCT_ID_ALVS_CONN_INFO, &table_params);
	if (retcode == false) {
		write_log(LOG_CRIT, "Failed to create alvs conn info table.");
		return false;
	}

	write_log(LOG_DEBUG, "Creating server classification table.");
	hash_params.key_size = sizeof(struct alvs_server_classification_key);
	hash_params.result_size = sizeof(struct alvs_server_classification_result);
	hash_params.max_num_of_entries = ALVS_SERVERS_MAX_ENTRIES;
	hash_params.hash_size = 0;
	hash_params.updated_from_dp = false;
	hash_params.is_external = true;
	hash_params.main_table_search_mem_heap = ALVS_EMEM_SEARCH_0_HEAP;
	hash_params.sig_table_search_mem_heap = ALVS_EMEM_SEARCH_0_HEAP;
	hash_params.res_table_search_mem_heap = ALVS_EMEM_SEARCH_1_HEAP;
	retcode = infra_create_hash(STRUCT_ID_ALVS_SERVER_CLASSIFICATION, &hash_params);
	if (retcode == false) {
		write_log(LOG_CRIT, "Failed to create alvs server classification hash.");
		return false;
	}

	write_log(LOG_DEBUG, "Creating application info table.");
	table_params.key_size = sizeof(struct application_info_key);
	table_params.result_size = sizeof(union application_info_result);
	table_params.max_num_of_entries = APPLICATION_INFO_MAX_ENTRIES;
	table_params.updated_from_dp = false;
	table_params.is_external = false;
	table_params.search_mem_heap = INFRA_1_CLUSTER_SEARCH_HEAP;
	retcode = infra_create_table(STRUCT_ID_APPLICATION_INFO, &table_params);
	if (retcode == false) {
		write_log(LOG_CRIT, "Failed to create application info table.");
		return false;
	}

	return true;
}
