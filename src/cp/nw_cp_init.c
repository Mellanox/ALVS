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
*  File:                nw_cp_init.c
*  Desc:                Implementation of NW initializations API.
*
*/

#include "nw_cp_init.h"

#include <EZapiChannel.h>
#include <EZapiStat.h>

#include "log.h"
#include "infrastructure.h"
#include "infrastructure_utils.h"
#include "nw_search_defs.h"
#include "nw_conf.h"


bool nw_initialize_protocol_decode(void)
{
	EZstatus ret_val;
	struct ether_addr my_mac;
	EZapiChannel_ProtocolDecoderParams protocol_decoder_params;

	/* Get defaults for profile 0 - this is the default profile used
	 * by application 0.
	 */
	memset(&protocol_decoder_params, 0, sizeof(protocol_decoder_params));
	protocol_decoder_params.uiProfile = 0;
	ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetProtocolDecoderParams, &protocol_decoder_params);
	if (EZrc_IS_ERROR(ret_val)) {
		return false;
	}

	/* Get my MAC */
	if (infra_get_my_mac(&my_mac) == false) {
		return false;
	}

	/* Set my MAC in protocol decode profile */
	protocol_decoder_params.aucDestMACAddressHigh[0] = my_mac.ether_addr_octet[0];
	protocol_decoder_params.aucDestMACAddressHigh[1] = my_mac.ether_addr_octet[1];
	protocol_decoder_params.aucDestMACAddressHigh[2] = my_mac.ether_addr_octet[2];
	protocol_decoder_params.aucDestMACAddressHigh[3] = my_mac.ether_addr_octet[3];
	protocol_decoder_params.aucDestMACAddressHigh[4] = my_mac.ether_addr_octet[4];
	protocol_decoder_params.aucDestMACAddressHigh[5] = my_mac.ether_addr_octet[5];
	protocol_decoder_params.aucDestMACAddressLow[0] = my_mac.ether_addr_octet[0];
	protocol_decoder_params.aucDestMACAddressLow[1] = my_mac.ether_addr_octet[1];
	protocol_decoder_params.aucDestMACAddressLow[2] = my_mac.ether_addr_octet[2];
	protocol_decoder_params.aucDestMACAddressLow[3] = my_mac.ether_addr_octet[3];
	protocol_decoder_params.aucDestMACAddressLow[4] = my_mac.ether_addr_octet[4];
	protocol_decoder_params.aucDestMACAddressLow[5] = my_mac.ether_addr_octet[5];

	ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetProtocolDecoderParams, &protocol_decoder_params);
	if (EZrc_IS_ERROR(ret_val)) {
		return false;
	}

	return true;
}

/* Token bucket for SYSlog */
#define SYSLOG_TB_PROFILE_0_CIR_RESOLUTION EZapiStat_TBProfileResolution_1_BYTE
#define SYSLOG_TB_PROFILE_0_CIR            0x40000000
#define SYSLOG_TB_PROFILE_0_CBS            0x40000000

bool nw_initialize_statistics(void)
{
	EZstatus ret_val;
	EZapiStat_LongCounterConfig long_counter_config;
	EZapiStat_TBProfile token_bucket_profile;
	EZapiStat_TBCounterConfig token_bucket_counter_config;

	memset(&token_bucket_profile, 0, sizeof(token_bucket_profile));

	token_bucket_profile.uiPartition = 0;
	token_bucket_profile.uiProfile = 0;

	ret_val = EZapiStat_Status(0, EZapiStat_StatCmd_GetTokenBucketProfile, &token_bucket_profile);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Config: EZapiStat_StatCmd_GetTokenBucketProfile failed.");
		return false;
	}

	token_bucket_profile.uiPartition = 0;
	token_bucket_profile.uiProfile = 0;
	token_bucket_profile.uiCIR = SYSLOG_TB_PROFILE_0_CIR;
	token_bucket_profile.eCIRResolution = SYSLOG_TB_PROFILE_0_CIR_RESOLUTION;
	token_bucket_profile.uiCBS = SYSLOG_TB_PROFILE_0_CBS;

	ret_val = EZapiStat_Config(0, EZapiStat_ConfigCmd_SetTokenBucketProfile, &token_bucket_profile);

	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Config: EZapiStat_ConfigCmd_SetTokenBucketProfile failed.");
		return false;
	}

	memset(&token_bucket_counter_config, 0, sizeof(token_bucket_counter_config));

	token_bucket_counter_config.pasCounters = malloc(sizeof(EZapiStat_TBCounter) * 1);
	if (token_bucket_counter_config.pasCounters == NULL) {
		write_log(LOG_CRIT, "infra_initialize_statistics: token_bucket_counter_config malloc failed.");
		return false;
	}
	memset(token_bucket_counter_config.pasCounters, 0, sizeof(EZapiStat_TBCounter) * 1);
	token_bucket_counter_config.uiPartition = 0;
	token_bucket_counter_config.bRange = TRUE;
	token_bucket_counter_config.uiStartCounter = SYSLOG_TB_STATS_ON_DEMAND_OFFSET;
	token_bucket_counter_config.uiNumCounters = SYSLOG_NUM_OF_TB_STATS;
	token_bucket_counter_config.pasCounters[0].uiCommitProfile = 0;
	token_bucket_counter_config.pasCounters[0].eAlgorithm = EZapiStat_TBAlgorithm_SINGLE_BUCKET;

	ret_val = EZapiStat_Config(0, EZapiStat_ConfigCmd_SetTokenBucketCounters, &token_bucket_counter_config);
	free(token_bucket_counter_config.pasCounters);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Config: EZapiStat_ConfigCmd_SetTokenBucketProfile failed.");
		return false;
	}

	/* Set long statistics values to be 0 */
	memset(&long_counter_config, 0, sizeof(long_counter_config));
	long_counter_config.pasCounters = malloc(sizeof(EZapiStat_LongCounter));
	if (long_counter_config.pasCounters == NULL) {
		write_log(LOG_CRIT, "infra_initialize_statistics: long_counter_config malloc failed.");
		return false;
	}
	memset(long_counter_config.pasCounters, 0, sizeof(EZapiStat_LongCounter));

	long_counter_config.uiPartition = 0;
	long_counter_config.bRange = TRUE;
	long_counter_config.uiStartCounter = SYSLOG_COLOR_FLAG_STATS_ON_DEMAND_OFFSET;
	long_counter_config.uiNumCounters = SYSLOG_NUM_OF_COLOR_FLAG_STATS;
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

	return true;
}

