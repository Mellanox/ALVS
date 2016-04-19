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
*/

#include "infrastructure.h"
#include "infrastructure_conf.h"
#include <stdint.h>
#include <string.h>
#include <EZapiChannel.h>
#include <stdio.h>

enum infra_imem_spaces_params {
	INFRA_IMEM_SPACES_PARAMS_TYPE             = 0,
	INFRA_IMEM_SPACES_PARAMS_SIZE             = 1,
	INFRA_NUM_OF_IMEM_SPACES_PARAMS
};

enum infra_emem_spaces_params {
	INFRA_EMEM_SPACES_PARAMS_TYPE             = 0,
	INFRA_EMEM_SPACES_PARAMS_PROTECTION       = 1,
	INFRA_EMEM_SPACES_PARAMS_SIZE             = 2,
	INFRA_NUM_OF_EMEM_SPACES_PARAMS
};

#define HALF_CLUSTER_CODE_SIZE 0
#define X1_CLUSTER_CODE_SIZE 0
#define X2_CLUSTER_CODE_SIZE 0
#define X4_CLUSTER_CODE_SIZE 0
#define X16_CLUSTER_CODE_SIZE 0
#define ALL_CLUSTER_CODE_SIZE 0

#define NUM_OF_INT_MEMORY_SPACES 18
#define NUM_OF_EXT_MEMORY_SPACES 4

static
uint32_t imem_spaces_params[NUM_OF_INT_MEMORY_SPACES][INFRA_NUM_OF_IMEM_SPACES_PARAMS] = {
		{EZapiChannel_IntMemSpaceType_HALF_CLUSTER_CODE, HALF_CLUSTER_CODE_SIZE},
		{EZapiChannel_IntMemSpaceType_HALF_CLUSTER_DATA, HALF_CLUSTER_DATA_SIZE},
		{EZapiChannel_IntMemSpaceType_HALF_CLUSTER_SEARCH, HALF_CLUSTER_SEARCH_SIZE},
		{EZapiChannel_IntMemSpaceType_1_CLUSTER_CODE, X1_CLUSTER_CODE_SIZE},
		{EZapiChannel_IntMemSpaceType_1_CLUSTER_DATA, X1_CLUSTER_DATA_SIZE},
		{EZapiChannel_IntMemSpaceType_1_CLUSTER_SEARCH, X1_CLUSTER_SEARCH_SIZE},
		{EZapiChannel_IntMemSpaceType_2_CLUSTER_CODE, X2_CLUSTER_CODE_SIZE},
		{EZapiChannel_IntMemSpaceType_2_CLUSTER_DATA, X2_CLUSTER_DATA_SIZE},
		{EZapiChannel_IntMemSpaceType_2_CLUSTER_SEARCH, X2_CLUSTER_SEARCH_SIZE},
		{EZapiChannel_IntMemSpaceType_4_CLUSTER_CODE, X4_CLUSTER_CODE_SIZE},
		{EZapiChannel_IntMemSpaceType_4_CLUSTER_DATA, X4_CLUSTER_DATA_SIZE},
		{EZapiChannel_IntMemSpaceType_4_CLUSTER_SEARCH, X4_CLUSTER_SEARCH_SIZE},
		{EZapiChannel_IntMemSpaceType_16_CLUSTER_CODE, X16_CLUSTER_CODE_SIZE},
		{EZapiChannel_IntMemSpaceType_16_CLUSTER_DATA, X16_CLUSTER_DATA_SIZE},
		{EZapiChannel_IntMemSpaceType_16_CLUSTER_SEARCH, X16_CLUSTER_SEARCH_SIZE},
		{EZapiChannel_IntMemSpaceType_ALL_CLUSTER_CODE, ALL_CLUSTER_CODE_SIZE},
		{EZapiChannel_IntMemSpaceType_ALL_CLUSTER_DATA, ALL_CLUSTER_DATA_SIZE},
		{EZapiChannel_IntMemSpaceType_ALL_CLUSTER_SEARCH, ALL_CLUSTER_SEARCH_SIZE}
};

