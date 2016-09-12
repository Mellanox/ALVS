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
*  File:                alvs_utils.c
*  Desc:                various alvs helper functions
*/

#include "global_defs.h"
#include "alvs_utils.h"

struct alvs_cmem             cmem_alvs               __cmem_var;
struct alvs_shared_cmem      shared_cmem_alvs        __cmem_shared_var;

/******************************************************************************
 * \brief         Initialize alvs shared CMEM constructor
 *
 * \return        true/false (success / failed )
 */
bool init_alvs_shared_cmem(void)
{
	uint32_t  result;

	/*Init connection classification DB*/
	result = ezdp_init_hash_struct_desc(STRUCT_ID_ALVS_CONN_CLASSIFICATION,
					    &shared_cmem_alvs.conn_class_struct_desc,
					    cmem_wa.alvs_wa.conn_hash_wa,
					    sizeof(cmem_wa.alvs_wa.conn_hash_wa));
	if (result != 0) {
		printf("ezdp_init_hash_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
				STRUCT_ID_ALVS_CONN_CLASSIFICATION, result, ezdp_get_err_msg());
		return false;
	}

	result = ezdp_validate_hash_struct_desc(&shared_cmem_alvs.conn_class_struct_desc,
						true,
						sizeof(struct alvs_conn_classification_key),
						sizeof(struct alvs_conn_classification_result));
	if (result != 0) {
		printf("ezdp_validate_hash_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
				STRUCT_ID_ALVS_CONN_CLASSIFICATION, result, ezdp_get_err_msg());
		return false;
	}

	/*Init connection info DB*/
	result = ezdp_init_table_struct_desc(STRUCT_ID_ALVS_CONN_INFO,
					     &shared_cmem_alvs.conn_info_struct_desc,
					     cmem_wa.alvs_wa.table_struct_work_area,
					     sizeof(cmem_wa.alvs_wa.table_struct_work_area));
	if (result != 0) {
		printf("ezdp_init_hash_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
				STRUCT_ID_ALVS_CONN_INFO, result, ezdp_get_err_msg());
		return false;
	}

	result = ezdp_validate_table_struct_desc(&shared_cmem_alvs.conn_info_struct_desc,
						 sizeof(struct alvs_conn_info_result));
	if (result != 0) {
		printf("ezdp_validate_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
				STRUCT_ID_ALVS_CONN_INFO, result, ezdp_get_err_msg());
		return false;
	}

	/*Init server classification DB*/
	result = ezdp_init_hash_struct_desc(STRUCT_ID_ALVS_SERVER_CLASSIFICATION,
					    &shared_cmem_alvs.server_class_struct_desc,
					    cmem_wa.alvs_wa.server_hash_wa,
					    sizeof(cmem_wa.alvs_wa.server_hash_wa));
	if (result != 0) {
		printf("ezdp_init_hash_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
				STRUCT_ID_ALVS_SERVER_CLASSIFICATION, result, ezdp_get_err_msg());
		return false;
	}

	result = ezdp_validate_hash_struct_desc(&shared_cmem_alvs.server_class_struct_desc,
						true,
						sizeof(struct alvs_server_classification_key),
						sizeof(struct alvs_server_classification_result));
	if (result != 0) {
		printf("ezdp_validate_hash_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
				STRUCT_ID_ALVS_SERVER_CLASSIFICATION, result, ezdp_get_err_msg());
		return false;
	}

	/*Init server info DB*/
	result = ezdp_init_table_struct_desc(STRUCT_ID_ALVS_SERVER_INFO,
					     &shared_cmem_alvs.server_info_struct_desc,
					     cmem_wa.alvs_wa.table_struct_work_area,
					     sizeof(cmem_wa.alvs_wa.table_struct_work_area));
	if (result != 0) {
		printf("ezdp_init_hash_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
				STRUCT_ID_ALVS_SERVER_INFO, result, ezdp_get_err_msg());
		return false;
	}

	result = ezdp_validate_table_struct_desc(&shared_cmem_alvs.server_info_struct_desc,
						 sizeof(struct alvs_server_info_result));
	if (result != 0) {
		printf("ezdp_validate_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
				STRUCT_ID_ALVS_SERVER_INFO, result, ezdp_get_err_msg());
		return false;
	}

	/*Init sched info DB*/
	result = ezdp_init_table_struct_desc(STRUCT_ID_ALVS_SCHED_INFO,
					     &shared_cmem_alvs.sched_info_struct_desc,
					     cmem_wa.alvs_wa.table_struct_work_area,
					     sizeof(cmem_wa.alvs_wa.table_struct_work_area));
	if (result != 0) {
		printf("ezdp_init_hash_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
				STRUCT_ID_ALVS_SCHED_INFO, result, ezdp_get_err_msg());
		return false;
	}

	result = ezdp_validate_table_struct_desc(&shared_cmem_alvs.sched_info_struct_desc,
						 sizeof(struct alvs_sched_info_result));
	if (result != 0) {
		printf("ezdp_validate_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
				STRUCT_ID_ALVS_SCHED_INFO, result, ezdp_get_err_msg());
		return false;
	}

	/*Init service class DB*/
	result = ezdp_init_hash_struct_desc(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION,
					    &shared_cmem_alvs.service_class_struct_desc,
					    cmem_wa.alvs_wa.service_hash_wa,
					    sizeof(cmem_wa.alvs_wa.service_hash_wa));
	if (result != 0) {
		printf("ezdp_init_hash_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
				STRUCT_ID_ALVS_SERVICE_CLASSIFICATION, result, ezdp_get_err_msg());
		return false;
	}

	result = ezdp_validate_hash_struct_desc(&shared_cmem_alvs.service_class_struct_desc,
						true,
						sizeof(struct alvs_service_classification_key),
						sizeof(struct alvs_service_classification_result));
	if (result != 0) {
		printf("ezdp_validate_hash_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
				STRUCT_ID_ALVS_SERVICE_CLASSIFICATION, result, ezdp_get_err_msg());
		return false;
	}

	/*Init service info DB*/
	result = ezdp_init_table_struct_desc(STRUCT_ID_ALVS_SERVICE_INFO,
					     &shared_cmem_alvs.service_info_struct_desc,
					     cmem_wa.alvs_wa.table_struct_work_area,
					     sizeof(cmem_wa.alvs_wa.table_struct_work_area));
	if (result != 0) {
		printf("ezdp_init_hash_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
				STRUCT_ID_ALVS_SERVICE_INFO, result, ezdp_get_err_msg());
		return false;
	}

	result = ezdp_validate_table_struct_desc(&shared_cmem_alvs.service_info_struct_desc,
						 sizeof(struct alvs_service_info_result));
	if (result != 0) {
		printf("ezdp_validate_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
				STRUCT_ID_ALVS_SERVICE_INFO, result, ezdp_get_err_msg());
		return false;
	}

	return true;
}

/******************************************************************************
 * \brief         Initialize alvs private CMEM constructor
 *
 * \return        true/false (success / failed )
 */
bool init_alvs_private_cmem(void)
{
	struct ezdp_ext_addr	addr;
	uint32_t id;

	memset(&addr, 0x0, sizeof(struct ezdp_ext_addr));
	addr.mem_type = EZDP_EXTERNAL_MS;
	addr.msid     = EMEM_SPINLOCK_MSID;

	for (id = 0; id < ALVS_CONN_LOCK_ELEMENTS_COUNT; id++) { /*TODO is this code redundant?*/
		ezdp_init_spinlock_ext_addr(&cmem_alvs.conn_spinlock, &addr);
		addr.address++;
	}

	/*init state sync*/
	cmem_alvs.conn_sync_state.conn_sync_status = ALVS_CONN_SYNC_NO_NEED;
	cmem_alvs.conn_sync_state.amount_buffers = 0;
	cmem_alvs.conn_sync_state.current_base = NULL;
	cmem_alvs.conn_sync_state.current_len = 0;
	cmem_alvs.conn_sync_state.conn_count = 0;

	return true;
}
