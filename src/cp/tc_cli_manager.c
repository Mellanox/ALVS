/***************************************************************************
*
* Copyright (c) 2016 Mellanox Technologies, Ltd. All rights reserved.
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
*  File:                tc_cli_manager.c
*  Desc:                Performs TC handle command given by CLI management
*
****************************************************************************/


#include "log.h"
#include "tc_cli_defs.h"
#include "cli_manager.h"
#include "cli_common.h"
#include "version.h"
#include "tc_api.h"



/******************************************************************************
 * \brief	  Handle common messages
 *
 * \param[in]     rcv_cli    - Received CLI message
 * \param[out]    res_cli    - Response CLI message
 *
 * \return	  void
 */
void cli_manager_handle_get_tc_filter(struct cli_msg  *rcv_cli,
				      struct cli_msg  *res_cli)
{
	struct tc_filter      tc_filter;
	struct tc_filter_id  *filters_array  = NULL;
	enum tc_api_rc        tc_api_rc      = TC_API_OK;
	uint32_t              num_of_filters = 0;
	int32_t               cli_rc         = 0;
	uint32_t              idx;

	write_log(LOG_DEBUG, "ifindex 0x%x, priority %d, parent 0x%x, handdle_id %d",
		  rcv_cli->payload.tc_filter_req.ifindex,
		  rcv_cli->payload.tc_filter_req.priority,
		  rcv_cli->payload.tc_filter_req.parent,
		  rcv_cli->payload.tc_filter_req.handdle_id);


	/* get all filters */
	tc_api_rc = tc_api_get_filters_list(rcv_cli->payload.tc_filter_req.ifindex,
					    rcv_cli->payload.tc_filter_req.priority,
					    filters_array,
					    &num_of_filters);
	if (tc_api_rc != TC_API_OK) {
		goto Exit;
	}

	if (num_of_filters == 0) {
		/* no actions */
		res_cli->header.len = 0;
		return;
	}

	/* get & send filters */
	for (idx = 0 ; idx < num_of_filters; idx++) {

		/* get next filter */
		tc_api_rc = tc_api_get_filter_info(&filters_array[idx],
						   &tc_filter);
		if (tc_api_rc != TC_API_OK) {
			goto Exit;
		}

		/* copy filter to message result */
		memcpy(&res_cli->payload.tc_filter_res,
		       &tc_filter,
		       sizeof(res_cli->payload.tc_filter_res));

		/* send message (iof not last */
		if ((idx + 1) == num_of_filters) {
			/* not last message */
			cli_rc = cli_manager_send_message(res_cli, false);
			if (cli_rc != 0) {
				/* socket error */
				goto Exit;
			}
		}
	}

Exit:
	/* free buffer allocated by tc_api_get_filters_list */
	if (filters_array != NULL) {
		free(filters_array);
	}

	if (cli_rc != 0) {
		/* socket error */
		/* TODO: need to exit without sending more messages */
	}

	if (tc_api_rc != TC_API_OK) {
		res_cli->header.err_code = DB_GET_FAILED;
		res_cli->header.len      = 0;
		return;
	}
}

/******************************************************************************
 * \brief	  Handle common messages
 *
 * \param[in]     rcv_cli    - Received CLI message
 * \param[out]    res_cli    - Response CLI message
 *
 * \return	  void
 */
void cli_manager_handle_get_family_actions(struct cli_msg  *rcv_cli,
					   struct cli_msg  *res_cli)
{
	struct tc_action   tc_action;
	uint32_t          *action_indexes = NULL;
	enum tc_api_rc     tc_api_rc      = TC_API_OK;
	uint32_t           num_of_actions = 0;
	int32_t            cli_rc         = 0;
	uint32_t           idx;
	bool               is_action_exists;

	write_log(LOG_DEBUG, "action_req 0x%x", rcv_cli->payload.tc_action_req);

	/* get all action indexes */
	tc_api_rc = tc_api_get_actions_list(rcv_cli->payload.tc_action_req,
					    action_indexes, &num_of_actions);
	if (tc_api_rc != TC_API_OK) {
		goto Exit;
	}

	if (num_of_actions == 0) {
		/* no actions */
		res_cli->header.len = 0;
		return;
	}

