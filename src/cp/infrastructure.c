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
#include <EZapiTCAM.h>

#include "log.h"
#include "infrastructure.h"
#include "infrastructure_utils.h"

#include "tc_init.h"

#include "alvs_conf.h"
#include "alvs_init.h"
#include "nw_conf.h"
#include "tc_conf.h"

#include "cfg.h"
#include "nw_init.h"

EZagtRPCServer host_server;

/* AGT port */
#define INFRA_AGT_PORT              1234

/* Network Interfaces Array */
uint32_t network_interface_params[NW_IF_NUM][INFRA_NUM_OF_INTERFACE_PARAMS] = {
		{INFRA_NW_IF_0_SIDE, INFRA_NW_IF_0_PORT},
		{INFRA_NW_IF_1_SIDE, INFRA_NW_IF_1_PORT},
		{INFRA_NW_IF_2_SIDE, INFRA_NW_IF_2_PORT},
		{INFRA_NW_IF_3_SIDE, INFRA_NW_IF_3_PORT}
};

/* Remote Interfaces Array */
uint32_t remote_interface_params[REMOTE_IF_NUM][INFRA_NUM_OF_INTERFACE_PARAMS] = {
		{INFRA_REMOTE_IF_0_SIDE, INFRA_REMOTE_IF_0_PORT},
		{INFRA_REMOTE_IF_1_SIDE, INFRA_REMOTE_IF_1_PORT},
		{INFRA_REMOTE_IF_2_SIDE, INFRA_REMOTE_IF_2_PORT},
		{INFRA_REMOTE_IF_3_SIDE, INFRA_REMOTE_IF_3_PORT}
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
	int ports_on_engine;

	if (type == EZapiChannel_EthIFType_10GE) {
		ports_on_engine = 12;
	} else if (type == EZapiChannel_EthIFType_40GE) {
		ports_on_engine = 3;
	} else	if (type == EZapiChannel_EthIFType_100GE) {
		ports_on_engine = 1;
	} else {
		write_log(LOG_ERR, "Unsupported interface type, supports only 100GE or 40GE or 10GE");
		return false;
	}

	/* set key values */
	/* port = engine*12 + if_number */
	eth_if_params.uiSide = side;
	eth_if_params.uiIFEngine = port_number / 12;
	eth_if_params.eEthIFType = type;
	eth_if_params.uiIFNumber = port_number % ports_on_engine;

	/* get default settings of the interface */
	ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetEthIFParams, &eth_if_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_NOTICE, "EZapiChannel_StatCmd_GetEthIFParams failed");
		return false;
	}

	/* set interface to be enabled and with TM bypass */
	eth_if_params.bRXEnable = true;
	eth_if_params.bTXEnable = true;
	eth_if_params.bTXTMBypass = true;

	ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetEthIFParams, &eth_if_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_NOTICE, "EZapiChannel_ConfigCmd_SetEthIFParams failed");
		return false;
	}

	/* set key values */
	/* port = engine*12 + if_number */
	eth_rx_channel_params.uiSide = side;
	eth_rx_channel_params.uiIFEngine  = port_number / 12;
	eth_rx_channel_params.eEthIFType  = type;
	eth_rx_channel_params.uiIFNumber  = port_number % ports_on_engine;
	eth_rx_channel_params.uiRXChannel  = 0;

	/* get default settings of the interface RX channel */
	ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetEthRXChannelParams, &eth_rx_channel_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_NOTICE, "EZapiChannel_StatCmd_GetEthRXChannelParams failed");
		return false;
	}

	/* set logical ID */
	eth_rx_channel_params.uiLogicalID = logical_id;
	eth_rx_channel_params.uiHeaderOffset = FRAME_HEADER_OFFSET;

	ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetEthRXChannelParams, &eth_rx_channel_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_NOTICE, "EZapiChannel_ConfigCmd_SetEthRXChannelParams failed");
		return false;
	}

	return true;
}

/**************************************************************************//**
 * \brief       Create all interfaces according to network_interface_params & remote_interface_params
 *
 * \return      bool - success or failure
 */
