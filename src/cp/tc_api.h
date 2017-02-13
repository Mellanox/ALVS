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
*  File:                tc_api.h
*  Desc:                Traffic Control (TC) API definitions.
*
*/
#ifndef _TC_API_H_
#define _TC_API_H_

#include "tc_manager.h"

enum tc_api_rc {
	TC_API_OK,
	TC_API_FAILURE,
	TC_API_DB_ERROR,
};

#define TC_CHECK_ERROR(function_to_execute)							\
do {												\
	enum tc_api_rc rc;									\
	rc = function_to_execute;								\
	if (rc != TC_API_OK) {									\
		write_log(LOG_NOTICE, "Function %s failed on line %d", __func__, __LINE__);	\
		return rc;									\
	}											\
} while (0)


enum tc_api_rc tc_add_filter(struct tc_filter *tc_filter_params);
enum tc_api_rc tc_delete_filter(struct tc_filter *tc_filter_params);
enum tc_api_rc tc_modify_filter(struct tc_filter *tc_filter_params);

enum tc_api_rc add_tc_action(struct tc_action *tc_action_params);
enum tc_api_rc delete_tc_action(struct tc_action *tc_action_params);
enum tc_api_rc modify_tc_action(struct tc_action *tc_action_params);

enum tc_api_rc tc_init(void);
void tc_destroy(void);

/******************************************************************************
 * \brief    Raises SIGTERM signal to main thread and exits the thread.
 *           Deletes the DB manager.
 *
 * \return   void
 */
void tc_db_manager_exit_with_error(void);



enum tc_api_rc tc_delete_all_filters_on_interface(uint32_t interface);

enum tc_api_rc tc_api_get_actions_list(enum tc_action_type type, uint32_t *action_indexes, uint32_t *num_of_actions);
enum tc_api_rc tc_api_get_action_info(enum tc_action_type type,
				      uint32_t action_index,
				      struct tc_action *tc_action,
				      bool *is_action_exists);
enum tc_api_rc tc_api_get_filters_list(uint32_t interface,
				       uint32_t priority,
				       struct tc_filter_id **filters_array,
				       uint32_t *num_of_filters);
enum tc_api_rc tc_api_get_filter_info(struct tc_filter_id *tc_filter_id, struct tc_filter *tc_filter_params);

#endif /* _TC_API_H_ */
