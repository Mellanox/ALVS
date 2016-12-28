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
*	file - cli_common.h
*	description - contains definitions for cli_common.c
 */

#ifndef _CLI_COMMON_H_
#define _CLI_COMMON_H_

#include "cli_defs.h"



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
		      int              socket_fd);


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
		     int              socket_fd);



/******************************************************************************
 * \brief    Validate CLI message header
 *
 * \param[in]   cli		- pointer to CLI message
 * \param[in]   msg_type	- Request / Response
 *
 * \return   enum cli_error_code
 */
enum cli_error_code validate_message(struct cli_msg    *cli,
				     enum cli_msg_type  msg_type);



#endif /* ndef _CLI_COMMON_H_ */
