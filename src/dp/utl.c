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
*  Project:             NPS400 ALVS application
*  File:                nw_routing.h
*  Desc:                common NPS data path utilities
*/


/* system includes */
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* dp includes */
#include <ezdp.h>
#include <ezframe.h>

#include "defs.h"

#define MAX_NUM_OF_CPUS 4096

/****** global variable ***********/
bool      is_child_process;
uint32_t  num_cpus;
uint32_t  run_cpus[MAX_NUM_OF_CPUS];
uint32_t  pids[MAX_NUM_OF_CPUS];


void  set_gracefull_stop(void);
void  set_gracefull_stop(void)
{
	ezframe_set_cancel_signal();
}

/************************************************************************
 * \brief      signal terminate handler
 *
 * \note       assumes num_cpus==0 if and only if child process
 *
 * \return     void
 */
void signal_terminate_handler_gracefully_stop(int signum __unused);
void signal_terminate_handler_gracefully_stop(int signum __unused)
{
	if ((is_child_process == true) || (num_cpus == 1)) {
		set_gracefull_stop();
	} else {
		killpg(0, SIGTERM);
		exit(0);
	}
}

/************************************************************************
 * \brief      signal terminate handler
 *
 * \note       assumes num_cpus==0 if and only if child process
 *
 * \return     void
 */
void signal_terminate_handler(int signum);
void signal_terminate_handler(int signum)
{
	if (signum != SIGTERM) {
		/* kill all other processes */
		killpg(0, SIGTERM);
	}
	abort();
}


/************************************************************************/
/*                           parse run_cpus                             */
/************************************************************************/

/****** macros ***********/
#define howmany(x, y) (((x)+((y)-1))/(y))
#define bitsperlong (8 * sizeof(unsigned long))
#define longsperbits(n) howmany(n, bitsperlong)


/****** structures ***********/
struct bitmask {
	unsigned int size;
	unsigned long *maskp;
};



/****** functions ***********/
struct bitmask *bitmask_alloc(unsigned int n);
struct bitmask *bitmask_alloc(unsigned int n)
{
	struct bitmask *bmp;

	bmp = malloc(sizeof(*bmp));
	if (!bmp)
		return 0;
	bmp->size = n;
	bmp->maskp = calloc(longsperbits(n), sizeof(unsigned long));
	if (!bmp->maskp) {
		free(bmp);
		return 0;
	}
	return bmp;
}

static unsigned int _getbit(const struct bitmask *bmp, unsigned int n)
{
	if (n < bmp->size)
		return (bmp->maskp[n/bitsperlong] >> (n % bitsperlong)) & 1;
	else
		return 0;
}

static void _setbit(struct bitmask *bmp, unsigned int n, unsigned int v)
{
	if (n < bmp->size) {
		if (v)
			bmp->maskp[n/bitsperlong] |= 1UL << (n % bitsperlong);
		else
			bmp->maskp[n/bitsperlong] &= ~(1UL << (n % bitsperlong));
	}
}

int bitmask_isbitset(const struct bitmask *bmp, unsigned int i);
int bitmask_isbitset(const struct bitmask *bmp, unsigned int i)
{
	return _getbit(bmp, i);
}

struct bitmask *bitmask_clearall(struct bitmask *bmp);
struct bitmask *bitmask_clearall(struct bitmask *bmp)
{
	unsigned int i;

	for (i = 0; i < bmp->size; i++)
		_setbit(bmp, i, 0);
	return bmp;
}


struct bitmask *bitmask_setbit(struct bitmask *bmp, unsigned int i);
struct bitmask *bitmask_setbit(struct bitmask *bmp, unsigned int i)
{
	_setbit(bmp, i, 1);
	return bmp;
}


static const char *nexttoken(const char *q,  int sep)
{
	if (q)
		q = strchr(q, sep);
	if (q)
		q++;
	return q;
}

extern uint32_t num_cpus;
static int cstr_to_cpuset(struct bitmask *mask, const char *str)
{
	const char *p, *q = str;
	int rc;

	num_cpus = 0;
	bitmask_clearall(mask);

	while (p = q, q = nexttoken(q, ','), p) {
		unsigned int a;	/* beginning of range */
		unsigned int b;	/* end of range */
		unsigned int s;	/* stride */
		const char *c1, *c2;

		/* get next CPU-ID or first CPU ID in the range */
		rc = sscanf(p, "%4u", &a);
		if (rc < 1) {
			/* ERROR: expecting number in the string */
			return 1;
		}

		/* init */
		b = a;	                /* in case of no range, end of the range equal the begining */
		s = 1;                   /* one stride */
		c1 = nexttoken(p, '-');  /* next '-' char */
		c2 = nexttoken(p, ',');  /* next ',' char */

		/* check if the next tocken is a range */
		if (c1 != NULL && (c2 == NULL || c1 < c2)) {
			/* hanldle a range */

			/* get the last CPU ID in the range */
			rc = sscanf(c1, "%4u", &b);
			if (rc < 1) {
				/* ERROR: expecting number in the string */
				return 1;
			}

			c1 = nexttoken(c1, ':');
			if (c1 != NULL && (c2 == NULL || c1 < c2))
				if (sscanf(c1, "%10u", &s) < 1)
					return 1;
		}

		/* checker */
		if (!(a <= b)) {
			/* ERROR: end of the range must not be lower than the beginning of the range */
			return 1;
		}

		/* add the range */
		while (a <= b) {
			if (bitmask_isbitset(mask, a) == 0) {
				run_cpus[num_cpus] = a; /* cpu_id */
				num_cpus++;
			}
			bitmask_setbit(mask, a);

			a += s;
		}
	}

	return 0;
}



/************************************************************************
 * \brief      Updates the cores array according to the input.
 * \details    Parses a list cpus represented by the given string,
 *             and places in the run_cpus array.
 *
 * \param[in]  processors_str - the string to parse given by the user
 *
 *  \note:     processors_str is a comma-delimited list of separate
 *             processors, or a range of processors, like so 0,5,7-9
 *
 * \return     None
 */
void add_run_cpus(const char *processors_str);
void add_run_cpus(const char *processors_str)
{
	struct bitmask  *new_mask;
	uint32_t        rc;

	/*
	 * new_mask is always used for the sched_setaffinity call
	 * On the sched_setaffinity the kernel will zero-fill its
	 * cpumask_t if the user's mask is shorter.
	 */
	new_mask = bitmask_alloc(MAX_NUM_OF_CPUS);
	if (!new_mask) {
		printf("bitmask_alloc failed\n");
		exit(1);
	}

	rc = cstr_to_cpuset(new_mask, processors_str);
	if (rc != 0) {
		free(new_mask);
		printf("cstr_to_cpuset failed\n");
		exit(1);
	}

	free(new_mask);
}