	/* get & send actions */
	for (idx = 0 ; idx < num_of_actions; idx++) {

		/* get next action */
		tc_api_rc = tc_api_get_action_info(rcv_cli->payload.tc_action_req,
						   action_indexes[idx],
						   &tc_action,
						   &is_action_exists);
		if (tc_api_rc != TC_API_OK) {
			goto Exit;
		}

		if (is_action_exists == false) {
			res_cli->header.len = 0;
			return;
		}

		/* copy action to message result */
		memcpy(&res_cli->payload.tc_action_res,
		       &tc_action,
		       sizeof(res_cli->payload.tc_action_res));

		/* send message (iof not last */
		if ((idx + 1) == num_of_actions) {
			/* not last message */
			cli_rc = cli_manager_send_message(res_cli, false);
			if (cli_rc != 0) {
				/* socket error */
				goto Exit;
			}
		}
	}

Exit:
	/* free buffer allocated by tc_api_get_actions_list */
	if (action_indexes != NULL) {
		free(action_indexes);
	}

	if (cli_rc != 0) {
		/* socket error */
		/* TODO: need to exit without sending more messages */
	}

	if (tc_api_rc != TC_API_OK) {
		res_cli->header.err_code = DB_GET_FAILED;
		res_cli->header.len      = 0;
		return;
	}
}


/******************************************************************************
 * \brief	  Handle TC statistic messages
 *
 * \param[in]     rcv_cli    - Received CLI message
 * \param[out]    res_cli    - Response CLI message
 *
 * \return	  void
 */
void cli_manager_handle_tc_stats_message(struct cli_msg  *rcv_cli,
				       struct cli_msg  *res_cli)
{
	/* handle sub-command type */
	switch (rcv_cli->header.op) {
	case CLI_OP_TYPE_GET_FILTER:
		cli_manager_handle_get_tc_filter(rcv_cli, res_cli);
		break;

	case CLI_OP_TYPE_GET_FAMILY_ACTIONS:
		cli_manager_handle_get_family_actions(rcv_cli, res_cli);
		break;

	default:
		res_cli->header.err_code = UNSUPPORTED_CLI_OP_TYPE;
		write_log(LOG_ERR, "Received unsupported sub-type command %d",
			  rcv_cli->header.op);
		break;
	}
}


/******************************************************************************
 * \brief	  Handle common messages
 *
 * \param[in]     rcv_cli    - Received CLI message
 * \param[out]    res_cli    - Response CLI message
 *
 * \return	  void
 */
void cli_manager_handle_common_message(struct cli_msg  *rcv_cli,
				       struct cli_msg  *res_cli)
{
	const char     *version;

	/* handle sub-command type */
	switch (rcv_cli->header.op) {
	case CLI_OP_TYPE_GET_VERSION:
		version = get_version();
		res_cli->header.len = strlen(version);
		strncpy(res_cli->payload.raw_data, version, res_cli->header.len);
		break;

	case CLI_OP_TYPE_EXIT:
		write_log(LOG_INFO, "Received exit command");
		cli_manager_exit();
		break;

	default:
		res_cli->header.err_code = UNSUPPORTED_CLI_OP_TYPE;
		write_log(LOG_ERR, "Received unsupported sub-type command %d",
			  rcv_cli->header.op);
		break;
	}
}


/******************************************************************************
 * \brief	  Handle received message
 *
 * \param[in]     rcv_cli    - Received CLI message
 * \param[out]    res_cli    - Response CLI message
 *
 * \return         void
 */
void cli_manager_handle_message(struct cli_msg  *rcv_cli,
				struct cli_msg  *res_cli)
{
	enum cli_error_code  err_code;

	write_log(LOG_DEBUG, "Received type %d sub-type %d",
		  rcv_cli->header.family, rcv_cli->header.op);

	/* set common response header */
	res_cli->header.version  = CLI_FORMAT_VERSION;
	res_cli->header.msg_type = RESPONSE_MESSAGE;
	res_cli->header.op       = rcv_cli->header.op;
	res_cli->header.family   = rcv_cli->header.family;
	res_cli->header.err_code = NO_ERROR_CODE;
	res_cli->header.len      = 0;


	/* validate received message */
	err_code = validate_message(rcv_cli, REQUEST_MESSAGE);
	if (err_code != NO_ERROR_CODE) {
		write_log(LOG_NOTICE, "validate_message failed. error code = %d", err_code);
		res_cli->header.err_code = err_code;
		return;
	}


	/* handle command type */
	switch (rcv_cli->header.family) {
	case CLI_FAMILY_COMMON:
		cli_manager_handle_common_message(rcv_cli, res_cli);
		break;

	case CLI_FAMILY_TC_STAT:
		cli_manager_handle_tc_stats_message(rcv_cli, res_cli);
		break;

	default:
		/* handle error message */
		res_cli->header.err_code = UNSUPPORTED_CLI_FAMILY;
		write_log(LOG_ERR, "Received unsupported command type %d",
			  rcv_cli->header.family);
		break;
	}
}
