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
 *  Project:             NPS400 application
 *  File:                cli_common.c
 *  Desc:                common functionality used by CLI application and daemon
 */

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include "cli_common.h"


/******************************************************************************
 * \brief    Write CLI message to socket
 *
 * \param[in]   cli		- pointer to CLI message
 * \param[in]   socket_fd	- CLI UNIX socket
 *
 * \return   (-1) for error. write errno set
 *           otherwise, number of bytes actually written
 */
int32_t write_message(struct cli_msg  *cli,
		      int              socket_fd)
{
	int32_t  rc;
	int32_t  msg_len;
	int32_t  writen_bytes;
	char    *buff = (char *)cli;

	writen_bytes = 0;
	msg_len      = sizeof(cli->header) + cli->header.len;

	/* Send command trough socket. write  */
	while (writen_bytes < msg_len) {
		rc = write(socket_fd, &(buff[writen_bytes]),
			   msg_len-writen_bytes);
		if (rc == (-1)) {
			return rc;
		}

		writen_bytes += rc;
	}

	/* finish successfully */
	return writen_bytes;
}


/******************************************************************************
 * \brief    Read CLI message from socket
 *
 * \param[in]   cli		- pointer to CLI message
 * \param[in]   socket_fd	- CLI UNIX socket
 *
 * \return   (-1) for error. read errno set
 *           ( 0) end of file
 *           otherwise, number of bytes actually read
 */
int32_t read_message(struct cli_msg  *cli,
		     int              socket_fd)
{
	uint8_t *buff;
	int32_t  total_rcv_len;
	int32_t  rcv_len;

	/* init */
	bzero(cli, sizeof(struct cli_msg));
	total_rcv_len = 0;
	buff          = (uint8_t *)cli;

	/* try to read more */
	do {

		/* read message from socket */
		rcv_len = read(socket_fd, buff, (sizeof(struct cli_msg) - total_rcv_len));
		if (rcv_len == (-1)) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
				return total_rcv_len;
			}

			/* Error reading */
			return rcv_len;
		}

		/* prepare next read */
		buff += rcv_len;

		/* update total read length */
		total_rcv_len += rcv_len;
	} while (rcv_len > 0);


	return total_rcv_len;
}


/******************************************************************************
 * \brief    Validate CLI message header
 *
 * \param[in]   cli		- pointer to CLI message
 * \param[in]   msg_type	- Request / Response
 *
 * \return   enum cli_error_code
 */
enum cli_error_code validate_message(struct cli_msg    *cli,
				     enum cli_msg_type  msg_type)
{
	if (cli->header.version != CLI_FORMAT_VERSION)	{
		return DIFFERENT_FORMAT_VERSION;
	}

	if (cli->header.len > CLI_PAYLOAD_SIZE)	{
		return PAYLOAD_LENGTH_EXCEED_MAX_SIZE;
	}

	if (cli->header.msg_type != msg_type) {
		return RECEIVED_UNEXPECTED_MSG_TYPE;
	}

	/* finish successfully */
	return NO_ERROR_CODE;
}
