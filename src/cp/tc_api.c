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
*  File:                tc_api.c
*  Desc:                Implementation of tc (traffic control) common API.
*
*/


#include "tc_api.h"
#include "tc_action.h"
#include "tc_flower.h"
#include "tc_db.h"

/******************************************************************************
 * \brief    Init ATC (Database & Index Pools)
 *
 * \return   enum tc_api_rc - TC_API_OK       - function succeed
 *                            TC_API_FAILURE  - function failed due to wrong configuration
 *                            TC_API_DB_ERROR - function failed due to problem on DB or NPS
 */
enum tc_api_rc tc_init(void)
{
	if (tc_action_init() == false) {
		return TC_API_DB_ERROR;
	}

	if (tc_flower_init() == false) {
		return TC_API_DB_ERROR;
	}

	if (tc_init_db() == false) {
		return TC_API_DB_ERROR;
	}

	return TC_API_OK;
}

/******************************************************************************
 * \brief    Close ATC (close Database & free Index Pools)
 *
 * \return   Void
 */
void tc_destroy(void)
{
	tc_flower_destroy();
	tc_action_destroy();

	if (tc_destroy_db() == false) {
		write_log(LOG_NOTICE, "failed to close TC database");
	}
}

/******************************************************************************
 * \brief    Add flower filter or modify filter if filter exist
 *
 * param[in] tc_filter_params - reference to filter configuration
 *
 * \return   enum tc_api_rc - TC_API_OK       - function succeed
 *                            TC_API_FAILURE  - function failed due to wrong configuration
 *                            TC_API_DB_ERROR - function failed due to problem on DB or NPS
 */
enum tc_api_rc tc_filter_add(struct tc_filter *tc_filter_params)
{
	switch (tc_filter_params->type) {
	case TC_FILTER_FLOWER:
		return tc_flower_filter_add(tc_filter_params);
	default:
		return TC_API_FAILURE;
	}
}

/******************************************************************************
 * \brief    Delete flower filter
 *
 * param[in] tc_filter_params - reference to filter configuration
 *
 * \return   enum tc_api_rc - TC_API_OK       - function succeed
 *                            TC_API_FAILURE  - function failed due to wrong configuration
 *                            TC_API_DB_ERROR - function failed due to problem on DB or NPS
 */
enum tc_api_rc tc_filter_delete(struct tc_filter *tc_filter_params)
{
	switch (tc_filter_params->type) {
	case TC_FILTER_FLOWER:
		return tc_flower_filter_delete(tc_filter_params);
	default:
		return TC_API_FAILURE;
	}
}

/******************************************************************************
 * \brief    Delete all filters on interface
 *
 * param[in]   interface - interface index
 * param[in]   type      - the type of the filter (flower, u32,...)
 *
 * \return   enum tc_api_rc - TC_API_OK       - function succeed
 *                            TC_API_FAILURE  - function failed due to wrong configuration
 *                            TC_API_DB_ERROR - function failed due to problem on DB or NPS
 */
enum tc_api_rc tc_delete_all_filters_on_interface(uint32_t interface, enum tc_filter_type type)
{
	switch (type) {
	case TC_FILTER_FLOWER:
		return tc_delete_all_flower_filters_on_interface(interface);
	default:
		return TC_API_FAILURE;
	}
}
