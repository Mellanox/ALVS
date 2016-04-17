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

#include "search.h"
#include <string.h>
#include <EZapiStruct.h>

#define ALVS_MAX_ENTRIES     65536

bool alvs_create_classification_db(void)
{
	EZstatus ez_ret_val;
	EZapiStruct_StructParams struct_params;
	EZapiStruct_HashMemMngParams hash_mem_mng_params;

	/* Configure the struct parameters */
	memset(&struct_params, 0, sizeof(struct_params));

	ez_ret_val = EZapiStruct_Status(ALVS_STRUCT_ID_SERVICES, EZapiStruct_StatCmd_GetStructParams, &struct_params);
	if (EZrc_IS_ERROR (ez_ret_val)) {
		return false;
	}

	struct_params.bEnable = true;
	struct_params.eStructType = EZapiStruct_StructType_HASH;
	struct_params.eStructMemoryArea = EZapiStruct_MemoryArea_EXTERNAL;
	struct_params.uiKeySize         = sizeof(struct alvs_service_key);
	struct_params.uiResultSize      = sizeof(struct alvs_service_result);
	struct_params.uiMaxEntries      = ALVS_MAX_ENTRIES;

	ez_ret_val = EZapiStruct_Config(ALVS_STRUCT_ID_SERVICES, EZapiStruct_ConfigCmd_SetStructParams, &struct_params);
	if (EZrc_IS_ERROR (ez_ret_val)) {
		return false;
	}


	/* Keep hash params with default values */


	/* Configure the HASH memory management parameters */
	memset(&hash_mem_mng_params, 0, sizeof( hash_mem_mng_params));

	ez_ret_val = EZapiStruct_Status(ALVS_STRUCT_ID_SERVICES, EZapiStruct_StatCmd_GetHashMemMngParams, &hash_mem_mng_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	hash_mem_mng_params.uiMainTableSpaceIndex = 0;
	hash_mem_mng_params.uiSigSpaceIndex = 0;
	hash_mem_mng_params.uiResSpaceIndex = 0;

	ez_ret_val = EZapiStruct_Config(ALVS_STRUCT_ID_SERVICES, EZapiStruct_ConfigCmd_SetHashMemMngParams, &hash_mem_mng_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	return true;
}

bool alvs_create_arp_db(void)
{
	EZstatus ez_ret_val;
	EZapiStruct_StructParams struct_params;
	EZapiStruct_HashMemMngParams hash_mem_mng_params;

	/* Configure the struct parameters */
	memset(&struct_params, 0, sizeof(struct_params));

	ez_ret_val = EZapiStruct_Status(ALVS_STRUCT_ID_ARP, EZapiStruct_StatCmd_GetStructParams, &struct_params);
	if (EZrc_IS_ERROR (ez_ret_val)) {
		return false;
	}

	struct_params.bEnable = true;
	struct_params.eStructType = EZapiStruct_StructType_HASH;
	struct_params.eStructMemoryArea = EZapiStruct_MemoryArea_EXTERNAL;
	struct_params.uiKeySize         = sizeof(struct alvs_arp_key);
	struct_params.uiResultSize      = sizeof(struct alvs_arp_result);
	struct_params.uiMaxEntries      = ALVS_MAX_ENTRIES;

	ez_ret_val = EZapiStruct_Config(ALVS_STRUCT_ID_ARP, EZapiStruct_ConfigCmd_SetStructParams, &struct_params);
	if (EZrc_IS_ERROR (ez_ret_val)) {
		return false;
	}


	/* Keep hash params with default values */


	/* Configure the HASH memory management parameters */
	memset(&hash_mem_mng_params, 0, sizeof( hash_mem_mng_params));

	ez_ret_val = EZapiStruct_Status(ALVS_STRUCT_ID_ARP, EZapiStruct_StatCmd_GetHashMemMngParams, &hash_mem_mng_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	hash_mem_mng_params.uiMainTableSpaceIndex = 0;
	hash_mem_mng_params.uiSigSpaceIndex = 0;
	hash_mem_mng_params.uiResSpaceIndex = 0;

	ez_ret_val = EZapiStruct_Config(ALVS_STRUCT_ID_ARP, EZapiStruct_ConfigCmd_SetHashMemMngParams, &hash_mem_mng_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	return true;
}

bool alvs_create_all_dbs(void)
{
	if(alvs_create_classification_db() == false) {
		return false;
	}
	if(alvs_create_arp_db() == false) {
		return false;
	}

	return true;
}

bool alvs_load_partition()
{
	EZstatus ez_ret_val;

	ez_ret_val = EZapiStruct_PartitionConfig(0, EZapiStruct_PartitionConfigCmd_LoadPartition, NULL);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	return true;
}

bool alvs_add_classification_entry(struct alvs_service_key *key, struct alvs_service_result *result)
{
	EZstatus ez_ret_val;
	EZapiEntry entry;

	memset(&entry, 0, sizeof(entry));

	entry.uiKeySize = sizeof(struct alvs_service_key);
	entry.pucKey = (void*)key;
	entry.uiResultSize = sizeof(struct alvs_service_result);
	entry.pucResult = (void*)result;

	ez_ret_val = EZapiStruct_AddEntry(ALVS_STRUCT_ID_SERVICES, NULL, &entry, NULL);

	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	return true;
}

bool alvs_add_arp_entry(struct alvs_arp_key *key, struct alvs_arp_result *result)
{
	EZstatus ez_ret_val;
	EZapiEntry entry;

	memset(&entry, 0, sizeof(entry));

	entry.uiKeySize = sizeof(struct alvs_arp_key);
	entry.pucKey = (void*)key;
	entry.uiResultSize = sizeof(struct alvs_arp_result);
	entry.pucResult = (void*)result;

	ez_ret_val = EZapiStruct_AddEntry(ALVS_STRUCT_ID_ARP, NULL, &entry, NULL);

	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	return true;
}

bool alvs_delete_classification_entry(struct alvs_service_key *key)
{
	EZstatus ez_ret_val;
	EZapiEntry entry;

	memset(&entry, 0, sizeof(entry));

	entry.uiKeySize = sizeof(struct alvs_arp_key);
	entry.pucKey = (void*)key;

	ez_ret_val = EZapiStruct_DeleteEntry(ALVS_STRUCT_ID_SERVICES, NULL, &entry, NULL);

	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	return true;
}

bool alvs_delete_arp_entry(struct alvs_arp_key *key)
{
	EZstatus ez_ret_val;
	EZapiEntry entry;

	memset(&entry, 0, sizeof(entry));

	entry.uiKeySize = sizeof(struct alvs_arp_key);
	entry.pucKey = (void*)key;

	ez_ret_val = EZapiStruct_DeleteEntry(ALVS_STRUCT_ID_ARP, NULL, &entry, NULL);

	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	return true;
}

bool alvs_create_search_mem_partition(void)
{
	EZstatus ez_ret_val;
	EZapiChannel_ExtMemSpaceParams ext_mem_space_params;

	/* configure msid0 in EMEM for classification & ARP DBs */
	memset(&ext_mem_space_params, 0, sizeof(ext_mem_space_params));

	ext_mem_space_params.uiIndex = 0;
	ez_ret_val = EZapiChannel_Status(0, EZapiChannel_StatCmd_GetExtMemSpaceParams, &ext_mem_space_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
			return false;
	}

	ext_mem_space_params.bEnable = true;
	ext_mem_space_params.eType = EZapiChannel_ExtMemSpaceType_SEARCH;
	ext_mem_space_params.uiSize = 256;
	ext_mem_space_params.uiMSID = 0;

	ez_ret_val = EZapiChannel_Config(0, EZapiChannel_ConfigCmd_SetExtMemSpaceParams, &ext_mem_space_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
			return false;
	}

	return true;
}
