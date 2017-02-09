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

#ifndef _INDEX_POOL_H_
#define _INDEX_POOL_H_

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "log.h"

/* index pool structure. */
struct index_pool {
	uint32_t *indexes;
	/* indexes array */

	uint8_t *indexes_flags;
	/* array of indication flag for each index (free{0} or allocated{1}) */

	uint32_t head;
	/* head if queue (points to the next index to allocate) */

	uint32_t max_size;
	/* maximum number of indexes in the pool */

	uint32_t length;
	/* number of free indexes in the pool */
};


#define INDEX_FREE 0
#define INDEX_ALLOC 1


/**************************************************************************//**
 * \brief       Rewind pool to the initial state (all indexes not allocated)
 *
 * \param[in] pool   - reference to index pool reference
 *
 * \return        void
 */
void index_pool_rewind(struct index_pool *pool);

/**************************************************************************//**
 * \brief       Initialize pool (all indexes not allocated)
 *
 * \param[in] pool      - reference to index pool reference
 * \param[in] max_size  - maximum number of indexes in the pool
 *
 * \return        true iff initialization succeed.
 */
bool index_pool_init(struct index_pool *pool, int max_size);


/**************************************************************************//**
 * \brief       Destroy pool
 * \note        This function can be called only for a pool that was
 *              successfully initialized using index_pool_init().
 *
 * \param[in] pool      - reference to index pool reference
 *
 * \return        void
 */
void index_pool_destroy(struct index_pool *pool);

/**************************************************************************//**
 * \brief       Allocate index from pool
 *
 * \param[in] pool      - reference to index pool reference
 * \param[out] index    - reference to allocated index
 *
 * \return        true if index allocated successfully.
 *                flase if no indexes to allocatte.
 */
bool index_pool_alloc(struct index_pool *pool, uint32_t *index);

/**************************************************************************//**
 * \brief       Free index to pool
 *
 * \param[in] pool      - reference to index pool reference
 * \param[in] index     - index to free
 *
 * \return        void
 */
void index_pool_release(struct index_pool *pool, uint32_t index);

#endif /* _INDEX_POOL_H_ */
