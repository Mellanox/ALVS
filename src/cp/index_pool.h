/*
 * index_pool.h
 *
 *  Created on: Sep 11, 2016
 *      Author: roee
 */

#ifndef _INDEX_POOL_H_
#define _INDEX_POOL_H_

#include <stdlib.h>
#include <stdbool.h>
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

/**************************************************************************//**
 * \brief       Initialize pool (all indexes not allocated)
 *
 * \param[in] pool      - reference to index pool reference
 * \param[in] max_size  - maximum number of indexes in the pool
 *
 * \return        true iff initialization succeed.
 */
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

/**************************************************************************//**
 * \brief       Destroy pool
 * \note        This function can be called only for a pool that was
 *              successfully initialized using index_pool_init().
 *
 * \param[in] pool      - reference to index pool reference
 *
 * \return        void
 */
void index_pool_destroy(struct index_pool *pool)
{
	free(pool->indexes);
	free(pool->indexes_flags);
}

/**************************************************************************//**
 * \brief       Allocate index from pool
 *
 * \param[in] pool      - reference to index pool reference
 * \param[out] index    - reference to allocated index
 *
 * \return        true if index allocated successfully.
 *                flase if no indexes to allocatte.
 */
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

/**************************************************************************//**
 * \brief       Free index to pool
 *
 * \param[in] pool      - reference to index pool reference
 * \param[in] index     - index to free
 *
 * \return        void
 */
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

#endif /* _INDEX_POOL_H_ */