bool infra_create_if_mapping(void)
{
	uint32_t ind;

	/* Configure network interfaces */
	for (ind = 0; ind < NW_IF_NUM; ind++) {
		if (infra_create_intetface(network_interface_params[ind][INFRA_INTERFACE_PARAMS_SIDE],
				network_interface_params[ind][INFRA_INTERFACE_PARAMS_PORT],
				system_cfg_get_port_type(),
				NW_BASE_LOGICAL_ID + ind) == false) {
			return false;
		}
	}

	if (system_cfg_is_remote_control_en() == true) {
		/* Configure remote interfaces */
		for (ind = 0; ind < REMOTE_IF_NUM; ind++) {
			if (infra_create_intetface(remote_interface_params[ind][INFRA_INTERFACE_PARAMS_SIDE],
					remote_interface_params[ind][INFRA_INTERFACE_PARAMS_PORT],
					EZapiChannel_EthIFType_10GE,
					REMOTE_BASE_LOGICAL_ID + ind) == false) {
				return false;
			}
		}
	}

	/* Configure interface to host */
	if (infra_create_intetface(INFRA_HOST_IF_SIDE,
			INFRA_HOST_IF_PORT,
			EZapiChannel_EthIFType_10GE,
			HOST_LOGICAL_ID) == false) {
		return false;
	}

	return true;
}


/*! IMEM spaces configuration parameters possible values. */
enum infra_imem_spaces_params {
	INFRA_IMEM_SPACES_PARAMS_TYPE               = 0,
	INFRA_IMEM_SPACES_PARAMS_SIZE               = 1,
	INFRA_IMEM_SPACES_PARAMS_HEAP               = 2,
	INFRA_NUM_OF_IMEM_SPACES_PARAMS
};

/*! EMEM spaces configuration parameters possible values. */

#define EMEM_SPACES_DEFAULT_NUM_OF_COPIES 2

enum infra_emem_spaces_params {
	INFRA_EMEM_SPACES_PARAMS_TYPE               = 0,
	INFRA_EMEM_SPACES_PARAMS_PROTECTION         = 1,
	INFRA_EMEM_SPACES_PARAMS_SIZE               = 2,
	INFRA_EMEM_SPACES_PARAMS_MSID               = 3,
	INFRA_EMEM_SPACES_PARAMS_HEAP               = 4,
	INFRA_EMEM_SPACES_PARAMS_NUM_OF_COPIES      = 5,
	INFRA_NUM_OF_EMEM_SPACES_PARAMS
};


#define INFRA_HALF_CLUSTER_CODE_SIZE          0
#define INFRA_1_CLUSTER_CODE_SIZE             90
#define INFRA_2_CLUSTER_CODE_SIZE             0
#define INFRA_4_CLUSTER_CODE_SIZE             0
#define INFRA_16_CLUSTER_CODE_SIZE            0
#define INFRA_ALL_CLUSTER_CODE_SIZE           256


#define NUM_OF_INT_MEMORY_SPACES            18

#ifdef CONFIG_ALVS
#	define NUM_OF_EXT_MEMORY_SPACES            6
#endif
#ifdef CONFIG_TC
#	define NUM_OF_EXT_MEMORY_SPACES            4
#endif

#define INFRA_HALF_CLUSTER_SEARCH_SIZE (NW_HALF_CLUSTER_SEARCH_SIZE + ALVS_HALF_CLUSTER_SEARCH_SIZE + TC_HALF_CLUSTER_SEARCH_SIZE)
#define INFRA_1_CLUSTER_SEARCH_SIZE (NW_1_CLUSTER_SEARCH_SIZE + ALVS_1_CLUSTER_SEARCH_SIZE + TC_1_CLUSTER_SEARCH_SIZE)
#define INFRA_2_CLUSTER_SEARCH_SIZE (NW_2_CLUSTER_SEARCH_SIZE + ALVS_2_CLUSTER_SEARCH_SIZE + TC_2_CLUSTER_SEARCH_SIZE)
#define INFRA_4_CLUSTER_SEARCH_SIZE (NW_4_CLUSTER_SEARCH_SIZE + ALVS_4_CLUSTER_SEARCH_SIZE + TC_4_CLUSTER_SEARCH_SIZE)
#define INFRA_16_CLUSTER_SEARCH_SIZE (NW_16_CLUSTER_SEARCH_SIZE + ALVS_16_CLUSTER_SEARCH_SIZE + TC_16_CLUSTER_SEARCH_SIZE)
#define INFRA_ALL_CLUSTER_SEARCH_SIZE (NW_ALL_CLUSTER_SEARCH_SIZE + ALVS_ALL_CLUSTER_SEARCH_SIZE + TC_HALF_CLUSTER_DATA_SIZE)


