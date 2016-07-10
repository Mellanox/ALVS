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
*  File:                main.c
*  Desc:                data path main. NPS Initialization & configuration.
*/

/* system includes */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>

/* dp includes */
#include <ezdp.h>
#include <ezframe.h>

/*internal includes*/
#include "nw_recieve.h"
#include "alvs_aging.h"
#include "version.h"

/******************************************************************************
 * Globals & defines
 *****************************************************************************/
ezframe_t  frame __cmem_var;
uint8_t    frame_data[EZFRAME_BUF_DATA_SIZE] __cmem_var;
union      cmem_workarea cmem_wa __cmem_var;

#define MAX_NUM_OF_CPUS 4096


bool      is_child_process;
uint32_t  num_cpus;
uint32_t  run_cpus[MAX_NUM_OF_CPUS];
uint32_t  pids[MAX_NUM_OF_CPUS];

/****** macros ***********/
#define howmany(x, y) (((x)+((y)-1))/(y))
#define bitsperlong (8 * sizeof(unsigned long))
#define longsperbits(n) howmany(n, bitsperlong)


/****** structures ***********/
struct bitmask {
	unsigned int size;
	unsigned long *maskp;
};

/******************************************************************************
 * Prototypes
 *****************************************************************************/
void signal_terminate_handler(int signum);
void signal_terminate_handler_gracefully_stop(int signum);
void set_gracefull_stop(void);
void signal_terminate_handler_gracefully_stop(int signum __unused);
void signal_terminate_handler(int signum);
struct bitmask *bitmask_alloc(unsigned int n);
int bitmask_isbitset(const struct bitmask *bmp, unsigned int i);
struct bitmask *bitmask_setbit(struct bitmask *bmp, unsigned int i);
void add_run_cpus(const char *processors_str);
struct bitmask *bitmask_clearall(struct bitmask *bmp);
bool init_memory(enum ezdp_data_mem_space data_ms_type, uintptr_t user_data __unused);

/************************************************************************
 * \brief      set_gracefull_stop
 * \return     void
 */
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

/****** functions ***********/

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

int bitmask_isbitset(const struct bitmask *bmp, unsigned int i)
{
	return _getbit(bmp, i);
}

struct bitmask *bitmask_clearall(struct bitmask *bmp)
{
	unsigned int i;

	for (i = 0; i < bmp->size; i++)
		_setbit(bmp, i, 0);
	return bmp;
}

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

/******************************************************************************
 * \brief         Call back function for Initializing memory
 *
 * \return        true/false (success / failed )
 */
bool init_memory(enum ezdp_data_mem_space data_ms_type, uintptr_t user_data __unused)
{
	switch (data_ms_type) {
	case EZDP_CMEM_DATA:
		return init_alvs_private_cmem();
	case EZDP_SHARED_CMEM_DATA:
		printf("init_shared_cmem cpu_id=%d\n", ezdp_get_cpu_id());
		alvs_open_log();
		return init_nw_shared_cmem() & init_alvs_shared_cmem();
	default:
		return true;
	}
}

/************************************************************************
 * \brief       Parses the run-time arguments of the application.
 *
 * \return      void
 */
static
void parse_arguments(int argc, char **argv, uint32_t *num_cpus_set_p)
{
	int idx;
	(*num_cpus_set_p) = 0;

	for (idx = 1; idx < argc;) {
		if (argv[idx][0] == '-') {
			if (strcmp(argv[idx], "--run_cpus") == 0) {
				add_run_cpus(argv[idx+1]);
				(*num_cpus_set_p) = 1;
				idx += 2;
			} else {
				printf("An unrecognized argument was found: %s\n", argv[idx]);
				idx++;
			}
		} else {
			printf("An unrecognized argument was found: %s\n", argv[idx]);
			idx++;
		}
	}
}


