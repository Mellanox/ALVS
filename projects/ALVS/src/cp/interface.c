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

#include <stdint.h>
#include "interface.h"
#include <EZapiChannel.h>

#define HOST_IF_SIDE    1
#define HOST_IF_ENGINE  0
#define HOST_IF_NUMBER  0

#define NW_IF_SIDE      0

bool create_if_mapping(void)
{
	uint32_t ind;
	EZstatus ez_ret_val;
	EZapiChannel_EthIFParams eth_if_params;
	EZapiChannel_EthRXChannelParams eth_rx_channel_params;

	/* Configure external interfaces */
	for(ind = 0; ind < EXT_IF_NUM; ind++) {
		eth_if_params.uiSide = NW_IF_SIDE;
		eth_if_params.uiIFEngine = ind;
		eth_if_params.eEthIFType = EXT_IF_TYPE;
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


		eth_rx_channel_params.uiSide = NW_IF_SIDE;
		eth_rx_channel_params.uiIFEngine  = ind;
		eth_rx_channel_params.eEthIFType  = EXT_IF_TYPE;
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
	eth_if_params.uiSide = HOST_IF_SIDE;
	eth_if_params.uiIFEngine = HOST_IF_ENGINE;
	eth_if_params.eEthIFType = EZapiChannel_EthIFType_10GE;
	eth_if_params.uiIFNumber = HOST_IF_NUMBER;

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


	return true;
}