#define INFRA_HALF_CLUSTER_DATA_SIZE (NW_HALF_CLUSTER_DATA_SIZE + ALVS_HALF_CLUSTER_DATA_SIZE + TC_HALF_CLUSTER_DATA_SIZE)
#define INFRA_1_CLUSTER_DATA_SIZE (NW_1_CLUSTER_DATA_SIZE + ALVS_1_CLUSTER_DATA_SIZE + TC_1_CLUSTER_DATA_SIZE)
#define INFRA_2_CLUSTER_DATA_SIZE (NW_2_CLUSTER_DATA_SIZE + ALVS_2_CLUSTER_DATA_SIZE + TC_2_CLUSTER_DATA_SIZE)
#define INFRA_4_CLUSTER_DATA_SIZE (NW_4_CLUSTER_DATA_SIZE + ALVS_4_CLUSTER_DATA_SIZE + TC_4_CLUSTER_DATA_SIZE)
#define INFRA_16_CLUSTER_DATA_SIZE (NW_16_CLUSTER_DATA_SIZE + ALVS_16_CLUSTER_DATA_SIZE + TC_16_CLUSTER_DATA_SIZE)
#define INFRA_ALL_CLUSTER_DATA_SIZE (NW_ALL_CLUSTER_DATA_SIZE + ALVS_ALL_CLUSTER_DATA_SIZE + TC_ALL_CLUSTER_DATA_SIZE)


uint32_t imem_spaces_params[NUM_OF_INT_MEMORY_SPACES][INFRA_NUM_OF_IMEM_SPACES_PARAMS] = {
	{EZapiChannel_IntMemSpaceType_HALF_CLUSTER_CODE, INFRA_HALF_CLUSTER_CODE_SIZE, INFRA_NOT_VALID_INTERNAL_HEAP},
	{EZapiChannel_IntMemSpaceType_HALF_CLUSTER_DATA, INFRA_HALF_CLUSTER_DATA_SIZE, INFRA_NOT_VALID_INTERNAL_HEAP},
	{EZapiChannel_IntMemSpaceType_HALF_CLUSTER_SEARCH, INFRA_HALF_CLUSTER_SEARCH_SIZE, INFRA_HALF_CLUSTER_SEARCH_HEAP},
	{EZapiChannel_IntMemSpaceType_1_CLUSTER_CODE, INFRA_1_CLUSTER_CODE_SIZE, INFRA_NOT_VALID_INTERNAL_HEAP},
	{EZapiChannel_IntMemSpaceType_1_CLUSTER_DATA, INFRA_1_CLUSTER_DATA_SIZE, INFRA_NOT_VALID_INTERNAL_HEAP},
	{EZapiChannel_IntMemSpaceType_1_CLUSTER_SEARCH, INFRA_1_CLUSTER_SEARCH_SIZE, INFRA_1_CLUSTER_SEARCH_HEAP},
	{EZapiChannel_IntMemSpaceType_2_CLUSTER_CODE, INFRA_2_CLUSTER_CODE_SIZE, INFRA_NOT_VALID_INTERNAL_HEAP},
	{EZapiChannel_IntMemSpaceType_2_CLUSTER_DATA, INFRA_2_CLUSTER_DATA_SIZE, INFRA_NOT_VALID_INTERNAL_HEAP},
	{EZapiChannel_IntMemSpaceType_2_CLUSTER_SEARCH, INFRA_2_CLUSTER_SEARCH_SIZE, INFRA_2_CLUSTER_SEARCH_HEAP},
	{EZapiChannel_IntMemSpaceType_4_CLUSTER_CODE, INFRA_4_CLUSTER_CODE_SIZE, INFRA_NOT_VALID_INTERNAL_HEAP},
	{EZapiChannel_IntMemSpaceType_4_CLUSTER_DATA, INFRA_4_CLUSTER_DATA_SIZE, INFRA_NOT_VALID_INTERNAL_HEAP},
	{EZapiChannel_IntMemSpaceType_4_CLUSTER_SEARCH, INFRA_4_CLUSTER_SEARCH_SIZE, INFRA_4_CLUSTER_SEARCH_HEAP},
	{EZapiChannel_IntMemSpaceType_16_CLUSTER_CODE, INFRA_16_CLUSTER_CODE_SIZE, INFRA_NOT_VALID_INTERNAL_HEAP},
	{EZapiChannel_IntMemSpaceType_16_CLUSTER_DATA, INFRA_16_CLUSTER_DATA_SIZE, INFRA_NOT_VALID_INTERNAL_HEAP},
	{EZapiChannel_IntMemSpaceType_16_CLUSTER_SEARCH, INFRA_16_CLUSTER_SEARCH_SIZE, INFRA_16_CLUSTER_SEARCH_HEAP},
	{EZapiChannel_IntMemSpaceType_ALL_CLUSTER_CODE, INFRA_ALL_CLUSTER_CODE_SIZE, INFRA_NOT_VALID_INTERNAL_HEAP},
	{EZapiChannel_IntMemSpaceType_ALL_CLUSTER_DATA, INFRA_ALL_CLUSTER_DATA_SIZE, INFRA_NOT_VALID_INTERNAL_HEAP},
	{EZapiChannel_IntMemSpaceType_ALL_CLUSTER_SEARCH, INFRA_ALL_CLUSTER_SEARCH_SIZE, INFRA_ALL_CLUSTER_SEARCH_HEAP}
};