static
uint32_t emem_spaces_params[NUM_OF_EXT_MEMORY_SPACES][INFRA_NUM_OF_EMEM_SPACES_PARAMS] = {
		{EZapiChannel_ExtMemSpaceType_GENERAL, EZapiChannel_ExtMemSpaceECCType_NONE, EMEM_DATA_NO_ECC_SIZE},
		{EZapiChannel_ExtMemSpaceType_GENERAL, EZapiChannel_ExtMemSpaceECCType_IN_BAND, EMEM_DATA_IN_BAND_SIZE},
		{EZapiChannel_ExtMemSpaceType_GENERAL, EZapiChannel_ExtMemSpaceECCType_OUT_OF_BAND, EMEM_DATA_OUT_OF_BAND_SIZE},
		{EZapiChannel_ExtMemSpaceType_SEARCH, EZapiChannel_ExtMemSpaceECCType_IN_BAND, EMEM_SEARCH_SIZE}
};

bool infra_create_if_mapping(void)
{
	uint32_t ind;
	EZstatus ez_ret_val;
	EZapiChannel_EthIFParams eth_if_params;
	EZapiChannel_EthRXChannelParams eth_rx_channel_params;

	/* Configure external interfaces */
	for(ind = 0; ind < INFRA_NW_IF_NUM; ind++) {
		eth_if_params.uiSide = INFRA_NW_IF_SIDE;
		eth_if_params.uiIFEngine = ind;
		eth_if_params.eEthIFType = INFRA_NW_IF_TYPE;
		eth_if_params.uiIFNumber = 0;

		ez_ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetEthIFParams, &eth_if_params);
		if (EZrc_IS_ERROR(ez_ret_val)) {
			return false;
		}

		eth_if_params.bRXEnable = true;
		eth_if_params.bTXEnable = true;
		eth_if_params.bTXTMBypass = true;

		ez_ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetEthIFParams, &eth_if_params);
		if (EZrc_IS_ERROR(ez_ret_val)) {
			return false;
		}


		eth_rx_channel_params.uiSide = INFRA_NW_IF_SIDE;
		eth_rx_channel_params.uiIFEngine  = ind;
		eth_rx_channel_params.eEthIFType  = INFRA_NW_IF_TYPE;
		eth_rx_channel_params.uiIFNumber  = 0;
		eth_rx_channel_params.uiRXChannel  = 0;

		ez_ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetEthRXChannelParams, &eth_rx_channel_params);
		if (EZrc_IS_ERROR (ez_ret_val)) {
			return false;
		}

		eth_rx_channel_params.uiLogicalID = INFRA_NW_IF_BASE_LOGICAL_ID + ind;

		ez_ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetEthRXChannelParams, &eth_rx_channel_params);
		if (EZrc_IS_ERROR(ez_ret_val)) {
			return false;
		}
	}

	/* Configure interface to host */
	eth_if_params.uiSide = INFRA_HOST_IF_SIDE;
	eth_if_params.uiIFEngine = INFRA_HOST_IF_ENGINE;
	eth_if_params.eEthIFType = EZapiChannel_EthIFType_10GE;
	eth_if_params.uiIFNumber = INFRA_HOST_IF_NUMBER;

	ez_ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetEthIFParams, &eth_if_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	eth_if_params.bRXEnable = true;
	eth_if_params.bTXEnable = true;
	eth_if_params.bTXTMBypass = true;

	ez_ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetEthIFParams, &eth_if_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	eth_rx_channel_params.uiSide = INFRA_HOST_IF_SIDE;
	eth_rx_channel_params.uiIFEngine  = INFRA_HOST_IF_ENGINE;
	eth_rx_channel_params.eEthIFType  = EZapiChannel_EthIFType_10GE;
	eth_rx_channel_params.uiIFNumber  = INFRA_HOST_IF_NUMBER;
	eth_rx_channel_params.uiRXChannel  = 0;

	ez_ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetEthRXChannelParams, &eth_rx_channel_params);
	if (EZrc_IS_ERROR (ez_ret_val)) {
		return false;
	}

	eth_rx_channel_params.uiLogicalID = INFRA_HOST_IF_LOGICAL_ID;

	ez_ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetEthRXChannelParams, &eth_rx_channel_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	return true;
}

uint32_t get_imem_index(void)
{
	static uint32_t index = 0;
	return index++;
}

uint32_t get_emem_index(void)
{
	static uint32_t index = 0;
	return index++;
}

