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
*  File:                infrastructure.c
*  Desc:                Implementation of infrastructure API.
*
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <EZapiChannel.h>
#include <EZapiStat.h>
#include <EZapiStruct.h>
#include <EZagtCPMain.h>
#include <EZosTask.h>
#include <EZagtRPC.h>

#include "log.h"
#include "infrastructure.h"
#include "alvs_db_manager.h"
#include "nw_db_manager.h"
#include "defs.h"
#include "user_defs.h"

EZagtRPCServer host_server;

/* AGT port */
#define INFRA_AGT_PORT              1234

/* Interfaces */
#define	INFRA_NW_IF_TYPE            EZapiChannel_EthIFType_10GE

/* Host interface parameters */
#define INFRA_HOST_IF_SIDE          1
#define INFRA_HOST_IF_PORT          24

/* Network interface #0 parameters */
#define INFRA_NW_IF_0_SIDE          0
#define INFRA_NW_IF_0_PORT          0

/* Network interface #1 parameters */
#define INFRA_NW_IF_1_SIDE          0
#define INFRA_NW_IF_1_PORT          12

/* Network interface #2 parameters */
#define INFRA_NW_IF_2_SIDE          1
#define INFRA_NW_IF_2_PORT          0

/* Network interface #3 parameters */
#define INFRA_NW_IF_3_SIDE          1
#define INFRA_NW_IF_3_PORT          12

/*! interface configuration parameters possible values. */
enum infra_interface_params {
	INFRA_INTERFACE_PARAMS_SIDE             = 0,
	INFRA_INTERFACE_PARAMS_PORT             = 1,
	INFRA_NUM_OF_INTERFACE_PARAMS
};

/*! IMEM spaces configuration parameters possible values. */
enum infra_imem_spaces_params {
	INFRA_IMEM_SPACES_PARAMS_TYPE               = 0,
	INFRA_IMEM_SPACES_PARAMS_SIZE               = 1,
	INFRA_IMEM_SPACES_PARAMS_INDEX              = 2,
	INFRA_IMEM_SPACES_PARAMS_HEAP               = 3,
	INFRA_NUM_OF_IMEM_SPACES_PARAMS
};

/*! EMEM spaces configuration parameters possible values. */
enum infra_emem_spaces_params {
	INFRA_EMEM_SPACES_PARAMS_TYPE               = 0,
	INFRA_EMEM_SPACES_PARAMS_PROTECTION         = 1,
	INFRA_EMEM_SPACES_PARAMS_SIZE               = 2,
	INFRA_EMEM_SPACES_PARAMS_MSID               = 3,
	INFRA_EMEM_SPACES_PARAMS_INDEX              = 4,
	INFRA_EMEM_SPACES_PARAMS_HEAP               = 5,
	INFRA_NUM_OF_EMEM_SPACES_PARAMS
};

/* Memory spaces */

#define INFRA_X1_CLUSTER_CODE_SIZE          64
#define INFRA_ALL_CLUSTER_CODE_SIZE         512
#define INFRA_X1_CLUSTER_SEARCH_SIZE        4
#define INFRA_X4_CLUSTER_SEARCH_SIZE        516

#define INFRA_EMEM_SEARCH_HASH_SIZE         (4*1024)
#define INFRA_EMEM_SEARCH_TABLE_SIZE        (4*1024)
#define INFRA_EMEM_DATA_OUT_OF_BAND_SIZE    1

#define NUM_OF_INT_MEMORY_SPACES            4
#define NUM_OF_EXT_MEMORY_SPACES            3

/* Search MSIDs */
#define EMEM_SEARCH_HASH_MSID              0x0
#define EMEM_SEARCH_TABLE_MSID             0x1

uint32_t network_interface_params[USER_NW_IF_NUM][INFRA_NUM_OF_INTERFACE_PARAMS] = {
		{INFRA_NW_IF_0_SIDE, INFRA_NW_IF_0_PORT},
		{INFRA_NW_IF_1_SIDE, INFRA_NW_IF_1_PORT},
		{INFRA_NW_IF_2_SIDE, INFRA_NW_IF_2_PORT},
		{INFRA_NW_IF_3_SIDE, INFRA_NW_IF_3_PORT}
};

uint32_t imem_spaces_params[NUM_OF_INT_MEMORY_SPACES][INFRA_NUM_OF_IMEM_SPACES_PARAMS] = {
	{EZapiChannel_IntMemSpaceType_1_CLUSTER_CODE, INFRA_X1_CLUSTER_CODE_SIZE, 0, INFRA_NOT_VALID_HEAP},
	{EZapiChannel_IntMemSpaceType_1_CLUSTER_SEARCH, INFRA_X1_CLUSTER_SEARCH_SIZE, 0, INFRA_X1_CLUSTER_SEARCH_HEAP},
	{EZapiChannel_IntMemSpaceType_4_CLUSTER_SEARCH, INFRA_X4_CLUSTER_SEARCH_SIZE, 0, INFRA_X4_CLUSTER_SEARCH_HEAP},
	{EZapiChannel_IntMemSpaceType_ALL_CLUSTER_CODE, INFRA_ALL_CLUSTER_CODE_SIZE, 0, INFRA_NOT_VALID_HEAP}
};