uint32_t emem_spaces_params[NUM_OF_EXT_MEMORY_SPACES][INFRA_NUM_OF_EMEM_SPACES_PARAMS] = {
	{EZapiChannel_ExtMemSpaceType_SEARCH, EZapiChannel_ExtMemSpaceECCType_NONE, NW_EMEM_SEARCH_SIZE, NW_EMEM_SEARCH_MSID, NW_EMEM_SEARCH_HEAP, EMEM_SPACES_DEFAULT_NUM_OF_COPIES},
	{EZapiChannel_ExtMemSpaceType_GENERAL, EZapiChannel_ExtMemSpaceECCType_OUT_OF_BAND, NW_EMEM_DATA_SIZE, NW_EMEM_DATA_MSID, INFRA_NOT_VALID_EXTERNAL_HEAP, EMEM_SPACES_DEFAULT_NUM_OF_COPIES},
#ifdef CONFIG_ALVS
	{EZapiChannel_ExtMemSpaceType_SEARCH, EZapiChannel_ExtMemSpaceECCType_NONE, ALVS_EMEM_SEARCH_0_SIZE, ALVS_EMEM_SEARCH_0_MSID, ALVS_EMEM_SEARCH_0_HEAP, EMEM_SPACES_DEFAULT_NUM_OF_COPIES},
	{EZapiChannel_ExtMemSpaceType_SEARCH, EZapiChannel_ExtMemSpaceECCType_NONE, ALVS_EMEM_SEARCH_1_SIZE, ALVS_EMEM_SEARCH_1_MSID, ALVS_EMEM_SEARCH_1_HEAP, EMEM_SPACES_DEFAULT_NUM_OF_COPIES},
	{EZapiChannel_ExtMemSpaceType_SEARCH, EZapiChannel_ExtMemSpaceECCType_NONE, ALVS_EMEM_SEARCH_2_SIZE, ALVS_EMEM_SEARCH_2_MSID, ALVS_EMEM_SEARCH_2_HEAP, EMEM_SPACES_DEFAULT_NUM_OF_COPIES},
	{EZapiChannel_ExtMemSpaceType_GENERAL, EZapiChannel_ExtMemSpaceECCType_OUT_OF_BAND, ALVS_EMEM_DATA_OUT_OF_BAND_SIZE, ALVS_EMEM_DATA_OUT_OF_BAND_MSID, INFRA_NOT_VALID_EXTERNAL_HEAP, EMEM_SPACES_DEFAULT_NUM_OF_COPIES},
#endif
#ifdef CONFIG_TC
	{EZapiChannel_ExtMemSpaceType_SEARCH, EZapiChannel_ExtMemSpaceECCType_NONE, TC_EMEM_SEARCH_0_SIZE, TC_EMEM_SEARCH_0_MSID, TC_EMEM_SEARCH_0_HEAP, EMEM_SPACES_DEFAULT_NUM_OF_COPIES},
	{EZapiChannel_ExtMemSpaceType_SEARCH, EZapiChannel_ExtMemSpaceECCType_NONE, TC_EMEM_SEARCH_1_SIZE, TC_EMEM_SEARCH_1_MSID, TC_EMEM_SEARCH_1_HEAP, 1},
#endif
};


