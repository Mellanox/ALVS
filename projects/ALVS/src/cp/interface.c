/***************************************************************************
*
*                    Copyright by EZchip Technologies, 2016
*
*
*  Project:          ALVS
*  File:             search.c
*  Description:      Search structures implementation.
*  Notes:
*
*
****************************************************************************/

#include <stdint.h>
#include "interface.h"
#include <EZapiChannel.h>

#define ALVS_HOST_IF_SIDE    1
#define ALVS_HOST_IF_ENGINE  0
#define ALVS_HOST_IF_NUMBER  0

#define ALVS_NW_IF_SIDE      0

bool alvs_create_if_mapping(void)
{
	uint32_t ind;
	EZstatus ez_ret_val;
	EZapiChannel_EthIFParams eth_if_params;
	EZapiChannel_EthRXChannelParams eth_rx_channel_params;
	EZapiChannel_PMUQueueParams pmu_queue_params;

	/* Configure external interfaces */
	for(ind = 0; ind < ALVS_EXT_IF_NUM; ind++) {
		eth_if_params.uiSide = ALVS_NW_IF_SIDE;
		eth_if_params.uiIFEngine = ind;
		eth_if_params.eEthIFType = ALVS_EXT_IF_TYPE;
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


		eth_rx_channel_params.uiSide = ALVS_NW_IF_SIDE;
		eth_rx_channel_params.uiIFEngine  = ind;
		eth_rx_channel_params.eEthIFType  = ALVS_EXT_IF_TYPE;
		eth_rx_channel_params.uiIFNumber  = 0;
		eth_rx_channel_params.uiRXChannel = 0;

		ez_ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetEthRXChannelParams, &eth_rx_channel_params);
		if (EZrc_IS_ERROR (ez_ret_val)) {
			return false;
		}

		eth_rx_channel_params.uiLogicalID = ind;

		ez_ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetEthRXChannelParams, &eth_rx_channel_params);
		if (EZrc_IS_ERROR(ez_ret_val)) {
			return false;
		}
	}

	/* Configure interface to host */
	eth_if_params.uiSide = ALVS_HOST_IF_SIDE;
	eth_if_params.uiIFEngine = ALVS_HOST_IF_ENGINE;
	eth_if_params.eEthIFType = EZapiChannel_EthIFType_10GE;
	eth_if_params.uiIFNumber = ALVS_HOST_IF_NUMBER;

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


	/* Configure PMU queues */
	// Do nothing - Keep defaults
}