uint32_t emem_spaces_params[NUM_OF_EXT_MEMORY_SPACES][INFRA_NUM_OF_EMEM_SPACES_PARAMS] = {
	{EZapiChannel_ExtMemSpaceType_SEARCH, 0, INFRA_EMEM_SEARCH_HASH_SIZE, EMEM_SEARCH_HASH_MSID, 0, INFRA_EMEM_SEARCH_HASH_HEAP},
	{EZapiChannel_ExtMemSpaceType_SEARCH, 0, INFRA_EMEM_SEARCH_TABLE_SIZE, EMEM_SEARCH_TABLE_MSID, 0, INFRA_EMEM_SEARCH_TABLE_HEAP},
	{EZapiChannel_ExtMemSpaceType_GENERAL, EZapiChannel_ExtMemSpaceECCType_OUT_OF_BAND, INFRA_EMEM_DATA_OUT_OF_BAND_SIZE, EMEM_SPINLOCK_MSID, 0, INFRA_NOT_VALID_HEAP}
};

/**************************************************************************//**
 * \brief       Create an interface on <side> and <port> with <type> and
 *              <logical_id>
 *
 * \param[in]   side            - side of the interface
 * \param[in]   port            - port of the interface
 * \param[in]   type            - type of the interface
 * \param[in]   logical_id      - logical_id of the interface
 *
 * \return      bool - success or failure
 */
bool infra_create_intetface(uint32_t side, uint32_t port_number, EZapiChannel_EthIFType type, uint32_t logical_id)
{
	EZstatus ret_val;
	EZapiChannel_EthIFParams eth_if_params;
	EZapiChannel_EthRXChannelParams eth_rx_channel_params;

	/* set key values */
	/* port = engine*12 + if_number */
	eth_if_params.uiSide = side;
	eth_if_params.uiIFEngine = port_number / 12;
	eth_if_params.eEthIFType = type;
	eth_if_params.uiIFNumber = port_number % 12;

	/* get default settings of the interface */
	ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetEthIFParams, &eth_if_params);
	if (EZrc_IS_ERROR(ret_val)) {
		return false;
	}

	/* set interface to be enabled and with TM bypass */
	eth_if_params.bRXEnable = true;
	eth_if_params.bTXEnable = true;
	eth_if_params.bTXTMBypass = true;

	ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetEthIFParams, &eth_if_params);
	if (EZrc_IS_ERROR(ret_val)) {
		return false;
	}


	/* set key values */
	/* port = engine*12 + if_number */
	eth_rx_channel_params.uiSide = side;
	eth_rx_channel_params.uiIFEngine  = port_number / 12;
	eth_rx_channel_params.eEthIFType  = type;
	eth_rx_channel_params.uiIFNumber  = port_number % 12;
	eth_rx_channel_params.uiRXChannel  = 0;

	/* get default settings of the interface RX channel */
	ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetEthRXChannelParams, &eth_rx_channel_params);
	if (EZrc_IS_ERROR(ret_val)) {
		return false;
	}

	/* set logical ID */
	eth_rx_channel_params.uiLogicalID = logical_id;

	ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetEthRXChannelParams, &eth_rx_channel_params);
	if (EZrc_IS_ERROR(ret_val)) {
		return false;
	}

	return true;
}

/**************************************************************************//**
 * \brief       Create all interfaces according to network_interface_params
 *
 * \return      bool - success or failure
 */
bool infra_create_if_mapping(void)
{
	uint32_t ind;

	/* Configure external interfaces */
	for (ind = 0; ind < USER_NW_IF_NUM; ind++) {
		if (infra_create_intetface(network_interface_params[ind][INFRA_INTERFACE_PARAMS_SIDE],
				network_interface_params[ind][INFRA_INTERFACE_PARAMS_PORT],
				INFRA_NW_IF_TYPE,
				USER_BASE_LOGICAL_ID + ind) == false) {
			return false;
		}
	}

	/* Configure interface to host */
	if (infra_create_intetface(INFRA_HOST_IF_SIDE,
			INFRA_HOST_IF_PORT,
			EZapiChannel_EthIFType_10GE,
			USER_HOST_LOGICAL_ID) == false) {
		return false;
	}

	return true;
}

/**************************************************************************//**
 * \brief       Allocate the next IMEM index
 *
 * \return      The next available index
 */
uint32_t get_imem_index(void)
{
	static uint32_t index;

	return index++;
}

/**************************************************************************//**
 * \brief       Allocate the next EMEM index
 *
 * \return      The next available index
 */
