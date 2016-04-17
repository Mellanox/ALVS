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

#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <stdint.h>
#include <stdbool.h>
#include <EZapiChannel.h>

#define     ALVS_NUM_OF_MSIDS                   12
#define     ALVS_HALF_CLUSTER_CODE_SIZE		0
#define     ALVS_HALF_CLUSTER_DATA_SIZE         0
#define     ALVS_1_CLUSTER_CODE_SIZE            0
#define     ALVS_1_CLUSTER_DATA_SIZE            0
#define     ALVS_2_CLUSTER_CODE_SIZE		0
#define     ALVS_2_CLUSTER_DATA_SIZE		0
#define     ALVS_4_CLUSTER_CODE_SIZE		0
#define     ALVS_4_CLUSTER_DATA_SIZE		0
#define     ALVS_16_CLUSTER_CODE_SIZE		0
#define     ALVS_16_CLUSTER_DATA_SIZE		0
#define     ALVS_ALL_CLUSTER_CODE_SIZE		0
#define     ALVS_ALL_CLUSTER_DATA_SIZE		0

static
uint32_t alvs_imem_sizes[ALVS_NUM_OF_MSIDS][2] = {
		{EZapiChannel_IntMemSpaceType_HALF_CLUSTER_CODE, ALVS_HALF_CLUSTER_CODE_SIZE},
		{EZapiChannel_IntMemSpaceType_HALF_CLUSTER_DATA, ALVS_HALF_CLUSTER_DATA_SIZE},
		{EZapiChannel_IntMemSpaceType_1_CLUSTER_CODE, ALVS_1_CLUSTER_CODE_SIZE},
		{EZapiChannel_IntMemSpaceType_1_CLUSTER_DATA, ALVS_1_CLUSTER_DATA_SIZE},
		{EZapiChannel_IntMemSpaceType_2_CLUSTER_CODE, ALVS_2_CLUSTER_CODE_SIZE},
		{EZapiChannel_IntMemSpaceType_2_CLUSTER_DATA, ALVS_2_CLUSTER_DATA_SIZE},
		{EZapiChannel_IntMemSpaceType_4_CLUSTER_CODE, ALVS_4_CLUSTER_CODE_SIZE},
		{EZapiChannel_IntMemSpaceType_4_CLUSTER_DATA, ALVS_4_CLUSTER_DATA_SIZE},
		{EZapiChannel_IntMemSpaceType_16_CLUSTER_CODE, ALVS_16_CLUSTER_CODE_SIZE},
		{EZapiChannel_IntMemSpaceType_16_CLUSTER_DATA, ALVS_16_CLUSTER_DATA_SIZE},
		{EZapiChannel_IntMemSpaceType_ALL_CLUSTER_CODE, ALVS_ALL_CLUSTER_CODE_SIZE},
		{EZapiChannel_IntMemSpaceType_ALL_CLUSTER_DATA, ALVS_ALL_CLUSTER_DATA_SIZE} };

bool alvs_create_mem_partition(void);

uint32_t alvs_get_imem_index(void);


#endif /* _MEMORY_H_ */