uint32_t infra_index_of_heap(uint32_t search_mem_heap,
			     bool is_external)
{
	uint32_t ind;

	if (is_external) {
		for (ind = 0; ind < NUM_OF_EXT_MEMORY_SPACES; ind++) {
			if (emem_spaces_params[ind][INFRA_EMEM_SPACES_PARAMS_HEAP] == search_mem_heap) {
				write_log(LOG_DEBUG, "Found index %d for search_mem_heap=%d in EMEM", emem_spaces_params[ind][INFRA_EMEM_SPACES_PARAMS_MSID], search_mem_heap);
				return emem_spaces_params[ind][INFRA_EMEM_SPACES_PARAMS_MSID];
			}
		}
	} else {
		for (ind = 0; ind < NUM_OF_INT_MEMORY_SPACES; ind++) {
			if (imem_spaces_params[ind][INFRA_IMEM_SPACES_PARAMS_HEAP] == search_mem_heap) {
				write_log(LOG_DEBUG, "Found index %d for search_mem_heap=%d in IMEM", imem_spaces_params[ind][INFRA_IMEM_SPACES_PARAMS_TYPE], search_mem_heap);
				return imem_spaces_params[ind][INFRA_IMEM_SPACES_PARAMS_TYPE];
			}
		}
	}

	/* Should not get here */
	write_log(LOG_CRIT, "Should not get here (with search_mem_heap=%d)", search_mem_heap);
	return 0;
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

	/* Disable first 3 indexes that are enabled by default */
	for (ind = 0; ind < 3; ind++) {
		memset(&int_mem_space_params, 0, sizeof(int_mem_space_params));

		int_mem_space_params.uiIndex = ind;

		/* get default settings for the memory space */
		ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetIntMemSpaceParams, &int_mem_space_params);
		if (EZrc_IS_ERROR(ret_val)) {
			return false;
		}

		int_mem_space_params.bEnable = false;

		ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetIntMemSpaceParams, &int_mem_space_params);
		if (EZrc_IS_ERROR(ret_val)) {
			write_log(LOG_CRIT, "fail disable IMEM index = %d (rc = 0x%08x)\n", int_mem_space_params.uiIndex, ret_val);
			return false;
		}

		write_log(LOG_DEBUG, "IMEM index %d disabled", ind);
	}

	/* Configure IMEM memory spaces */
	for (ind = 0; ind < NUM_OF_INT_MEMORY_SPACES; ind++) {
		/* Configure only memory spaces with positive size */
		if (imem_spaces_params[ind][INFRA_IMEM_SPACES_PARAMS_SIZE] > 0) {
			memset(&int_mem_space_params, 0, sizeof(int_mem_space_params));

			/* Index is type(=MSID) */
			int_mem_space_params.uiIndex = imem_spaces_params[ind][INFRA_IMEM_SPACES_PARAMS_TYPE];

			/* get default settings for the memory space */
			ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetIntMemSpaceParams, &int_mem_space_params);
			if (EZrc_IS_ERROR(ret_val)) {
				return false;
			}

			/* set size and type */
			int_mem_space_params.eType = (EZapiChannel_IntMemSpaceType)imem_spaces_params[ind][INFRA_IMEM_SPACES_PARAMS_TYPE];
			int_mem_space_params.bEnable = true;
			int_mem_space_params.uiSize = imem_spaces_params[ind][INFRA_IMEM_SPACES_PARAMS_SIZE];

			ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetIntMemSpaceParams, &int_mem_space_params);
			if (EZrc_IS_ERROR(ret_val)) {
				write_log(LOG_CRIT, "fail IMEM config index = %d, size = %d (rc = 0x%08x)\n", int_mem_space_params.uiIndex, int_mem_space_params.uiSize, ret_val);
				return false;
			}

			write_log(LOG_DEBUG, "IMEM[%d] = {index %d, size %d}", ind, int_mem_space_params.uiIndex, int_mem_space_params.uiSize);
		}
	}

	/* Configure EMEM memory spaces */
	for (ind = 0; ind < NUM_OF_EXT_MEMORY_SPACES; ind++) {
		/* Configure only memory spaces with positive size */
		if (emem_spaces_params[ind][INFRA_EMEM_SPACES_PARAMS_SIZE] > 0) {
			memset(&ext_mem_space_params, 0, sizeof(ext_mem_space_params));

			/* Index is MSID */
			ext_mem_space_params.uiIndex = emem_spaces_params[ind][INFRA_EMEM_SPACES_PARAMS_MSID];

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

			if (ext_mem_space_params.eType == EZapiChannel_ExtMemSpaceType_SEARCH) {
				ext_mem_space_params.uiCopies = emem_spaces_params[ind][INFRA_EMEM_SPACES_PARAMS_NUM_OF_COPIES];
			}

			ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetExtMemSpaceParams, &ext_mem_space_params);
			if (EZrc_IS_ERROR(ret_val)) {
				return false;
			}

			write_log(LOG_DEBUG, "EMEM[%d] = {msid=%d, size=%d}.", ind, ext_mem_space_params.uiMSID, ext_mem_space_params.uiSize);
		}
	}

	return true;
}