uint32_t get_emem_index(void)
{
	static uint32_t index;

	return index++;
}

/**************************************************************************//**
 * \brief       Create memory partition,
 *              For IMEM - according to imem_spaces_params
 *              For EMEM - according to emem_spaces_params
 *
 * \return      bool - success or failure
 */
bool infra_create_mem_partition(void)
{
	EZstatus ret_val;
	EZapiChannel_IntMemSpaceParams int_mem_space_params;
	EZapiChannel_ExtMemSpaceParams ext_mem_space_params;
	uint32_t ind;

	/* Configure IMEM memory spaces */
	for (ind = 0; ind < NUM_OF_INT_MEMORY_SPACES; ind++) {
		/* Configure only memory spaces with positive size */
		if (imem_spaces_params[ind][INFRA_IMEM_SPACES_PARAMS_SIZE] > 0) {
			memset(&int_mem_space_params, 0, sizeof(int_mem_space_params));

			/* Allocate next available index in IMEM */
			int_mem_space_params.uiIndex = get_imem_index();

			/* get default settings for the memory space */
			ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetIntMemSpaceParams, &int_mem_space_params);
			if (EZrc_IS_ERROR(ret_val)) {
				return false;
			}

			/* set size and type */
			int_mem_space_params.bEnable = true;
			int_mem_space_params.eType = (EZapiChannel_IntMemSpaceType)imem_spaces_params[ind][INFRA_IMEM_SPACES_PARAMS_TYPE];
			int_mem_space_params.uiSize = imem_spaces_params[ind][INFRA_IMEM_SPACES_PARAMS_SIZE];

			ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetIntMemSpaceParams, &int_mem_space_params);
			if (EZrc_IS_ERROR(ret_val)) {
				return false;
			}

			/* Keep index in imem_spaces_params */
			imem_spaces_params[ind][INFRA_IMEM_SPACES_PARAMS_INDEX] = int_mem_space_params.uiIndex;
			write_log(LOG_DEBUG, "IMEM %d is in index %d\n", ind, int_mem_space_params.uiIndex);
		}
	}

	/* Configure EMEM memory spaces */
	for (ind = 0; ind < NUM_OF_EXT_MEMORY_SPACES; ind++) {
		/* Configure only memory spaces with positive size */
		if (emem_spaces_params[ind][INFRA_EMEM_SPACES_PARAMS_SIZE] > 0) {
			memset(&ext_mem_space_params, 0, sizeof(ext_mem_space_params));

			/* Allocate next available index in IMEM */
			ext_mem_space_params.uiIndex = get_emem_index();

			/* get default settings for the memory space */
			ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetExtMemSpaceParams, &ext_mem_space_params);
			if (EZrc_IS_ERROR(ret_val)) {
				return false;
			}

			/* set size, type, protection and MSID */
			ext_mem_space_params.bEnable = true;
			ext_mem_space_params.eType = (EZapiChannel_ExtMemSpaceType)emem_spaces_params[ind][INFRA_EMEM_SPACES_PARAMS_TYPE];
			ext_mem_space_params.uiSize = emem_spaces_params[ind][INFRA_EMEM_SPACES_PARAMS_SIZE];
			ext_mem_space_params.eECCType = (EZapiChannel_ExtMemSpaceECCType)emem_spaces_params[ind][INFRA_EMEM_SPACES_PARAMS_PROTECTION];
			ext_mem_space_params.uiMSID = emem_spaces_params[ind][INFRA_EMEM_SPACES_PARAMS_MSID];

			ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetExtMemSpaceParams, &ext_mem_space_params);
			if (EZrc_IS_ERROR(ret_val)) {
				return false;
			}

			/* Keep index in imem_spaces_params */
			emem_spaces_params[ind][INFRA_EMEM_SPACES_PARAMS_INDEX] = ext_mem_space_params.uiIndex;
			write_log(LOG_DEBUG, "EMEM %d is in index %d and msid %d.\n", ind, ext_mem_space_params.uiIndex, ext_mem_space_params.uiMSID);
		}
	}

	return true;
}

/**************************************************************************//**
 * \brief       Configure protocol decode profile with my MAC
 *
 * \return      bool - success or failure
 */
bool infra_configure_protocol_decode(void)
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

/**************************************************************************//**
 * \brief       Configure Statistics
 *
 * \return      bool - success or failure
 */