bool infra_create_mem_partition(void)
{
	EZstatus ez_ret_val;
	EZapiChannel_IntMemSpaceParams int_mem_space_params;
	EZapiChannel_ExtMemSpaceParams ext_mem_space_params;
	uint32_t ind;

	for (ind = 0; ind < NUM_OF_INT_MEMORY_SPACES; ind++)
	{
		if (imem_spaces_params[ind][INFRA_IMEM_SPACES_PARAMS_SIZE] > 0) {
			memset(&int_mem_space_params, 0, sizeof(int_mem_space_params));

			int_mem_space_params.uiIndex = get_imem_index();
			ez_ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetIntMemSpaceParams, &int_mem_space_params);
			if (EZrc_IS_ERROR(ez_ret_val)) {
				return false;
			}

			int_mem_space_params.bEnable = true;
			int_mem_space_params.eType = imem_spaces_params[ind][INFRA_IMEM_SPACES_PARAMS_TYPE];
			int_mem_space_params.uiSize = imem_spaces_params[ind][INFRA_IMEM_SPACES_PARAMS_SIZE];

			ez_ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetIntMemSpaceParams, &int_mem_space_params);
			if (EZrc_IS_ERROR(ez_ret_val)) {
				return false;
			}
		}
	}

	for (ind = 0; ind < NUM_OF_EXT_MEMORY_SPACES; ind++)
	{
		if (emem_spaces_params[ind][INFRA_EMEM_SPACES_PARAMS_SIZE] > 0) {
			memset(&ext_mem_space_params, 0, sizeof(ext_mem_space_params));

			ext_mem_space_params.uiIndex = get_emem_index();
			ez_ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetExtMemSpaceParams, &ext_mem_space_params);
			if (EZrc_IS_ERROR(ez_ret_val)) {
				return false;
			}

			ext_mem_space_params.bEnable = true;
			ext_mem_space_params.eType = emem_spaces_params[ind][INFRA_EMEM_SPACES_PARAMS_TYPE];
			ext_mem_space_params.uiSize = emem_spaces_params[ind][INFRA_EMEM_SPACES_PARAMS_SIZE];
			ext_mem_space_params.eECCType = emem_spaces_params[ind][INFRA_EMEM_SPACES_PARAMS_PROTECTION];
			ext_mem_space_params.uiMSID = ext_mem_space_params.uiIndex;

			ez_ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetExtMemSpaceParams, &ext_mem_space_params);
			if (EZrc_IS_ERROR(ez_ret_val)) {
				return false;
			}
		}
	}

	return true;
}

bool infra_configure_protocol_decode(void)
{
	EZstatus retVal;
	FILE* fd;
	EZapiChannel_ProtocolDecoderParams protocol_decoder_params;

	memset(&protocol_decoder_params, 0, sizeof(protocol_decoder_params));

	protocol_decoder_params.uiProfile = 0;

	retVal = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetProtocolDecoderParams, &protocol_decoder_params);
	if (EZrc_IS_ERROR(retVal)) {
		return false;
	}

	fd = fopen("/sys/class/net/eth0/address","r");
	if(fd == NULL) {
		return false;
	}
	fscanf(fd, "%2hhx%*c%2hhx%*c%2hhx%*c%2hhx%*c%2hhx%*c%2hhx",
	       &protocol_decoder_params.aucDestMACAddressLow[0],
	       &protocol_decoder_params.aucDestMACAddressLow[1],
	       &protocol_decoder_params.aucDestMACAddressLow[2],
	       &protocol_decoder_params.aucDestMACAddressLow[3],
	       &protocol_decoder_params.aucDestMACAddressLow[4],
	       &protocol_decoder_params.aucDestMACAddressLow[5]);
	fclose(fd);

	protocol_decoder_params.aucDestMACAddressHigh[0] = protocol_decoder_params.aucDestMACAddressLow[0];
	protocol_decoder_params.aucDestMACAddressHigh[1] = protocol_decoder_params.aucDestMACAddressLow[1];
	protocol_decoder_params.aucDestMACAddressHigh[2] = protocol_decoder_params.aucDestMACAddressLow[2];
	protocol_decoder_params.aucDestMACAddressHigh[3] = protocol_decoder_params.aucDestMACAddressLow[3];
	protocol_decoder_params.aucDestMACAddressHigh[4] = protocol_decoder_params.aucDestMACAddressLow[4];
	protocol_decoder_params.aucDestMACAddressHigh[5] = protocol_decoder_params.aucDestMACAddressLow[5];

	retVal = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetProtocolDecoderParams, &protocol_decoder_params);

	if (EZrc_IS_ERROR(retVal)) {
		return false;
	}

	return true;
}
