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

#define INFRA_BASE_LOGICAL_ID 0

enum infra_search_mem_heaps {
	INFRA_HALF_CLUSTER_SEARCH_HEAP,
	INFRA_X1_CLUSTER_SEARCH_HEAP,
	INFRA_X2_CLUSTER_SEARCH_HEAP,
	INFRA_X4_CLUSTER_SEARCH_HEAP,
	INFRA_X16_CLUSTER_SEARCH_HEAP,
	INFRA_ALL_CLUSTER_SEARCH_HEAP,
	INFRA_EMEM_SEARCH_HEAP
};

struct infra_hash_params {
	uint32_t key_size;
	uint32_t result_size;
	uint32_t max_num_of_entries;
	bool updated_from_dp;
};

struct infra_table_params {
	uint32_t key_size;
	uint32_t result_size;
	uint32_t max_num_of_entries;
};

bool infra_created(void);
bool infra_powered_up(void);
bool infra_initialized(void);
bool infra_finalized(void);
bool infra_running(void);

bool infra_enable_agt(void);
void infra_disable_agt(void);

bool infra_get_my_mac(struct ether_addr *my_mac);

bool infra_create_hash(uint32_t struct_id,
		       enum infra_search_mem_heaps search_mem_heap,
		       struct infra_hash_params *params);
bool infra_create_table(uint32_t struct_id,
			enum infra_search_mem_heaps search_mem_heap,
			struct infra_table_params *params);
bool infra_add_entry(uint32_t struct_id, void *key, uint32_t key_size,
		     void *result, uint32_t result_size);
bool infra_delete_entry(uint32_t struct_id, void *key, uint32_t key_size);

#endif /* _INFRASTRUCTURE_H_ */
