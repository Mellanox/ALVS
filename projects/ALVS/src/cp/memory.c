/***************************************************************************
*
*                    Copyright by EZchip Technologies, 2016
*
*
*  Project:          ALVS
*  File:             memory.c
*  Description:      Memory partition implementation.
*  Notes:
*
*
****************************************************************************/

#include "memory.h"
#include "search.h"
#include <string.h>

uint32_t alvs_get_imem_index(void)
{
	static uint32_t index = 0;
	return index++;
}

bool alvs_create_mem_partition(void)
{
	EZstatus ez_ret_val;
	EZapiChannel_IntMemSpaceParams int_mem_space_params;
	uint32_t ind;

	for(ind = 0; ind < ALVS_NUM_OF_MSIDS; ind++)
	{
		if (alvs_imem_sizes[ind][1] > 0) {
			memset(&int_mem_space_params, 0, sizeof(int_mem_space_params));

			int_mem_space_params.uiIndex = alvs_get_imem_index();
			ez_ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetIntMemSpaceParams, &int_mem_space_params);
			if (EZrc_IS_ERROR(ez_ret_val)) {
				return false;
			}

			int_mem_space_params.bEnable = true;
			int_mem_space_params.eType = alvs_imem_sizes[ind][0];
			int_mem_space_params.uiSize = alvs_imem_sizes[ind][1];

			ez_ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetIntMemSpaceParams, &int_mem_space_params);
			if (EZrc_IS_ERROR(ez_ret_val)) {
				return false;
			}
		}
	}

	alvs_create_search_mem_partition();

	return true;
}
