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
*  Project:             NPS400 ALVS application
*  File:                alvs_cli_manager.c
*  Desc:                Performs ALVS handle command given by CLI management
*
****************************************************************************/


#include "log.h"
#include "alvs_cli_defs.h"
#include "cli_manager.h"
#include "cli_common.h"
#include "version.h"



/******************************************************************************
 * \brief	  Handle received common message
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

	write_log(LOG_DEBUG, "sub-type %d", rcv_cli->header.op);

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
 * \return	  void
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

	default:
		/* handle error message */
		res_cli->header.err_code = UNSUPPORTED_CLI_FAMILY;
		write_log(LOG_ERR, "Received unsupported command type %d",
			  rcv_cli->header.family);
		break;
	}
}
