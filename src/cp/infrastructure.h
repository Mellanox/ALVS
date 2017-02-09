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
enum infra_internal_search_mem_heaps {
	INFRA_HALF_CLUSTER_SEARCH_HEAP,
	INFRA_1_CLUSTER_SEARCH_HEAP,
	INFRA_2_CLUSTER_SEARCH_HEAP,
	INFRA_4_CLUSTER_SEARCH_HEAP,
	INFRA_16_CLUSTER_SEARCH_HEAP,
	INFRA_ALL_CLUSTER_SEARCH_HEAP,
	INFRA_NOT_VALID_INTERNAL_HEAP
};

/*! Search memory heaps possible values. */
enum infra_external_search_mem_heaps {
	NW_EMEM_SEARCH_HEAP,
#ifdef CONFIG_ALVS
	ALVS_EMEM_SEARCH_0_HEAP,
	ALVS_EMEM_SEARCH_1_HEAP,
	ALVS_EMEM_SEARCH_2_HEAP,
#endif
#ifdef CONFIG_TC
	TC_EMEM_SEARCH_0_HEAP,
#endif
	INFRA_NOT_VALID_EXTERNAL_HEAP
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
 * \brief       Destruct all DBs
 *
 */
void infra_db_destructor(void);

/**************************************************************************//**
 * \brief       Find the index of the required memory heap
 *
 * \return      bool - success or failure
 */
uint32_t infra_index_of_heap(uint32_t search_mem_heap,
			     bool is_external);

#endif /* _INFRASTRUCTURE_H_ */