bool infra_create_statistics(void)
{
	EZstatus ret_val;
	EZapiStat_PartitionParams partition_params;
	EZapiStat_GroupParams group_params;
	EZapiStat_PostedPartitionParams posted_partition_params;
	EZapiStat_PostedGroupParams posted_group_params;

	memset(&partition_params, 0, sizeof(partition_params));
	partition_params.uiPartition = 1;
	ret_val = EZapiStat_Status(0, EZapiStat_StatCmd_GetPartitionParams, &partition_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Status: EZapiStat_StatCmd_GetPartitionParams failed.\n");
		return false;
	}

	partition_params.bEnable = true;
	partition_params.uiUnitMask = 0xff;
	partition_params.uiMSID = USER_ON_DEMAND_STATS_MSID;

	ret_val = EZapiStat_Config(0, EZapiStat_ConfigCmd_SetPartitionParams, &partition_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Config: EZapiStat_ConfigCmd_SetPartitionParams failed.\n");
		return false;
	}

	memset(&group_params, 0, sizeof(group_params));
	group_params.uiPartition = 1;
	group_params.uiGroup = 0;
	ret_val = EZapiStat_Status(0, EZapiStat_StatCmd_GetGroupParams, &group_params);

	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Status: EZapiStat_StatCmd_GetGroupParams failed.\n");
		return false;
	}

	group_params.uiStartCounter = 0;
	group_params.uiNumCounters = (36 * 1024 * 1024);
	ret_val = EZapiStat_Config(0, EZapiStat_ConfigCmd_SetGroupParams, &group_params);

	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Config: EZapiStat_ConfigCmd_SetGroupParams failed.\n");
		return false;
	}


	/* Get posted statistics defaults for partition 0 */
	memset(&posted_partition_params, 0, sizeof(posted_partition_params));
	posted_partition_params.uiPartition = 0;
	ret_val = EZapiStat_Status(0, EZapiStat_StatCmd_GetPostedPartitionParams, &posted_partition_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Status: EZapiStat_StatCmd_GetPostedPartitionParams failed.\n");
		return false;
	}

	/* Set MSID */
	posted_partition_params.bEnable = true;
	posted_partition_params.uiMSID = USER_POSTED_STATS_MSID;
	posted_partition_params.uiUnitMask = 0xf;

	ret_val = EZapiStat_Config(0, EZapiStat_ConfigCmd_SetPostedPartitionParams, &posted_partition_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Config: EZapiStat_ConfigCmd_SetPostedPartitionParams failed.\n");
		return false;
	}


	/* Get posted statistics defaults for partition 0 */
	memset(&posted_group_params, 0, sizeof(posted_group_params));
	posted_group_params.uiPartition = 0;
	posted_group_params.eGroupType = EZapiStat_PostedGroupType_OPTIMIZED;
	ret_val = EZapiStat_Status(0, EZapiStat_StatCmd_GetPostedGroupParams, &posted_group_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Status: EZapiStat_StatCmd_GetPostedGroupParams failed.\n");
		return false;
	}

	/* Set group size and optimized group type */
	posted_group_params.uiNumCounters = (7680*1024);

	ret_val = EZapiStat_Config(0, EZapiStat_ConfigCmd_SetPostedGroupParams, &posted_group_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Config: EZapiStat_ConfigCmd_SetPostedGroupParams failed.\n");
		return false;
	}

	return true;
}

/**************************************************************************//**
 * \brief       Create index pools
 *
 * \return      bool - success or failure
 */
bool infra_create_index_pools(void)
{
	EZstatus ret_val;
	EZapiChannel_IndexPoolParams index_pool_params;

	/* configure 2 pools for search */
	memset(&index_pool_params, 0, sizeof(index_pool_params));
	index_pool_params.uiPool = 0;

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
	index_pool_params.uiPool = 1;

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
	index_pool_params.uiPool = USER_POOL_ID;

	ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetIndexPoolParams, &index_pool_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiChannel_Status: EZapiChannel_StatCmd_GetIndexPoolParams failed.");
		return false;
	}

	index_pool_params.bEnable = true;
	index_pool_params.uiNumIndexes = 32*1024*1024;

	ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetIndexPoolParams, &index_pool_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiChannel_Status: EZapiChannel_ConfigCmd_SetIndexPoolParams failed.");
		return false;
	}

	return true;
}

/**************************************************************************//**
 * \brief       Create timers
 *
 * \return      bool - success or failure
 */
bool infra_create_timers(void)
{
	EZstatus ret_val;
	EZapiChannel_PMUTimerParams pmu_timer_params;

	pmu_timer_params.uiSide = 0;
	pmu_timer_params.uiTimer = 0;
	ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetPMUTimerParams, &pmu_timer_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiChannel_Status: EZapiChannel_StatCmd_GetPMUTimerParams failed.\n");
		return false;
	}

	pmu_timer_params.bEnable = true;
	pmu_timer_params.uiLogicalID = USER_TIMER_LOGICAL_ID;
	pmu_timer_params.uiPMUQueue = 0;   /* TODO - need a dedicated queue for timers */
	pmu_timer_params.uiNumJobs = 1*1024*1024;
	pmu_timer_params.uiNanoSecPeriod = 0;
	pmu_timer_params.uiSecPeriod = 960;  /* TODO - define */

	ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetPMUTimerParams, &pmu_timer_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiChannel_Config: EZapiChannel_ConfigCmd_SetPMUTimerParams failed.\n");
		return false;
	}

	return true;
}