#if ((NW_TOTAL_ON_DEMAND_STATS + ALVS_TOTAL_ON_DEMAND_STATS + TC_TOTAL_ON_DEMAND_STATS) <= (16*1024*1024))
#	define TOTAL_ON_DEMAND_STATS  (16*1024*1024)
#else
#	error "Num of on demand statistics exceeds 16M"
#endif

#if ((NW_TOTAL_POSTED_STATS + ALVS_TOTAL_POSTED_STATS + TC_TOTAL_POSTED_STATS) <= (7680*1024))
#	define TOTAL_POSTED_STATS  (7680*1024)
#else
#	error "Num of posted statistics exceeds 7680K"
#endif


/**************************************************************************//**
 * \brief       Configure Statistics
 *
 * \return      bool - success or failure
 */
bool infra_create_statistics(void)
{
	EZstatus ret_val;
	EZapiStat_PostedPartitionParams posted_partition_params;
	EZapiStat_PostedGroupParams posted_group_params;
	EZapiStat_PartitionParams on_demand_partition_params;
	EZapiStat_GroupParams on_demand_group_params;

	/* Get on_demand statistics defaults for partition 0 */
	memset(&on_demand_partition_params, 0, sizeof(on_demand_partition_params));

	on_demand_partition_params.uiPartition = 0;

	ret_val = EZapiStat_Status(0, EZapiStat_StatCmd_GetPartitionParams, &on_demand_partition_params);

	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Status: EZapiStat_StatCmd_GetPartitionParams failed.");
		return false;
	}

	/* Set MSID */
	on_demand_partition_params.bEnable = TRUE;
	on_demand_partition_params.uiMSID = NW_ON_DEMAND_STATS_MSID;
	ret_val = EZapiStat_Config(0, EZapiStat_ConfigCmd_SetPartitionParams, &on_demand_partition_params);

	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Status: EZapiStat_ConfigCmd_SetPartitionParams failed.");
		return false;
	}

	/* On_demand statistics defaults for partition 0 - for long counters and TB*/
	memset(&on_demand_group_params, 0, sizeof(on_demand_group_params));

	on_demand_group_params.uiPartition = 0;
	on_demand_group_params.eGroupType = EZapiStat_GroupType_STANDARD;
	on_demand_group_params.uiGroup = 0;
	ret_val = EZapiStat_Status(0, EZapiStat_StatCmd_GetGroupParams, &on_demand_group_params);

	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Status: EZapiStat_StatCmd_GetGroupParams failed.");
		return false;
	}

	on_demand_group_params.uiStartCounter = 0;
	on_demand_group_params.uiNumCounters = TOTAL_ON_DEMAND_STATS;
	on_demand_group_params.eGroupType = EZapiStat_GroupType_STANDARD;
	ret_val = EZapiStat_Config(0, EZapiStat_ConfigCmd_SetGroupParams, &on_demand_group_params);

	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Status: EZapiStat_ConfigCmd_SetGroupParams failed.");
		return false;
	}



	/* Get posted statistics defaults for partition 0 */
	memset(&posted_partition_params, 0, sizeof(posted_partition_params));
	posted_partition_params.uiPartition = 0;
	ret_val = EZapiStat_Status(0, EZapiStat_StatCmd_GetPostedPartitionParams, &posted_partition_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Status: EZapiStat_StatCmd_GetPostedPartitionParams failed.");
		return false;
	}

	/* POSTED counters */
	posted_partition_params.bEnable = true;
	posted_partition_params.uiMSID = NW_POSTED_STATS_MSID;
	ret_val = EZapiStat_Config(0, EZapiStat_ConfigCmd_SetPostedPartitionParams, &posted_partition_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Config: EZapiStat_ConfigCmd_SetPostedPartitionParams failed.");
		return false;
	}


	/* Get posted statistics defaults for partition 0 */
	memset(&posted_group_params, 0, sizeof(posted_group_params));
	posted_group_params.uiPartition = 0;
	posted_group_params.eGroupType = EZapiStat_PostedGroupType_OPTIMIZED;
	ret_val = EZapiStat_Status(0, EZapiStat_StatCmd_GetPostedGroupParams, &posted_group_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Status: EZapiStat_StatCmd_GetPostedGroupParams failed.");
		return false;
	}

	/* Set group size and optimized group type */
	posted_group_params.uiNumCounters = TOTAL_POSTED_STATS;

	ret_val = EZapiStat_Config(0, EZapiStat_ConfigCmd_SetPostedGroupParams, &posted_group_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Config: EZapiStat_ConfigCmd_SetPostedGroupParams failed.");
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
#ifdef CONFIG_ALVS
	if (alvs_create_index_pools() == false) {
		write_log(LOG_CRIT, "infra_create_index_pools: alvs_create_index_pools failed.");
		return false;
	}
#endif

	return true;
}

