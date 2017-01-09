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
*  File:                infrastructure_utils.c
*  Desc:                Implementation of infrastructure utils API.
*
*/

#include <EZapiPrm.h>
#include <EZapiStat.h>
#include <EZapiStruct.h>
#include <EZapiTCAM.h>

#include "log.h"
#include "infrastructure.h"
#include "infrastructure_utils.h"


/**************************************************************************//**
 * \brief       Create hash data structure
 *
 * \param[in]   struct_id       - structure id of the hash
 * \param[in]   params          - parameters of the hash (size of key & result,
 *                                max number of entries and update mode)
 *
 * \return      bool - success or failure
 */
bool infra_create_hash(uint32_t struct_id, struct infra_hash_params *params)
{
	EZstatus ez_ret_val;
	EZapiStruct_StructParams struct_params;
	EZapiStruct_HashParams hash_params;
	EZapiStruct_HashMemMngParams hash_mem_mng_params;

	/* Get defaults of the structure */
	memset(&struct_params, 0, sizeof(struct_params));

	ez_ret_val = EZapiStruct_Status(struct_id, EZapiStruct_StatCmd_GetStructParams, &struct_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "infra_create_hash: EZapiStruct_Status(GetStructParams) failed, struct ID %d (RC=%08x)", struct_id, ez_ret_val);
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

	if (params->is_external == true) {
		struct_params.eStructMemoryArea = EZapiStruct_MemoryArea_EXTERNAL;
	} else {
		struct_params.eStructMemoryArea = EZapiStruct_MemoryArea_INTERNAL;
	}

	ez_ret_val = EZapiStruct_Config(struct_id, EZapiStruct_ConfigCmd_SetStructParams, &struct_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "infra_create_hash: EZapiStruct_Config(SetStructParams) failed, struct ID %d (RC=%08x)", struct_id, ez_ret_val);
		return false;
	}


	/* Get defaults of the hash parameters */
	memset(&hash_params, 0, sizeof(hash_params));

	ez_ret_val = EZapiStruct_Status(struct_id, EZapiStruct_StatCmd_GetHashParams, &hash_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "infra_create_hash: EZapiStruct_Status(GetHashParams) failed, struct ID %d (RC=%08x)", struct_id, ez_ret_val);
		return false;
	}

	/* set single cycle and update mode */
	hash_params.bSingleCycle = true;
	if (params->updated_from_dp == true) {
		hash_params.eUpdateMode = EZapiStruct_UpdateMode_DP;
		hash_params.eMultiChannelDataMode = EZapiStruct_MultiChannelDataMode_DIFFERENT;
	} else {
		hash_params.eUpdateMode = EZapiStruct_UpdateMode_CP;
		hash_params.eCacheMode = EZapiStruct_CacheMode_FULL;
		hash_params.eMultiChannelDataMode = EZapiStruct_MultiChannelDataMode_IDENTICAL;
	}

	ez_ret_val = EZapiStruct_Config(struct_id, EZapiStruct_ConfigCmd_SetHashParams, &hash_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "infra_create_hash: EZapiStruct_Config(SetHashParams) failed, struct ID %d (RC=%08x)", struct_id, ez_ret_val);
		return false;
	}


	/* Get defaults of the hash memory management */
	memset(&hash_mem_mng_params, 0, sizeof(hash_mem_mng_params));

	ez_ret_val = EZapiStruct_Status(struct_id, EZapiStruct_StatCmd_GetHashMemMngParams, &hash_mem_mng_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "infra_create_hash: EZapiStruct_Config(GetHashMemMngParams) failed, struct ID %d (RC=%08x)", struct_id, ez_ret_val);
		return false;
	}

	/* Set index of the memory heap */
	if (params->hash_size > 0) {
		hash_mem_mng_params.uiHashSize = params->hash_size;
	}

	if (params->updated_from_dp == true) {
		hash_mem_mng_params.uiResultIndexPool = params->result_pool_id;
		hash_mem_mng_params.uiSigIndexPool = params->sig_pool_id;
	}

	hash_mem_mng_params.uiMainTableSpaceIndex = infra_index_of_heap(params->main_table_search_mem_heap, params->is_external);
	hash_mem_mng_params.uiSigSpaceIndex = infra_index_of_heap(params->sig_table_search_mem_heap, params->is_external);
	hash_mem_mng_params.uiResSpaceIndex = infra_index_of_heap(params->res_table_search_mem_heap, params->is_external);

	ez_ret_val = EZapiStruct_Config(struct_id, EZapiStruct_ConfigCmd_SetHashMemMngParams, &hash_mem_mng_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "infra_create_hash: EZapiStruct_Config(SetHashMemMngParams) failed, struct ID %d (RC=%08x)", struct_id, ez_ret_val);
		return false;
	}

	return true;
}