/**************************************************************************//**
 * \brief       Infrastructure configuration at created state
 *
 * \return      bool - success or failure
 */
bool infra_created(void)
{
	/* create interfaces */
	if (infra_create_if_mapping() == false) {
		write_log(LOG_CRIT, "setup_chip: infra_create_if_mapping failed.\n");
		return false;
	}

	/* create memory partition */
	if (infra_create_mem_partition() == false) {
		write_log(LOG_CRIT, "setup_chip: infra_create_mem_partition failed.\n");
		return false;
	}

	/* create statistics */
	if (infra_create_statistics() == false) {
		write_log(LOG_CRIT, "setup_chip: infra_create_statistics failed.\n");
		return false;
	}

	/* configure protocol decode */
	if (infra_configure_protocol_decode() == false) {
		write_log(LOG_CRIT, "setup_chip: infra_configure_protocol_decode failed.\n");
		return false;
	}

	/* create index pools */
	if (infra_create_index_pools() == false) {
		write_log(LOG_CRIT, "setup_chip: infra_create_index_pools failed.\n");
		return false;
	}

	/* create index pools */
	if (infra_create_timers() == false) {
		write_log(LOG_CRIT, "setup_chip: infra_create_timers failed.\n");
		return false;
	}

	return true;
}


/**************************************************************************//**
 * \brief       Initialize all statistics counter to be zero
 *
 * \return      bool - success or failure
 */
bool infra_initialize_statistics(void)
{
	EZstatus ret_val;
	EZapiStat_LongCounterConfig long_counter_config;
	EZapiStat_PostedCounterConfig posted_counter_config;

	memset(&long_counter_config, 0, sizeof(long_counter_config));

	long_counter_config.pasCounters = malloc(sizeof(EZapiStat_LongCounter));
	if (long_counter_config.pasCounters == NULL) {
		write_log(LOG_CRIT, "infra_initialize_statistics: EZapiStat_LongCounter malloc failed.\n");
		return false;
	}
	memset(long_counter_config.pasCounters, 0, sizeof(EZapiStat_LongCounter));
	long_counter_config.uiPartition = 1;
	long_counter_config.bRange = true;
	long_counter_config.uiStartCounter = 0;
	long_counter_config.uiNumCounters = (36*1024*1024);
	long_counter_config.pasCounters[0].uiValue = 0;
	long_counter_config.pasCounters[0].uiValueMSB = 0;
	long_counter_config.pasCounters[0].bEnableThresholdMsg = false;
	long_counter_config.pasCounters[0].uiThreshold = 58;

	ret_val = EZapiStat_Config(0, EZapiStat_ConfigCmd_SetLongCounters, &long_counter_config);
	free(long_counter_config.pasCounters);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Config: EZapiStat_ConfigCmd_SetLongCounters failed.\n");
		return false;
	}

	/* Set posted statistics values to be 0 */
	memset(&posted_counter_config, 0, sizeof(posted_counter_config));
	posted_counter_config.pasCounters = malloc(sizeof(EZapiStat_PostedCounter));
	if (posted_counter_config.pasCounters == NULL) {
		write_log(LOG_CRIT, "infra_initialize_statistics: EZapiStat_PostedCounter malloc failed.\n");
		return false;
	}
	memset(posted_counter_config.pasCounters, 0, sizeof(EZapiStat_PostedCounter));
	posted_counter_config.uiPartition = 0;
	posted_counter_config.bRange = true;
	posted_counter_config.uiStartCounter = 0;
	posted_counter_config.uiNumCounters = (7680*1024);
	posted_counter_config.pasCounters[0].uiByteValue = 0;
	posted_counter_config.pasCounters[0].uiByteValueMSB = 0;
	posted_counter_config.pasCounters[0].uiFrameValue = 0;
	posted_counter_config.pasCounters[0].uiFrameValueMSB = 0;

	ret_val = EZapiStat_Config(0, EZapiStat_ConfigCmd_SetPostedCounters, &posted_counter_config);
	free(posted_counter_config.pasCounters);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Config: EZapiStat_ConfigCmd_SetPostedCounters failed.\n");
		return false;
	}

	return true;
}

/**************************************************************************//**
 * \brief       Infrastructure configuration at initialized state
 *
 * \return      bool - success or failure
 */
