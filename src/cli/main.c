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
 *  File:                main.c
 *  Desc:                Main thread configuration.
 */

#include <stdlib.h>
#include <popt.h>

#include "cli.h"
#include "cli_common.h"

/************************************************/
/* Defines                                      */
/************************************************/


/************************************************/
/* Global variables                             */
/************************************************/
static int sockfd;


/************************************************/
/* Functions                                    */
/************************************************/

/******************************************************************************
 * \brief    Print error & errno and exit
 *
 * \param[in]   msg		- error message
 *
 * \return   void
 */
void error(char *msg)
{
	perror(msg);
	exit(0);
}



/******************************************************************************
 * \brief    Print CLI message
 *
 * \param[in]   cli		- pointer to CLI message
 *
 * \return   void
 */
void print_message(struct cli_msg  *cli __unused)
{
	uint32_t *raw_data  __unused;
	uint32_t idx;

	print_dbg("Message header:\n");
	print_dbg("  version:  %d\n", cli->header.version);
	print_dbg("  msg_type: %d\n", cli->header.msg_type);
	print_dbg("  op:       %d\n", cli->header.op);
	print_dbg("  family:   %d\n", cli->header.family);
	print_dbg("  len:      %d\n", cli->header.len);

	raw_data = (uint32_t *)&cli->header;
	print_dbg("Header  0x%08x %08x %08x %08x\n\n",
	       raw_data[0], raw_data[1], raw_data[2], raw_data[3]);


	print_dbg("Message payload:");

	raw_data = (uint32_t *)cli->payload.raw_data;
	for (idx = 0; idx < (CLI_PAYLOAD_SIZE/4); idx++) {
		if ((idx % 8) == 0) {
			print_dbg("\n  0x%08x", raw_data[idx]);
		} else {
			print_dbg("  %08x",   raw_data[idx]);
		}
	}
	print_dbg("\n");
}

/******************************************************************************
 * \brief    Initialize CLI client socket
 *           Define Unix socket and connect it
 *
 * \return   void
 */
void init_socket(void)
{
	struct sockaddr_un  serv_addr;
	uint32_t            servlen;
	int                 rc;

	/* init variables */
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;
	strncpy(serv_addr.sun_path, CLI_SOCKET_PATH, sizeof(CLI_SOCKET_PATH));
	servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);

	/* Create the socket */
	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0) {
		error("Creating socket");
	}

	/* Connect to socket */
	rc = connect(sockfd, (struct sockaddr *) &serv_addr, servlen);
	if (rc < 0) {
		error("Connecting");
	}
}



/******************************************************************************
 * \brief    Main function
 *
 */
int main(int argc, char *argv[])
{
	struct cli_msg	 cli;
	struct cli_msg	 rcv_cli;
	int32_t		 rcv_len;
	int32_t		 written_len;
	bool		 rc;


	/************************************************/
	/* Parse arguments                              */
	/************************************************/
	parse_arguments(argc, argv, &cli);


	/************************************************/
	/* Init                                         */
	/************************************************/
	init_socket();


	/************************************************/
	/* Send message/request to daemon               */
	/************************************************/
	print_message(&cli);
	written_len = write_message(&cli, sockfd);
	if (written_len < 0) {
		error("write_message failed");
	}

	/************************************************/
	/* Read response                                */
	/************************************************/
READ_MESSAGE:
	/* read header */
	rcv_len = read_message((uint8_t *)&rcv_cli.header,
			       sockfd,
			       sizeof(rcv_cli.header));
	print_dbg("Header read. rcv_len %d\n", rcv_len);
	if (rcv_len < 0) {
		error("read header failed");
	}

	/* read payload */
	rcv_len = read_message((uint8_t *)&rcv_cli.payload,
			       sockfd,
			       rcv_cli.header.len);
	print_dbg("Payload read. rcv_len %d\n", rcv_len);
	if (rcv_len < 0) {
		error("read payload failed");
	}


	/************************************************/
	/* handle response                              */
	/************************************************/
	print_message(&rcv_cli);
	rc = handle_response(&rcv_cli);
	if (rc == false) {
		error("handle response failed");
	}

	/************************************************/
	/* check if need to read more messages          */
	/************************************************/
	if (rcv_cli.header.is_last == 0) {
		/* read next message */
		print_dbg("is_last = %d. reading next message\n",
			  rcv_cli.header.is_last);
		goto READ_MESSAGE;
	}

	/* finish successfully */
	return 0;
}