/**************************************************************************//**
 * \brief       Create TCAM data structure
 *
 * \param[in]   params          - parameters of the tcam
 *
 * \return      bool - success or failure
 */
bool infra_create_tcam(struct infra_tcam_params *params)
{
	EZstatus retVal = EZok;
	EZapiTCAM_IntTCAMLookupTable sIntTCAMLookupTable;
	EZapiTCAM_IntTCAMLookupProfile sIntTCAMLookupProfile;
	unsigned int i;

	/* Configure table */
	memset(&sIntTCAMLookupTable, 0, sizeof(sIntTCAMLookupTable));
	sIntTCAMLookupTable.uiSide = params->side;
	sIntTCAMLookupTable.uiTable = params->table;
	retVal = EZapiTCAM_Status(0, EZapiTCAM_StatCmd_GetIntTCAMLookupTable, &sIntTCAMLookupTable);
	if (EZrc_IS_ERROR(retVal)) {
		write_log(LOG_CRIT, "ZapiTCAM_Status: EZapiTCAM_StatCmd_GetIntTCAMLookupTable failed, channel Id %u", 0);
		return false;
	}

	/*
	 * There is a CP bug in which we cannot configure an internal TCAM table with params->max_num_of_entries
	 * the following is a workaround that configures 8K entries. (maximal table size for key size = 10Byte)
	 * This workaround allows us to configure only one internal TCAM structure per side.
	 */
	sIntTCAMLookupTable.uiNumIndexes = 0xFFFFFFFF;
	sIntTCAMLookupTable.uiStartBank = 0;
	sIntTCAMLookupTable.uiNumBanks = 4;
	sIntTCAMLookupTable.uiStartRow = 0;
	sIntTCAMLookupTable.uiNumRows = 2048;
	/* End of workaround */

	sIntTCAMLookupTable.bAssociatedData = TRUE;
	sIntTCAMLookupTable.uiAssociatedDataSize = params->result_size;
	sIntTCAMLookupTable.uiKeySize = params->key_size;
	retVal = EZapiTCAM_Config(0, EZapiTCAM_ConfigCmd_SetIntTCAMLookupTable, &sIntTCAMLookupTable);
	if (EZrc_IS_ERROR(retVal)) {
		write_log(LOG_CRIT, "ZapiTCAM_Status: EZapiTCAM_ConfigCmd_SetIntTCAMLookupTable failed, channel Id %u", 0);
		return false;
	}

	/* Configure profile for table */
	memset(&sIntTCAMLookupProfile, 0, sizeof(sIntTCAMLookupProfile));
	sIntTCAMLookupProfile.uiSide = params->side;
	sIntTCAMLookupProfile.uiProfile = params->profile;
	retVal = EZapiTCAM_Status(0, EZapiTCAM_StatCmd_GetIntTCAMLookupProfile, &sIntTCAMLookupProfile);
	if (EZrc_IS_ERROR(retVal)) {
		write_log(LOG_CRIT, "ZapiTCAM_Status: EZapiTCAM_StatCmd_GetIntTCAMLookupProfile failed, channel Id %u", 0);
		return false;
	}

	for (i = 0; i < params->lookup_table_count; i++) {
		sIntTCAMLookupProfile.auiLookupTable[i] = params->profile;
	}
	sIntTCAMLookupProfile.bSequentialLookup = TRUE;
	retVal = EZapiTCAM_Config(0, EZapiTCAM_ConfigCmd_SetIntTCAMLookupProfile, &sIntTCAMLookupProfile);
	if (EZrc_IS_ERROR(retVal)) {
		write_log(LOG_CRIT, "ZapiTCAM_Status: EZapiTCAM_ConfigCmd_SetIntTCAMLookupProfile failed, channel Id %u", 0);
		return false;
	}

	return true;
}

