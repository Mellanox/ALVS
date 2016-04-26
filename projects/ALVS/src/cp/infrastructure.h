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

#ifndef _INFRASTRUCTURE_H_
#define _INFRASTRUCTURE_H_

#include <stdbool.h>
#include <stdint.h>
#include "struct_ids.h"

#define INFRA_HOST_IF_SIDE          1
#define INFRA_HOST_IF_ENGINE        0
#define INFRA_HOST_IF_NUMBER        0
#define INFRA_HOST_IF_LOGICAL_ID    128

#define INFRA_NW_IF_SIDE            0
#define INFRA_NW_IF_BASE_LOGICAL_ID 0

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

bool infra_create_if_mapping(void);
bool infra_create_mem_partition(void);
bool infra_create_statistics(void);
bool infra_configure_protocol_decode(void);

bool infra_initialize_statistics(void);

bool infra_create_hash(enum alvs_struct_id struct_id, enum infra_search_mem_heaps search_mem_heap, struct infra_hash_params *params);
bool infra_create_table(enum alvs_struct_id struct_id, enum infra_search_mem_heaps search_mem_heap, struct infra_table_params *params);
bool infra_add_entry(enum alvs_struct_id struct_id, void *key, uint32_t key_size, void *result, uint32_t result_size);
bool infra_delete_entry(enum alvs_struct_id struct_id, void *key, uint32_t key_size);
bool load_partition(void);

#endif /* _INFRASTRUCTURE_H_ */
