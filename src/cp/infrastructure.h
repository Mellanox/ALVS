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
*  File:                infrastructure.h
*  Desc:                Infrastructure API.
*
*/

#ifndef _INFRASTRUCTURE_H_
#define _INFRASTRUCTURE_H_

#include <stdbool.h>
#include <stdint.h>
#include <net/ethernet.h>

/*! Search memory heaps possible values. */
enum infra_search_mem_heaps {
	INFRA_X1_CLUSTER_SEARCH_HEAP,
	INFRA_X4_CLUSTER_SEARCH_HEAP,
	INFRA_EMEM_SEARCH_HASH_HEAP,
	INFRA_EMEM_SEARCH_1_TABLE_HEAP,
	INFRA_EMEM_SEARCH_2_TABLE_HEAP,
	INFRA_NOT_VALID_HEAP
};

/*! Required parameters for hash creation data structure  */
struct infra_hash_params {
	uint32_t key_size;
	uint32_t result_size;
	uint32_t max_num_of_entries;
	uint32_t hash_size;
	bool updated_from_dp;
	uint32_t sig_pool_id;
	uint32_t result_pool_id;
	enum infra_search_mem_heaps main_table_search_mem_heap;
	enum infra_search_mem_heaps sig_table_search_mem_heap;
	enum infra_search_mem_heaps res_table_search_mem_heap;

};

/*! Required parameters for table creation data structure  */
struct infra_table_params {
	uint32_t key_size;
	uint32_t result_size;
	uint32_t max_num_of_entries;
	bool updated_from_dp;
	enum infra_search_mem_heaps search_mem_heap;
};

/**************************************************************************//**
 * \brief       Infrastructure configuration at created state
 *
 * \return      bool - success or failure
 */
bool infra_created(void);

/**************************************************************************//**
 * \brief       Infrastructure configuration at powered-up state
 *
 * \return      bool - success or failure
 */
bool infra_powered_up(void);

/**************************************************************************//**
 * \brief       Infrastructure configuration at initialized state
 *
 * \return      bool - success or failure
 */
bool infra_initialized(void);

/**************************************************************************//**
 * \brief       Infrastructure configuration at finalized state
 *
 * \return      bool - success or failure
 */
bool infra_finalized(void);

/**************************************************************************//**
 * \brief       Infrastructure configuration at running state
 *
 * \return      bool - success or failure
 */
bool infra_running(void);

/**************************************************************************//**
 * \brief       Enable agent debug interface
 *
 * \return      bool - success or failure
 */
bool infra_enable_agt(void);

/**************************************************************************//**
 * \brief       Disable agent debug interface
 *
 */
void infra_disable_agt(void);

/**************************************************************************//**
 * \brief       Get my MAC
 *
 * \param[out]  my_mac - reference to ethernet address type
 *
 * \return      true - success
 *              false - can't find tap interface file
 */
bool infra_get_my_mac(struct ether_addr *my_mac);

/**************************************************************************//**
 * \brief       Create hash data structure
 *
 * \param[in]   struct_id       - structure id of the hash
 * \param[in]   params          - parameters of the hash (size of key & result,
 *                                max number of entries and update mode)
 *
 * \return      bool - success or failure
 */
bool infra_create_hash(uint32_t struct_id,
		       struct infra_hash_params *params);

/**************************************************************************//**
 * \brief       Create table data structure
 *
 * \param[in]   struct_id       - structure id of the table
 * \param[in]   params          - parameters of the table (size of key & result
 *                                and max number of entries)
 *
 * \return      bool - success or failure
 */
bool infra_create_table(uint32_t struct_id,
			struct infra_table_params *params);

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
bool infra_add_entry(uint32_t struct_id, void *key, uint32_t key_size,
		     void *result, uint32_t result_size);

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
bool infra_modify_entry(uint32_t struct_id, void *key, uint32_t key_size,
			void *result, uint32_t result_size);

/**************************************************************************//**
 * \brief       Delete an entry from a data structure
 *
 * \param[in]   struct_id       - structure id of the search structure
 * \param[in]   key             - reference to key
 * \param[in]   key_size        - size of the key in bytes
 *
 * \return      bool - success or failure
 */
bool infra_delete_entry(uint32_t struct_id, void *key, uint32_t key_size);

/**************************************************************************//**
 * \brief       Delete all entries from a data structure
 *
 * \param[in]   struct_id       - structure id of the search structure
 *
 * \return      bool - success or failure
 */
bool infra_delete_all_entries(uint32_t struct_id);

/**************************************************************************//**
 * \brief       translate msid to msid_select
 *
 * \param[in]   external_memory       - bool if this msid external
 *		Note - now this function is only for external memory
 *		emem_msid	- emem_msid - msid for translation
 * \return      msid_select
 */
uint32_t infra_from_msid_to_index(bool external_memory, uint32_t emem_msid);

#endif /* _INFRASTRUCTURE_H_ */