/******************************************************************************
* \brief                Print info of running application:
*                       Num of CPUs
*                       Version
*                       Memory
*
* \return               void
*/
void alvs_print_info(void)
{
	struct ezdp_mem_section_info mem_info;

	alvs_write_log(LOG_INFO, "starting ALVS DP application.");
	alvs_write_log(LOG_INFO, "num CPUs:            %d", num_cpus);
	alvs_write_log(LOG_INFO, "Application version: %s.", version);
	alvs_write_log(LOG_DEBUG, "ALVS MEMORY:");

	/* print memory section info */
	ezdp_get_mem_section_info(&mem_info, 0);

	alvs_write_log(LOG_DEBUG, "  private_cmem size is             %d bytes\n",
		       mem_info.private_cmem_size);
	alvs_write_log(LOG_DEBUG, "  shared_cmem size is              %d bytes\n",
		       mem_info.shared_cmem_size);

	if (mem_info.cache_size > 0) {
		alvs_write_log(LOG_DEBUG, "  cache size is                    %d bytes\n",
			       mem_info.cache_size);
	}
	if (mem_info.imem_private_data_size > 0) {
		alvs_write_log(LOG_DEBUG, "  imem_private data size is        %d bytes\n",
			       mem_info.imem_private_data_size);
	}
	if (mem_info.imem_half_cluster_code_size > 0) {
		alvs_write_log(LOG_DEBUG, "  imem_half_cluster code size is   %d bytes\n",
			       mem_info.imem_half_cluster_code_size);
	}
	if (mem_info.imem_half_cluster_data_size > 0) {
		alvs_write_log(LOG_DEBUG, "  imem_half_cluster data size is   %d bytes\n",
			       mem_info.imem_half_cluster_data_size);
	}
	if (mem_info.imem_1_cluster_code_size > 0) {
		alvs_write_log(LOG_DEBUG, "  imem_1_cluster code size is      %d bytes\n",
			       mem_info.imem_1_cluster_code_size);
	}
	if (mem_info.imem_1_cluster_data_size > 0) {
		alvs_write_log(LOG_DEBUG, "  imem_1_cluster data size is      %d bytes\n",
			       mem_info.imem_1_cluster_data_size);
	}
	if (mem_info.imem_2_cluster_code_size > 0) {
		alvs_write_log(LOG_DEBUG, "  imem_2_cluster code size is      %d bytes\n",
			       mem_info.imem_2_cluster_code_size);
	}
	if (mem_info.imem_2_cluster_data_size > 0) {
		alvs_write_log(LOG_DEBUG, "  imem_2_cluster data size is      %d bytes\n",
			       mem_info.imem_2_cluster_data_size);
	}
	if (mem_info.imem_4_cluster_code_size > 0) {
		alvs_write_log(LOG_DEBUG, "  imem_4_cluster code size is      %d bytes\n",
			       mem_info.imem_4_cluster_code_size);
	}
	if (mem_info.imem_4_cluster_data_size > 0) {
		alvs_write_log(LOG_DEBUG, "  imem_4_cluster data size is      %d bytes\n",
			       mem_info.imem_4_cluster_data_size);
	}
	if (mem_info.imem_16_cluster_code_size > 0) {
		alvs_write_log(LOG_DEBUG, "  imem_16_cluster code size is     %d bytes\n",
			       mem_info.imem_16_cluster_code_size);
	}
	if (mem_info.imem_16_cluster_data_size > 0) {
		alvs_write_log(LOG_DEBUG, "  imem_16_cluster data size is     %d bytes\n",
			       mem_info.imem_16_cluster_data_size);
	}
	if (mem_info.imem_all_cluster_code_size > 0) {
		alvs_write_log(LOG_DEBUG, "  imem_all_cluster code size is    %d bytes\n",
			       mem_info.imem_all_cluster_code_size);
	}
	if (mem_info.imem_all_cluster_data_size > 0) {
		alvs_write_log(LOG_DEBUG, "  imem_all_cluster data size is    %d bytes\n",
			       mem_info.imem_all_cluster_data_size);
	}
	if (mem_info.emem_data_size > 0) {
		alvs_write_log(LOG_DEBUG, "  emem data size is                %d bytes\n",
			       mem_info.emem_data_size);
	}
}
/******************************************************************************
* \brief                Main packet processing function
*
* \note                 Receive frame

* \return               void
*/
void packet_processing(void)
{
	int32_t port_id;

	if (ezdp_get_cpu_id() == run_cpus[0]) {
		alvs_print_info();
	}


	while (true) {
		/* === Receive Frame === */
		port_id = ezframe_receive(&frame, EZFRAME_HANDLE_FRAME_CANCEL);

		/* ===  received cancel frame === */
		if (unlikely(port_id == -1)) {
			exit(0);
		}

		if (port_id == ALVS_AGING_TIMER_LOGICAL_ID) {
			alvs_handle_aging_event(frame.job_desc.rx_info.timer_info.event_id);
			alvs_discard_frame();
		} else {
			nw_recieve_and_parse_frame(&frame,
						   frame_data,
						   port_id);
		}
	} /* while (true) */
}

