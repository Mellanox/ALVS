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
*  File:                application_infra.c
*  Desc:                Implementation of application infrastructure API.
*
*/

#include "log.h"
#include "application_infra.h"
#include "application_search_defs.h"
#include "infrastructure.h"
#include "defs.h"


/******************************************************************************
 * \brief    Constructor function for application info DB.
 *           application info DB is a direct table contains 16 entries, exists on X1-IMEM
 *
 * \return   true/false
 */
bool application_info_db_constructor(void)
{
	struct infra_table_params table_params;
	bool retcode;

	write_log(LOG_DEBUG, "Creating application info table.");
	table_params.key_size = sizeof(struct application_info_key);
	table_params.result_size = sizeof(union application_info_result);
	table_params.max_num_of_entries = APPLICATION_INFO_MAX_ENTRIES;
	table_params.updated_from_dp = false;
	table_params.search_mem_heap = INFRA_X1_CLUSTER_SEARCH_HEAP;
	retcode = infra_create_table(STRUCT_ID_APPLICATION_INFO, &table_params);
	if (retcode == false) {
		write_log(LOG_CRIT, "Failed to create application info table.");
		return false;
	}
	return true;
}

