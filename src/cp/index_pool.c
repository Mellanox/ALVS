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
*  Project:             NPS400 ANL application
*  File:                index_pool.c
*  Desc:                Index Pool Functions.
*
*/

#include "index_pool.h"

void index_pool_rewind(struct index_pool *pool)
{
	uint32_t ind;

	/* initialize head to 0 and length to max size */
	pool->head = 0;
	pool->length = pool->max_size;

	/* intialize indexes to [1..max_size]
	 * and allocation flags to 0.
	 */
	for (ind = 0; ind < pool->max_size; ind++) {
		pool->indexes[ind] = ind;
		pool->indexes_flags[ind] = INDEX_FREE;
	}
}

bool index_pool_init(struct index_pool *pool, int max_size)
{
	/* initialize max_size */
	pool->max_size = max_size;

	/* allocate indexes array */
	pool->indexes = (uint32_t *)malloc(max_size * sizeof(uint32_t));
	if (pool->indexes == NULL) {
		return false;
	}

	/* allocate allocation flags */
	pool->indexes_flags = (uint8_t *)malloc(max_size * sizeof(uint8_t));
	if (pool->indexes_flags == NULL) {
		free(pool->indexes);
		return false;
	}

	/* rewind pool */
	index_pool_rewind(pool);
	return true;
}

void index_pool_destroy(struct index_pool *pool)
{
	free(pool->indexes);
	free(pool->indexes_flags);
}

bool index_pool_alloc(struct index_pool *pool, uint32_t *index)
{
	/* if no more indexes return false */
	if (pool->length == 0) {
		return false;
	}

	/* take index in head */
	*index = pool->indexes[pool->head];
	pool->indexes_flags[*index] = INDEX_ALLOC;
	pool->head = (pool->head + 1) % pool->max_size;
	pool->length--;

	return true;
}

void index_pool_release(struct index_pool *pool, uint32_t index)
{
	uint32_t tail;

	/* Check if index is valid */
	if (index >= pool->max_size) {
		write_log(LOG_WARNING, "Index %d is out of pool boundaries..", index);
		return;
	}

	/* check if index is allocated */
	if (pool->indexes_flags[index] == INDEX_FREE) {
		write_log(LOG_WARNING, "Index %d is already free.", index);
		return;
	}


	/* add it to tail of queue */
	/* calculate tail as (head+length)%size */
	tail = (pool->head + pool->length) % pool->max_size;
	pool->indexes[tail] = index;
	pool->indexes_flags[index] = INDEX_FREE;
	pool->length++;
}