/******************************************************************************
* \brief        Main function
*/
int main(int argc, char **argv)
{
	struct ezdp_mem_section_info mem_info;
	int32_t cpid = 0;
	uint32_t idx;
	int32_t status;
	uint32_t num_cpus_set;
	uint32_t result;

	/* listen to the SHUTDOWN signal to handle terminate signal from the simulator */
	signal(SIGINT, signal_terminate_handler_gracefully_stop);
	signal(SIGTERM, signal_terminate_handler_gracefully_stop);
	signal(SIGILL, signal_terminate_handler);
	signal(SIGSEGV, signal_terminate_handler);
	signal(SIGBUS, signal_terminate_handler);
	signal(SIGCHLD, SIG_IGN);

	is_child_process = false;

	/* Parse the run-time arguments */
	parse_arguments(argc, argv, &num_cpus_set);

	result = ezdp_sync_cp();
	if (result != 0) {
		printf("ezdp_sync_cp failed %d %s. Exiting...\n", result, ezdp_get_err_msg());
		exit(1);
	}

	ezdp_get_mem_section_info(&mem_info, 0);
	/* validate that at least 256 bytes thread cache is enabled*/
	if (mem_info.cache_size < 256) {
		printf("Too many cmem variables. Thread cache size is zero. private_cmem_size is %d shared_cmem_size is %d. Exiting...\n",
				mem_info.private_cmem_size, mem_info.shared_cmem_size);
		exit(1);
	}

	result = ezdp_init_global(0);
	if (result != 0) {
		printf("ezdp_init_global failed %d %s. Exiting...\n", result, ezdp_get_err_msg());
		exit(1);
	}

	if (!num_cpus_set) {
		/* run single process on processor 0*/
		num_cpus = 1;

		run_cpus[0] = 16;

		/* init the application */
		result = ezdp_init_local(0, run_cpus[0], init_memory, 0, 0);
		if (result != 0) {
			printf("ezdp_init_local failed %d %s. Exiting...\n", result, ezdp_get_err_msg());
			exit(1);
		}

		result = ezframe_init_local();
		if (result != 0) {
			printf("ezframe_init_local failed. %d; %s. Exiting...\n", result, ezdp_get_err_msg());
			exit(1);
		}

		/* call to packet processing function */
		ezdp_run(&packet_processing, num_cpus);

		return 0;
	}

	if (num_cpus == 1) {
		/* init the application */
		result = ezdp_init_local(0, run_cpus[0], init_memory, 0, 0);
		if (result != 0) {
			printf("ezdp_init_local failed %d %s. Exiting...\n", result, ezdp_get_err_msg());
			exit(1);
		}
		result = ezframe_init_local();
		if (result != 0) {
			printf("ezframe_init_local failed. %d; %s. Exiting...\n", result, ezdp_get_err_msg());
			exit(1);
		}

		/* call to packet processing function */
		ezdp_run(&packet_processing, num_cpus);

		return 0;
	}

	for (idx = 0; idx < num_cpus; idx++) {
		cpid = fork();

		if (cpid == -1) { /* error */
			perror("Error creating child process - fork fail\n");
			exit(1);
		}

		if (cpid  == 0) { /* code executed by child */
			/* this will mark this process as a child process */
			is_child_process = true;

			/* init the dp/application */
			result = ezdp_init_local(0, run_cpus[idx], init_memory, 0, 0);
			if (result != 0) {
				printf("ezdp_init_local failed %d %s. Exiting...\n", result, ezdp_get_err_msg());
				exit(1);
			}
			result = ezframe_init_local();
			if (result != 0) {
				printf("ezframe_init_local failed. %d; %s. Exiting...\n", result, ezdp_get_err_msg());
				exit(1);
			}
			/* call to packet processing function */
			ezdp_run(&packet_processing, num_cpus);

			return 0;
		}
		pids[idx] = cpid;
	}

	/* code executed by parent */
	for (idx = 0; idx < num_cpus; idx++) {
		wait(&status);
	}

	return 0;
}
