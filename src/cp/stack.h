/*
 * stack.h
 *
 *  Created on: May 11, 2016
 *      Author: roee
 */

#ifndef _STACK_H_
#define _STACK_H_

#include <stdlib.h>
#include <stdbool.h>

struct index_pool {
	uint32_t *data;
	uint32_t pos;
	uint32_t max_size;
};

bool index_pool_init(struct index_pool *pool, int max_size)
{
	uint32_t ind;

	pool->max_size = max_size;
	pool->data = (uint32_t *)malloc(max_size*sizeof(uint32_t));
	if (pool->data == NULL) {
		return false;
	}

	index_pool_rewind(pool);
	return true;
}

void index_pool_destroy(struct index_pool *pool)
{
	free(pool->data);
}

void index_pool_release(struct index_pool *pool, uint32_t value)
{
	pool->data[--pool->pos] = value;
}

bool index_pool_alloc(struct index_pool *pool, uint32_t *value)
{
	if (pool->pos == pool->max_size) {
		return false;
	}
	*value = pool->data[pool->pos++];
	return true;
}

void index_pool_rewind(struct index_pool *pool)
{
	uint32_t ind;

	pool->pos = 0;
	for (ind = 0; ind < pool->max_size; ind++) {
		pool->data[ind] = ind;
	}
}

#endif /* _STACK_H_ */