bool infra_initialized(void)
{
	EZstatus ez_ret_val;

	/* Launch NW DB constructor */
	if (nw_db_constructor() == false) {
		write_log(LOG_CRIT, "infra_initialized: nw_db_constructor failed.\n");
		return false;
	}

	/* Launch ALVS DB constructor */
	if (alvs_db_constructor() == false) {
		write_log(LOG_CRIT, "infra_initialized: alvs_db_constructor failed.\n");
		return false;
	}

	/* Load partition */
	write_log(LOG_DEBUG, "Load partition...\n");
	ez_ret_val = EZapiStruct_PartitionConfig(0, EZapiStruct_PartitionConfigCmd_LoadPartition, NULL);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "setup_chip: load partition failed.\n");
		return false;
	}

	/* Initialize statistics counters */
	if (infra_initialize_statistics() == false) {
		write_log(LOG_CRIT, "setup_chip: initialize_statistics failed.\n");
		return false;
	}

	return true;
}

/**************************************************************************//**
 * \brief       Infrastructure configuration at powered_up state
 *
 * \return      bool - success or failure
 */
bool infra_powered_up(void)
{
	/* Do nothing */
	return true;
}

/**************************************************************************//**
 * \brief       Infrastructure configuration at finalized state
 *
 * \return      bool - success or failure
 */
bool infra_finalized(void)
{
	/* Do nothing */
	return true;
}

/**************************************************************************//**
 * \brief       Infrastructure configuration at running state
 *
 * \return      bool - success or failure
 */
bool infra_running(void)
{
	/* Do nothing */
	return true;
}

/**************************************************************************//**
 * \brief       Find the index of the required memory heap
 *
 * \return      bool - success or failure
 */
uint32_t index_of(enum infra_search_mem_heaps search_mem_heap)
{
	uint32_t ind;

	/* Find the memory type in emem_spaces_params or imem_spaces_params */
	for (ind = 0; ind < NUM_OF_EXT_MEMORY_SPACES; ind++) {
		if (emem_spaces_params[ind][INFRA_EMEM_SPACES_PARAMS_HEAP] == search_mem_heap) {
			return emem_spaces_params[ind][INFRA_EMEM_SPACES_PARAMS_INDEX];
		}
	}
	for (ind = 0; ind < NUM_OF_INT_MEMORY_SPACES; ind++) {
		if (imem_spaces_params[ind][INFRA_IMEM_SPACES_PARAMS_HEAP] == search_mem_heap) {
			return imem_spaces_params[ind][INFRA_IMEM_SPACES_PARAMS_INDEX];
		}
	}

	/* Should not get here */
	write_log(LOG_CRIT, "Should not get here (with search_mem_heap=%d)\n", search_mem_heap);
	return 0;
}

/**************************************************************************//**
 * \brief       Create hash data structure
 *
 * \param[in]   struct_id       - structure id of the hash
 * \param[in]   search_mem_heap - memory heap where hash should reside on
 * \param[in]   params          - parameters of the hash (size of key & result,
 *                                max number of entries and update mode)
 *
 * \return      bool - success or failure
 */