/******************************************************************************
 * \brief    Constructor function for all network data bases.
 *           this function is called not from the network thread but from the
 *           main thread on NPS configuration bringup.
 *
 * \return   void
 */
bool nw_db_constructor(void)
{
	struct infra_hash_params hash_params;
	struct infra_table_params table_params;
	struct infra_tcam_params tcam_params;

	write_log(LOG_DEBUG, "Creating ARP table.");
	hash_params.key_size = sizeof(struct nw_arp_key);
	hash_params.result_size = sizeof(struct nw_arp_result);
	hash_params.max_num_of_entries = 65536;  /* TODO - define? */
	hash_params.hash_size = 0;
	hash_params.updated_from_dp = false;
	hash_params.is_external = true;
	hash_params.main_table_search_mem_heap = NW_EMEM_SEARCH_0_HEAP;
	hash_params.sig_table_search_mem_heap = NW_EMEM_SEARCH_0_HEAP;
	hash_params.res_table_search_mem_heap = NW_EMEM_SEARCH_0_HEAP;
	if (infra_create_hash(STRUCT_ID_NW_ARP,
			      &hash_params) == false) {
		write_log(LOG_CRIT, "Error - Failed creating ARP table.");
		return false;
	}

	write_log(LOG_DEBUG, "Creating FIB table.");
	tcam_params.key_size = sizeof(struct nw_fib_key);
	tcam_params.max_num_of_entries = NW_FIB_TCAM_MAX_SIZE;
	tcam_params.profile = NW_FIB_TCAM_PROFILE;
	tcam_params.result_size = sizeof(struct nw_fib_result);
	tcam_params.side = NW_FIB_TCAM_SIDE;
	tcam_params.lookup_table_count = NW_FIB_TCAM_LOOKUP_TABLE_COUNT;
	tcam_params.table = NW_FIB_TCAM_TABLE;
	if (infra_create_tcam(&tcam_params) == false) {
		write_log(LOG_CRIT, "Error - Failed creating FIB TCAM.");
		return false;
	}

	write_log(LOG_DEBUG, "Creating interface table.");
	table_params.key_size = sizeof(struct nw_if_key);
	table_params.result_size = sizeof(struct nw_if_result);
	table_params.max_num_of_entries = 256;  /* TODO - define? */
	table_params.updated_from_dp = false;
	hash_params.is_external = false;
	table_params.search_mem_heap = INFRA_1_CLUSTER_SEARCH_HEAP;
	if (infra_create_table(STRUCT_ID_NW_INTERFACES,
			       &table_params) == false) {
		write_log(LOG_CRIT, "Error - Failed creating interface table.");
		return false;
	}

	write_log(LOG_DEBUG, "Creating Lag Group table.");
	table_params.key_size = sizeof(struct nw_lag_group_key);
	table_params.result_size = sizeof(struct nw_lag_group_result);
	table_params.max_num_of_entries = NW_LAG_GROUPS_TABLE_MAX_ENTRIES;
	table_params.updated_from_dp = false;
	table_params.search_mem_heap = INFRA_1_CLUSTER_SEARCH_HEAP;
	if (infra_create_table(STRUCT_ID_NW_LAG_GROUPS,
			       &table_params) == false) {
		write_log(LOG_CRIT, "Error - Failed creating lag group table.");
		return false;
	}

	return true;
}
