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