bool infra_create_hash(uint32_t struct_id, enum infra_search_mem_heaps search_mem_heap, struct infra_hash_params *params)
{
	EZstatus ez_ret_val;
	EZapiStruct_StructParams struct_params;
	EZapiStruct_HashParams hash_params;
	EZapiStruct_HashMemMngParams hash_mem_mng_params;

	/* Get defaults of the structure */
	memset(&struct_params, 0, sizeof(struct_params));

	ez_ret_val = EZapiStruct_Status(struct_id, EZapiStruct_StatCmd_GetStructParams, &struct_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	/* Set structure to be hash with key size, result size, max number of entries and memory area. */
	struct_params.bEnable = true;
	struct_params.eStructType = EZapiStruct_StructType_HASH;
	struct_params.uiKeySize = params->key_size;
	struct_params.uiResultSize = params->result_size;
	struct_params.uiMaxEntries = params->max_num_of_entries;
	struct_params.sChannelMap.bSingleDest = true;
	struct_params.sChannelMap.uDest.uiChannel = 0;
	if (search_mem_heap == INFRA_EMEM_SEARCH_HASH_HEAP || search_mem_heap == INFRA_EMEM_SEARCH_TABLE_HEAP) {
		struct_params.eStructMemoryArea = EZapiStruct_MemoryArea_EXTERNAL;
	} else {
		struct_params.eStructMemoryArea = EZapiStruct_MemoryArea_INTERNAL;
	}

	ez_ret_val = EZapiStruct_Config(struct_id, EZapiStruct_ConfigCmd_SetStructParams, &struct_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}


	/* Get defaults of the hash parameters */
	memset(&hash_params, 0, sizeof(hash_params));

	ez_ret_val = EZapiStruct_Status(struct_id, EZapiStruct_StatCmd_GetHashParams, &hash_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	/* set single cycle and update mode */
	hash_params.bSingleCycle = true;
	if (params->updated_from_dp == true) {
		hash_params.eUpdateMode = EZapiStruct_UpdateMode_DP;
		hash_params.eMultiChannelDataMode = EZapiStruct_MultiChannelDataMode_DIFFERENT;
	} else {
		hash_params.eUpdateMode = EZapiStruct_UpdateMode_CP;
		hash_params.eMultiChannelDataMode = EZapiStruct_MultiChannelDataMode_IDENTICAL;
	}

	ez_ret_val = EZapiStruct_Config(struct_id, EZapiStruct_ConfigCmd_SetHashParams, &hash_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}


	/* Get defaults of the hash memory management */
	memset(&hash_mem_mng_params, 0, sizeof(hash_mem_mng_params));

	ez_ret_val = EZapiStruct_Status(struct_id, EZapiStruct_StatCmd_GetHashMemMngParams, &hash_mem_mng_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	/* Set index of the memory heap */
	if (search_mem_heap == INFRA_EMEM_SEARCH_HASH_HEAP || search_mem_heap == INFRA_EMEM_SEARCH_TABLE_HEAP) {
		hash_mem_mng_params.eSigPageMemoryArea = EZapiStruct_MemoryArea_EXTERNAL;
	} else {
		hash_mem_mng_params.eSigPageMemoryArea = EZapiStruct_MemoryArea_INTERNAL;
	}
	if (params->updated_from_dp == true) {
		hash_mem_mng_params.uiResultIndexPool = params->result_pool_id;
		hash_mem_mng_params.uiSigIndexPool = params->sig_pool_id;
	}
	hash_mem_mng_params.uiMainTableSpaceIndex = index_of(search_mem_heap);
	hash_mem_mng_params.uiSigSpaceIndex = index_of(search_mem_heap);
	hash_mem_mng_params.uiResSpaceIndex = index_of(search_mem_heap);

	ez_ret_val = EZapiStruct_Config(struct_id, EZapiStruct_ConfigCmd_SetHashMemMngParams, &hash_mem_mng_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	return true;
}

/**************************************************************************//**
 * \brief       Create table data structure
 *
 * \param[in]   struct_id       - structure id of the table
 * \param[in]   search_mem_heap - memory heap where table should reside on
 * \param[in]   params          - parameters of the table (size of key & result
 *                                and max number of entries)
 *
 * \return      bool - success or failure
 */
bool infra_create_table(uint32_t struct_id, enum infra_search_mem_heaps search_mem_heap, struct infra_table_params *params)
{
	EZstatus ez_ret_val;
	EZapiStruct_StructParams struct_params;
	EZapiStruct_TableMemMngParams table_mem_mng_params;

	/* Get defaults of the structure */
	memset(&struct_params, 0, sizeof(struct_params));
	ez_ret_val = EZapiStruct_Status(struct_id, EZapiStruct_StatCmd_GetStructParams, &struct_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	/* Set structure to be table with key size, result size, max number of entries and memory area. */
	struct_params.bEnable = true;
	struct_params.eStructType = EZapiStruct_StructType_TABLE;
	struct_params.uiKeySize = params->key_size;
	struct_params.uiResultSize = params->result_size;
	struct_params.uiMaxEntries = params->max_num_of_entries;
	struct_params.sChannelMap.bSingleDest = true;
	struct_params.sChannelMap.uDest.uiChannel = 0;
	if (search_mem_heap == INFRA_EMEM_SEARCH_HASH_HEAP || search_mem_heap == INFRA_EMEM_SEARCH_TABLE_HEAP) {
		struct_params.eStructMemoryArea = EZapiStruct_MemoryArea_EXTERNAL;
	} else {
		struct_params.eStructMemoryArea = EZapiStruct_MemoryArea_INTERNAL;
	}

	ez_ret_val = EZapiStruct_Config(struct_id, EZapiStruct_ConfigCmd_SetStructParams, &struct_params);

	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	/* Get defaults of the table memory management */
	memset(&table_mem_mng_params, 0, sizeof(table_mem_mng_params));

	ez_ret_val = EZapiStruct_Status(struct_id, EZapiStruct_StatCmd_GetTableMemMngParams, &table_mem_mng_params);

	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	/* Set index of the memory heap */
	table_mem_mng_params.uiSpaceIndex = index_of(search_mem_heap);

	ez_ret_val = EZapiStruct_Config(struct_id, EZapiStruct_ConfigCmd_SetTableMemMngParams, &table_mem_mng_params);

	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	return true;
}

/**************************************************************************//**
 * \brief       Add an entry to a data structure
 *
 * \param[in]   struct_id       - structure id of the search structure
 * \param[in]   key             - reference to key
 * \param[in]   key_size        - size of the key in bytes
 * \param[in]   result          - reference to result
 * \param[in]   result_size     - size of the result in bytes
 *
 * \return      bool - success or failure
 */
bool infra_add_entry(uint32_t struct_id, void *key, uint32_t key_size, void *result, uint32_t result_size)
{
	EZstatus ez_ret_val;
	EZapiEntry entry;

	/* Set entry with key and result to add */
	memset(&entry, 0, sizeof(entry));
	entry.uiKeySize = key_size;
	entry.pucKey = key;
	entry.uiResultSize = result_size;
	entry.pucResult = result;

	/* Add the entry */
	ez_ret_val = EZapiStruct_AddEntry(struct_id, NULL, &entry, NULL);

	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	return true;
}

/**************************************************************************//**
 * \brief       Modify an entry in a data structure
 *
 * \param[in]   struct_id       - structure id of the search structure
 * \param[in]   key             - reference to key
 * \param[in]   key_size        - size of the key in bytes
 * \param[in]   result          - reference to result
 * \param[in]   result_size     - size of the result in bytes
 *
 * \return      bool - success or failure
 */
bool infra_modify_entry(uint32_t struct_id, void *key, uint32_t key_size, void *result, uint32_t result_size)
{
	EZstatus ez_ret_val;
	EZapiEntry entry;

	/* Set entry with key and result to add */
	memset(&entry, 0, sizeof(entry));
	entry.uiKeySize = key_size;
	entry.pucKey = key;
	entry.uiResultSize = result_size;
	entry.pucResult = result;

	/* Add the entry */
	ez_ret_val = EZapiStruct_UpdateEntry(struct_id, NULL, &entry, NULL);

	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	return true;
}

/**************************************************************************//**
 * \brief       Delete an entry from a data structure
 *
 * \param[in]   struct_id       - structure id of the search structure
 * \param[in]   key             - reference to key
 * \param[in]   key_size        - size of the key in bytes
 *
 * \return      bool - success or failure
 */
bool infra_delete_entry(uint32_t struct_id, void *key, uint32_t key_size)
{
	EZstatus ez_ret_val;
	EZapiEntry entry;

	/* Set entry with key to delete */
	memset(&entry, 0, sizeof(entry));
	entry.uiKeySize = key_size;
	entry.pucKey = key;

	/* Delete the entry */
	ez_ret_val = EZapiStruct_DeleteEntry(struct_id, NULL, &entry, NULL);

	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	return true;
}

bool infra_delete_all_entries(uint32_t struct_id)
{
	EZstatus ez_ret_val;

	/* Delete all entries */
	ez_ret_val = EZapiStruct_DeleteAllEntries(struct_id, NULL, NULL);

	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	return true;
}

/**************************************************************************//**
 * \brief       Get my MAC
 *
 * \param[out]  my_mac - reference to ethernet address type
 *
 * \return      true - success
 *              false - can't find tap interface file
 */
bool infra_get_my_mac(struct ether_addr *my_mac)
{
	FILE *fd;
	uint fscanf_res;
	/* open address file from sys/class/net folder */
	fd = fopen("/sys/class/net/eth0/address", "r");

	if (fd == NULL) {
		return false;
	}

	/* read my MAC from file */
	fscanf_res = fscanf(fd, "%2hhx%*c%2hhx%*c%2hhx%*c%2hhx%*c%2hhx%*c%2hhx",
	       &my_mac->ether_addr_octet[0], &my_mac->ether_addr_octet[1],
	       &my_mac->ether_addr_octet[2], &my_mac->ether_addr_octet[3],
	       &my_mac->ether_addr_octet[4], &my_mac->ether_addr_octet[5]);

	if (fscanf_res != ETH_ALEN) {
		fclose(fd);
		return false;
	}

	fclose(fd);

	return true;
}

/**************************************************************************//**
 * \brief       Enable agent debug interface
 *
 * \return      bool - success or failure
 */
bool infra_enable_agt(void)
{
	EZstatus ez_ret_val;
	EZtask task;

	/* Create rpc server for given port */
	host_server = EZagtRPC_CreateServer(INFRA_AGT_PORT);
	if (host_server == NULL) {
		write_log(LOG_CRIT, "Can't create server for AGT\n");
		return false;
	}

	/* Register standard CP commands */
	ez_ret_val = EZagt_RegisterFunctions(host_server);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "Can't register function for AGT\n");
		return false;
	}

	/* Register standard CP commands */
	ez_ret_val = EZagtCPMain_RegisterFunctions(host_server);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "Can't register function for AGTcpMAIN\n");
		return false;
	}

	/* Create task for run rpc-json commands  */
	task = EZosTask_Spawn("agt", EZosTask_NORMAL_PRIORITY, 0x100000,
			      (EZosTask_Spawn_FuncPtr)EZagtRPC_ServerRun, host_server);
	if (task == EZosTask_INVALID_TASK) {
		write_log(LOG_CRIT, "Can't spawn AGT\n");
		return false;
	}

	return true;
}

/**************************************************************************//**
 * \brief       Disable agent debug interface
 *
 */
void infra_disable_agt(void)
{
	/* Stop server */
	EZagtRPC_ServerStop(host_server);

	EZosTask_Delay(10);

	/* Destroy server */
	EZagtRPC_ServerDestroy(host_server);
}

