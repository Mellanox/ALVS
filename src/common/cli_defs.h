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
*	file - cli_defs.h
*	description - common defines used by CLI application and daemon
*/

#ifndef _CLI_DEFS_H_
#define _CLI_DEFS_H_


/************************************************/
/* includes                                     */
/************************************************/
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include "stdbool.h"
#include <unistd.h>
#include <stdint.h>
#include <ezdp_defs.h>

/************************************************/
/* Defines                                      */
/************************************************/

/* application name */
#if defined(CONFIG_ALVS)
#	define ANL_APP_NAME "alvs"
#elif defined(CONFIG_TC)
#	define ANL_APP_NAME "atc"
#else
#error ANL_APP_NAME not defined
#endif

#define ANL_CLI_APP_NAME		ANL_APP_NAME

/* folder of the socket */
#define CLI_SOCKET_ROOT		"/tmp"

/* Path to socket */
#define CLI_SOCKET_PATH		CLI_SOCKET_ROOT "/" ANL_CLI_APP_NAME

/* Message buffer max length */
#define CLI_MSG_SIZE		1024

#define CLI_FORMAT_VERSION	1


/************************************************/
/* Enums                                        */
/************************************************/
enum cli_error_code {
	NO_ERROR_CODE			=  0,
	DIFFERENT_FORMAT_VERSION,	/* 1 */
	UNSUPPORTED_CLI_FAMILY,		/* 2 */
	UNSUPPORTED_CLI_OP_TYPE,	/* 3 */
	PAYLOAD_LENGTH_EXCEED_MAX_SIZE,	/* 4 */
	RECEIVED_UNEXPECTED_MSG_TYPE,	/* 5 */
};

enum cli_msg_type {
	REQUEST_MESSAGE,		/* 0 */
	RESPONSE_MESSAGE		/* 1 */
};


/************************************************/
/* Structures                                   */
/************************************************/

/******************************************************************************
 *	Define command message header structure.
 *
 */
struct cli_msg_header {
	uint16_t	op;		/* bits 16-31 */
	uint8_t		family;		/* bits  8-15 */
	uint8_t		version;	/* bits  0-7  */

	uint8_t		msg_type;	/* bits 56-63 (only one bit is used) */
	uint8_t		err_code;	/* bits 48-55 */
	uint16_t	len;		/* bits 31-47 */

	unsigned	/* reserved */	: 32; /* bits 64-95 */

	unsigned	/* reserved */	: 32; /* bits 96-127 */
};
CASSERT(sizeof(struct cli_msg_header) == (16));


/******************************************************************************
 *	General payload data used for message
 *	each command define it own payload usage
 */
union cli_msg_payload {
	char		raw_data[CLI_MSG_SIZE - sizeof(struct cli_msg_header)];

	/* Future: place for all specific message payload */
};
#define CLI_PAYLOAD_SIZE  (CLI_MSG_SIZE - sizeof(struct cli_msg_header))
CASSERT(sizeof(union cli_msg_payload) == CLI_PAYLOAD_SIZE);


/******************************************************************************
 *	Command message structure
 *	Structure used for:
 *	    1. sending commands to Daemon
 *	    2. Receiving response from Daemon
 */
struct cli_msg {
	struct cli_msg_header	header;
	union cli_msg_payload	payload;
};
CASSERT(sizeof(struct cli_msg) == (CLI_MSG_SIZE));


#endif /* _CLI_DEFS_H_ */