/**************************************************************************//**
 * \brief       Create table data structure
 *
 * \param[in]   struct_id       - structure id of the table
 * \param[in]   params          - parameters of the table (size of key & result
 *                                and max number of entries)
 *
 * \return      bool - success or failure
 */
bool infra_create_table(uint32_t struct_id, struct infra_table_params *params)
{
	EZstatus ez_ret_val;
	EZapiStruct_StructParams struct_params;
	EZapiStruct_TableParams table_params;
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
	if (params->is_external == true) {
		struct_params.eStructMemoryArea = EZapiStruct_MemoryArea_EXTERNAL;
	} else {
		struct_params.eStructMemoryArea = EZapiStruct_MemoryArea_INTERNAL;
	}

	ez_ret_val = EZapiStruct_Config(struct_id, EZapiStruct_ConfigCmd_SetStructParams, &struct_params);

	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}


	/* Get defaults of the table parameters */
	memset(&table_params, 0, sizeof(table_params));

	ez_ret_val = EZapiStruct_Status(struct_id, EZapiStruct_StatCmd_GetTableParams, &table_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	/* set single cycle and update mode */
	if (params->updated_from_dp == true) {
		table_params.eUpdateMode = EZapiStruct_UpdateMode_DP;
		table_params.eMultiChannelDataMode = EZapiStruct_MultiChannelDataMode_DIFFERENT;
	} else {
		table_params.eUpdateMode = EZapiStruct_UpdateMode_CP;
		table_params.eCacheMode = EZapiStruct_CacheMode_FULL;
		table_params.eMultiChannelDataMode = EZapiStruct_MultiChannelDataMode_IDENTICAL;
	}

	ez_ret_val = EZapiStruct_Config(struct_id, EZapiStruct_ConfigCmd_SetTableParams, &table_params);
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
	table_mem_mng_params.uiSpaceIndex = infra_index_of_heap(params->search_mem_heap, params->is_external);

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
/**************************************************************************//**
 * \brief       Add an entry to a TCAM data structure
 *
 * \param[in]   side            - side of TCAM table (0/1)
 * \param[in]   table           - table number
 * \param[in]   key             - reference to key
 * \param[in]   key_size        - size of the key in bytes
 * \param[in]   mask            - reference to mask
 * \param[in]   index           - index in table
 * \param[in]   result          - reference to result
 * \param[in]   result_size     - size of the result in bytes
 *
 * \return      bool - success or failure
 */
bool infra_add_tcam_entry(uint32_t side, uint32_t table, void *key, uint32_t key_size,
			  void *mask, uint32_t index, void *result, uint32_t result_size)
{
	EZstatus retVal = EZok;
	EZapiTCAM_IntTCAMDataParams sIntTCAMDataParams;

	memset(&sIntTCAMDataParams, 0, sizeof(sIntTCAMDataParams));
	sIntTCAMDataParams.uiSide = side;
	sIntTCAMDataParams.uiTable = table;
	sIntTCAMDataParams.uiIndex = index;
	sIntTCAMDataParams.bValid = true;
	sIntTCAMDataParams.uiKeySize = key_size;
	sIntTCAMDataParams.pucKey = key;
	sIntTCAMDataParams.pucMask = mask;
	sIntTCAMDataParams.uiAssociatedDataSize = result_size;
	sIntTCAMDataParams.pucAssociatedData = result;

	retVal = EZapiTCAM_Config(0, EZapiTCAM_ConfigCmd_WriteIntTCAMData, &sIntTCAMDataParams);

	if (EZrc_IS_ERROR(retVal)) {
		return false;
	}

	return true;
}

/**************************************************************************//**
 * \brief       Delete an entry from a TCAM data structure
 *
 * \param[in]   side            - side of TCAM table (0/1)
 * \param[in]   table           - table number
 * \param[in]   key             - reference to key
 * \param[in]   key_size        - size of the key in bytes
 * \param[in]   index           - index in table
 *
 * \return      bool - success or failure
 */
bool infra_delete_tcam_entry(uint32_t side, uint32_t table, void *key, uint32_t key_size, uint32_t index)
{
	EZstatus retVal = EZok;
	EZapiTCAM_IntTCAMDataParams sIntTCAMDataParams;

	memset(&sIntTCAMDataParams, 0, sizeof(sIntTCAMDataParams));
	sIntTCAMDataParams.uiSide = side;
	sIntTCAMDataParams.uiTable = table;
	sIntTCAMDataParams.uiIndex = index;
	sIntTCAMDataParams.bValid = false;
	sIntTCAMDataParams.uiKeySize = key_size;
	sIntTCAMDataParams.pucKey = key;  /* Will be ignored, but can't be NULL */
	sIntTCAMDataParams.pucMask = key;  /* Will be ignored, but can't be NULL  */
	sIntTCAMDataParams.uiAssociatedDataSize = 0;

	retVal = EZapiTCAM_Config(0, EZapiTCAM_ConfigCmd_WriteIntTCAMData, &sIntTCAMDataParams);
	if (EZrc_IS_ERROR(retVal)) {
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
 * \brief       Read Long Counters Values
 * \param[in]   counter_index   - index of starting counter
 *		num_of_counters - number of counters from the starting counter
 *		[out] counters_value - pointer to the array of results (array of uint64 size must be num_of_couinters)
 * \return      bool
 *
 */
bool infra_get_long_counters(uint32_t counter_index,
			     uint32_t num_of_counters,
			     uint64_t *counters_value)
{
	union {
		uint64_t raw_data;
		struct {
			uint32_t value_lsb;
			uint32_t value_msb;
		};
	} value;

	uint32_t i;
	EZstatus ret_val;
	EZapiStat_LongCounter *long_counter;
	EZapiStat_LongCounterConfig long_counter_config;

	long_counter = (EZapiStat_LongCounter *)malloc(sizeof(EZapiStat_LongCounter) * num_of_counters);
	if (long_counter == NULL) {
		write_log(LOG_ERR, "Failed to allocate memory for EZapiStat_PostedCounter");
		return false;
	}
	memset(long_counter, 0, sizeof(EZapiStat_LongCounter) * num_of_counters);
	memset(&long_counter_config, 0, sizeof(long_counter_config));

	long_counter_config.pasCounters = long_counter;
	long_counter_config.uiStartCounter = counter_index;
	long_counter_config.uiNumCounters = num_of_counters;
	long_counter_config.bRange = TRUE;
	ret_val = EZapiStat_Status(0, EZapiStat_StatCmd_GetLongCounters, &long_counter_config);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Status: EZapiStat_StatCmd_GetLongCounters failed.");
		free(long_counter);
		return false;
	}

	for (i = 0; i < num_of_counters; i++) {
		value.value_lsb = long_counter[i].uiValue;
		value.value_msb = long_counter[i].uiValueMSB;
		counters_value[i] = value.raw_data;
	}

	free(long_counter);
	return true;
}

/**************************************************************************//**
 * \brief       Get posted counters value (read several counters num_of_counters)
 *
 * \param[in]   counter_index   - index of starting counter
 *		num_of_counters - number of counters from the starting counter
 *		[out] counters_value - pointer to the array of results (array of uint64 size must be num_of_couinters)
 * \return      bool
 */
bool infra_get_posted_counters(uint32_t counter_index,
			       uint32_t num_of_counters,
			       uint64_t *counters_value)
{
	union {
		uint64_t raw_data;
		struct {
			uint32_t value_lsb;
			uint32_t value_msb;
		};
	} value;

	uint32_t i;
	EZstatus ret_val;
	EZapiStat_PostedCounter *posted_counter;
	EZapiStat_PostedCounterConfig posted_counter_config;

	posted_counter = (EZapiStat_PostedCounter *)malloc(sizeof(EZapiStat_PostedCounter) * num_of_counters);
	if (posted_counter == NULL) {
		write_log(LOG_ERR, "Failed to allocate memory for EZapiStat_PostedCounter");
		return false;
	}
	memset(posted_counter, 0, sizeof(EZapiStat_PostedCounter) * num_of_counters);
	memset(&posted_counter_config, 0, sizeof(posted_counter_config));

	posted_counter_config.uiPartition = 0;
	posted_counter_config.pasCounters = posted_counter;
	posted_counter_config.bRange = true;
	posted_counter_config.uiStartCounter = counter_index;
	posted_counter_config.uiNumCounters = num_of_counters;
	posted_counter_config.bDoubleOperation = false;
	ret_val = EZapiStat_Status(0, EZapiStat_StatCmd_GetPostedCounters,
					&posted_counter_config);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Config: EZapiStat_StatCmd_GetPostedCounters failed.");
		free(posted_counter);
		return false;
	}

	for (i = 0; i < num_of_counters; i++) {
		value.value_lsb = posted_counter[i].uiByteValue;
		value.value_msb = posted_counter[i].uiByteValueMSB;
		counters_value[i] = value.raw_data;
	}

	free(posted_counter);
	return true;
}

/**************************************************************************//**
 * \brief       Set posted counters values - set to a several counters (num_of_counters)
 *
 * \param[in]   counter_index   - index of starting counter
 *		num_of_counters - number of counters from the starting counter
 * \return      bool
 */
bool infra_clear_posted_counters(uint32_t counter_index,
				 uint32_t num_of_counters)
{
	EZstatus ret_val;
	EZapiStat_PostedCounter posted_counter;
	EZapiStat_PostedCounterConfig posted_counter_config;

	memset(&posted_counter_config, 0, sizeof(posted_counter_config));
	memset(&posted_counter, 0, sizeof(posted_counter));

	posted_counter_config.uiPartition = 0;
	posted_counter_config.pasCounters = &posted_counter;
	posted_counter_config.bRange = true;
	posted_counter_config.uiStartCounter = counter_index;
	posted_counter_config.uiNumCounters = num_of_counters;
	ret_val = EZapiStat_Config(0, EZapiStat_ConfigCmd_SetPostedCounters, &posted_counter_config);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Config: EZapiStat_ConfigCmd_SetPostedCounters failed.");
		return false;
	}

	return true;
}