/**************************************************************************//**
 * \brief       Create timers
 *
 * \return      bool - success or failure
 */
bool infra_create_timers(void)
{
#ifdef CONFIG_ALVS
	if (alvs_create_timers() == false) {
		write_log(LOG_CRIT, "infra_create_timers: alvs_create_timers failed.");
		return false;
	}
#endif

	return true;
}

/**************************************************************************//**
 * \brief       Enable real time clock on NPS
 *
 * \return      bool - success or failure
 */
bool infra_enable_real_time_clock(void)
{
	EZapiChannel_RTCParams  rtc_params;
	EZstatus                ret_val;

	/* init variables */
	memset(&rtc_params, 0, sizeof(rtc_params));

	/* get RTC configuration (default) */
	ret_val = EZapiChannel_Status(REAL_TIME_CLOCK_CHANNEL_ID,
				      EZapiChannel_StatCmd_GetRTCParams,
				      &rtc_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_NOTICE, "EZapiChannel_StatCmd_GetRTCParams failed");
		return false;
	}

	/* Enable RTC */
	rtc_params.bEnable = TRUE;
	ret_val = EZapiChannel_Config(REAL_TIME_CLOCK_CHANNEL_ID,
				      EZapiChannel_ConfigCmd_SetRTCParams,
				      &rtc_params);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_NOTICE, "EZapiChannel_ConfigCmd_SetRTCParams failed");
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
		write_log(LOG_CRIT, "setup_chip: infra_create_if_mapping failed.");
		return false;
	}

	/* create memory partition */
	if (infra_create_mem_partition() == false) {
		write_log(LOG_CRIT, "setup_chip: infra_create_mem_partition failed.");
		return false;
	}

	/* create statistics */
	if (infra_create_statistics() == false) {
		write_log(LOG_CRIT, "setup_chip: infra_create_mem_partition failed.");
		return false;
	}

	/* create index pools */
	if (infra_create_index_pools() == false) {
		write_log(LOG_CRIT, "setup_chip: infra_create_index_pools failed.");
		return false;
	}

