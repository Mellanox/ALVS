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
*  Project:             NPS400 TC application
*  File:                tc_actions.h
*  Desc:                TC actions include file.
*
*/

#ifndef _TC_ACTION_API_H_
#define _TC_ACTION_API_H_

#include "log.h"
#include "infrastructure_utils.h"
#include "tc_conf.h"
#include "tc_api.h"
#include "index_pool.h"


/************************************************/
/* Extern variables                             */
/************************************************/
/* TODO: ugly extern - change it
 */
extern struct index_pool action_index_pool;
extern struct index_pool action_extra_info_index_pool;

/************************************************/
/* Defines                                      */
/************************************************/
#define NOT_VALID_ACTION_INDEX         0xFFFFFFFF
#define NOT_VALID_FILTER_ACTIONS_INDEX 0xFFFFFFFF

/************************************************/
/* Structs                                      */
/************************************************/
struct action_stats {
	uint32_t last_used_timestamp;
	uint32_t created_timestamp;
	uint64_t in_packets;
	uint64_t in_bytes;
	enum tc_action_type action_type;
};

struct pedit_key_data {
	uint32_t key_index;
	struct tc_pedit_key_data pedit_key_data;

};

struct pedit_action_data {
	enum tc_action_type control_action_type;
	uint8_t num_of_keys;
	struct pedit_key_data key_data[MAX_NUM_OF_KEYS_IN_PEDIT_DATA];

};

struct action_id {
	uint32_t linux_action_index;
	enum tc_action_type action_family_type;
};

struct action_info {
	struct action_id action_id;
	enum tc_action_type action_type;
	uint32_t nps_index;
	bool independent_action;
	struct ezdp_sum_addr statistics_base;
	union {
		struct mirred_action_data mirred;
		struct pedit_action_data pedit;
	} action_data;
	uint32_t created_timestamp;
};

/************************************************/
/* Function declaration                         */
/************************************************/

enum tc_api_rc add_tc_action(struct tc_action *tc_action_params);
enum tc_api_rc delete_tc_action(struct tc_action *tc_action_params);
enum tc_api_rc modify_tc_action(struct tc_action *tc_action_params);
bool tc_action_init(void);
void tc_action_destroy(void);

/**************************************************************************//**
 * \brief       prepare general action information
 *
 * \param[out]  action_info	- converted action information
 *
 * \return      TC_API_FAILURE	- Failed to prepare info
 *              TC_API_OK	- prepare successfully
 */
enum tc_api_rc prepare_general_action_info(struct action_info  *action_info);

/**************************************************************************//**
 * \brief       convert mirred action information from tc action
 *
 * \param[in]   action		- action received by tc manager layer
 * \param[out]  action_info	- converted action information
 *
 * \return      TC_API_FAILURE	- Failed to prepare info
 *				  action is not supported
 *              TC_API_OK	- prepare successfully
 */
enum tc_api_rc prepare_mirred_action_info(struct tc_action    *action,
					  struct action_info  *action_info);

enum tc_api_rc prepare_action_info(struct tc_action *tc_action_params, struct action_info *action_info);

enum tc_api_rc delete_old_filter_actions(uint32_t old_num_of_actions,
					 struct action_id *old_action_id_array,
					 uint32_t new_num_of_actions,
					 struct action_info *new_action_info_array);

enum tc_api_rc delete_and_free_action_entries(uint32_t num_of_actions, struct action_id *action_info);

enum tc_api_rc delete_and_free_action_info_entries(uint32_t num_of_actions, struct action_info *action_info);

enum tc_api_rc add_tc_action_to_nps_table(struct action_info *action_info);

enum tc_api_rc delete_tc_action_from_nps(struct action_info *action_info);

enum tc_api_rc modify_tc_action_on_nps(struct action_info *action_info);

enum tc_api_rc get_last_used_action_timestamp(uint32_t  action_index,
				       uint32_t  *last_used_timestamp);

enum tc_api_rc tc_unbind_action_from_filter(struct tc_action *tc_action_params);

enum tc_api_rc tc_int_add_action(struct tc_action *tc_action_params,
				 struct action_info *action_info,
				 bool independent_action);

#endif /* _TC_FLOWER_API_H_ */