/**************************************************************************//**
 * \brief       Set long counters values - set to a several counters (num_of_counters)
 *
 * \param[in]   counter_index   - index of starting counter
 *		num_of_counters - number of counters from the starting counter
 * \return      bool
 */
bool infra_clear_long_counters(uint32_t counter_index,
			       uint32_t num_of_counters)
{
	EZstatus ret_val;
	EZapiStat_LongCounterConfig long_counter_config;
	EZapiStat_LongCounter long_counter;

	memset(&long_counter_config, 0, sizeof(long_counter_config));
	memset(&long_counter, 0, sizeof(long_counter));

	long_counter_config.uiPartition = 0;
	long_counter_config.bRange = true;
	long_counter_config.uiStartCounter = counter_index;
	long_counter_config.uiNumCounters = num_of_counters;
	long_counter_config.pasCounters = &long_counter;
	ret_val = EZapiStat_Config(0, EZapiStat_ConfigCmd_SetLongCounterValues, &long_counter_config);

	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Config: EZapiStat_ConfigCmd_SetLongCounters failed.");
		return false;
	}

	return true;
}


/**************************************************************************//**
 * \brief       Copy data to memory (IMEM/EMEM)
 *
 * \param[in]   addr         - extended address of the memory to copy data to
 *		data         - the data to copy to memory
 *		data_size    - size of the data (in bytes)
 *
 * \return      bool
 */
bool infra_set_memory(struct ezdp_ext_addr *addr,
		      void *data,
		      uint32_t data_size)
{
	EZstatus ret_val;

	ret_val =  EZapiPrm_WriteMem(0, /* uiChannelId */
				     (addr->mem_type == EZDP_EXTERNAL_MS ? EZapiPrm_MemId_EXT_MEM : EZapiPrm_MemId_INT_MEM), /* eMemId */
				     addr->msid, /* uiMemSpaceIndex */
				     addr->address, /* uiLSBAddress */
				     addr->address_msb, /* uiMSBAddress */
				     0, /* bRange */
				     0, /* uiRangeSize */
				     0, /* uiRangeStep */
				     0, /* bSingleCopy */
				     0, /* bGCICopy */
				     0, /* uiCopyIndex */
				     data_size,
				     data,   /* pucData */
				     0 /* pSpecialParams */);

	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "infra_set_memory: EZapiPrm_WriteMem failed.");
		return false;
	}

	return true;
}