#ifdef CONFIG_ALVS
	/* create timers */
	if (infra_create_timers() == false) {
		write_log(LOG_CRIT, "setup_chip: infra_create_timers failed.");
		return false;
	}
#endif

	/* initialize protocol decode */
	if (nw_initialize_protocol_decode() == false) {
		write_log(LOG_CRIT, "setup_chip: nw_initialize_protocol_decode failed.");
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

	/* Launch DB constructor */
	if (nw_constructor() == false) {
		write_log(LOG_CRIT, "infra_initialized: nw_constructor failed.");
		return false;
	}
#ifdef CONFIG_ALVS
	if (alvs_db_constructor() == false) {
		write_log(LOG_CRIT, "infra_initialized: alvs_db_constructor failed.");
		return false;
	}
#endif
#ifdef CONFIG_TC
	if (tc_db_constructor() == false) {
		write_log(LOG_CRIT, "infra_initialized: tc_db_constructor failed.");
		return false;
	}
#endif
	/* Load partition */
	write_log(LOG_DEBUG, "Load partition...");
	ez_ret_val = EZapiStruct_PartitionConfig(0, EZapiStruct_PartitionConfigCmd_LoadPartition, NULL);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "infra_initialized: load partition failed (rc = 0x%08x)", ez_ret_val);
		return false;
	}

	/* Initialize tables */
	if (nw_initialize_tables() == false) {
		write_log(LOG_CRIT, "infra_initialized: nw_initialize_tables failed.");
		return false;
	}

	/* Initialize statistics counters */
	if (nw_initialize_statistics() == false) {
		write_log(LOG_CRIT, "infra_initialized: nw_initialize_statistics failed.");
		return false;
	}
#ifdef CONFIG_ALVS
	if (alvs_initialize_statistics() == false) {
		write_log(LOG_CRIT, "infra_initialized: alvs_initialize_statistics failed.");
		return false;
	}
#endif
#ifdef CONFIG_TC
	if (tc_initialize_statistics() == false) {
		write_log(LOG_CRIT, "infra_initialized: tc_db_constructor failed.");
		return false;
	}

	if (infra_enable_real_time_clock() == false) {
		write_log(LOG_CRIT, "setup_chip: enable_real_time_clock failed.");
		return false;
	}
#endif

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
		write_log(LOG_CRIT, "Can't create server for AGT");
		return false;
	}

	/* Register standard CP commands */
	ez_ret_val = EZagt_RegisterFunctions(host_server);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "Can't register function for AGT");
		return false;
	}

	/* Register standard CP commands */
	ez_ret_val = EZagtCPMain_RegisterFunctions(host_server);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "Can't register function for AGTcpMAIN");
		return false;
	}

	/* Create task for run rpc-json commands  */
	task = EZosTask_Spawn("agt", EZosTask_NORMAL_PRIORITY, 0x100000,
			      (EZosTask_Spawn_FuncPtr)EZagtRPC_ServerRun, host_server);
	if (task == EZosTask_INVALID_TASK) {
		write_log(LOG_CRIT, "Can't spawn AGT");
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

/**************************************************************************//**
 * \brief       Destruct all DBs
 *
 */
void infra_db_destructor(void)
{
	nw_destructor();
}
