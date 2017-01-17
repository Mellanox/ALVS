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
*  Project:             NPS400 application
*  File:                cli_manager.c
*  Desc:                Performs the main process of CLI management
*
****************************************************************************/

#include <signal.h>
#include <errno.h>
#include <pthread.h>

#include "log.h"
#include "cli_manager.h"
#include "cli_common.h"



/************************************************/
/* Defines                                      */
/************************************************/
#define PENDING_SOCKET_CONNECTIONS	10


/************************************************/
/* Global variables                             */
/************************************************/
static int    sockfd    = (-1);
static int    newsockfd = (-1);
static bool  *cli_manager_cancel_application_flag;


/************************************************/
/* Extern functions                             */
/************************************************/
void cli_manager_handle_message(struct cli_msg  *rcv_cli,
				struct cli_msg  *res_cli);

/************************************************/
/* Functions                                    */
/************************************************/

/******************************************************************************
 * \brief    Print CLI message
 *
 * \param[in]   cli		- pointer to CLI message
 *
 * \return   void
 */
static
void cli_manager_print_message(struct cli_msg  *cli)
{
	write_log(LOG_DEBUG, "Message header:");
	write_log(LOG_DEBUG, "  version:  %d", cli->header.version);
	write_log(LOG_DEBUG, "  msg_type: %d", cli->header.msg_type);
	write_log(LOG_DEBUG, "  op:       %d", cli->header.op);
	write_log(LOG_DEBUG, "  family:   %d", cli->header.family);
	write_log(LOG_DEBUG, "  len:      %d", cli->header.len);
}


/******************************************************************************
 * \brief	  Init CLI socket FD
 *
 * \return	  void
 */
static
void cli_manager_init_socket(void)
{
	struct sockaddr_un  serv_addr;
	int32_t             servlen, rc;
	struct timeval      timeout;


	write_log(LOG_DEBUG, "Init CLI socket");

	/* Create socket file descriptor */
	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0) {
		write_log(LOG_CRIT, "creating socket failed. sockfd %d", sockfd);
		cli_manager_exit();
	}

	/* bind socket */
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;
	strcpy(serv_addr.sun_path, CLI_SOCKET_PATH);
	servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);

	rc = bind(sockfd, (struct sockaddr *) &serv_addr, servlen);
	if (rc < 0) {
		write_log(LOG_CRIT, "binding socket failed. rc %d", rc);
		cli_manager_exit();
	}

	/* set receive timeout - needs for accept() */
	timeout.tv_sec  = 1;
	timeout.tv_usec = 0;
	rc = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
			(char *)&timeout, sizeof(timeout));
	if (rc < 0) {
		write_log(LOG_CRIT, "setsockopt failed to set received timeout socket failed. rc %d", rc);
		cli_manager_exit();
	}
}


/******************************************************************************
 * \brief	  perform listen & accept on CLI socket
 *
 * \param[in]     release_old_socket    - True for close old socket
 *                                      - False dont close old socket
 *
 * \return        new socket FD given by accept()
 */
static
int cli_manager_listen_and_accept_socket(void)
{
	/* socket variables */
	struct sockaddr_un cli_addr;
	socklen_t          clilen;
	int32_t            rc;

	/* Go to server mode - Enable the socket to accept connections */
	rc = listen(sockfd, PENDING_SOCKET_CONNECTIONS);
	if (rc < 0) {
		write_log(LOG_CRIT, "listening to cli socket failed");
		cli_manager_exit();
	}

	/* Wait for client */
	clilen = sizeof(cli_addr);
	return accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
}


/******************************************************************************
 * \brief     Initialization function for CLI manager thread
 *
 * \return	  void
 */
static
void cli_manager_init(void)
{
	sigset_t sigs_to_block;

	/* Mask SIGTERM signal for this thread. */
	sigemptyset(&sigs_to_block);
	sigaddset(&sigs_to_block, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &sigs_to_block, NULL);

	cli_manager_init_socket();
}


/******************************************************************************
 * \brief	  Poll on CLI socket & process all received messages
 *
 * \return	  void
 */
static
void cli_manager_poll(void)
{
	write_log(LOG_DEBUG, "CLI thread started");

	struct cli_msg	 rcv_cli;
	struct cli_msg	 res_cli;
	int32_t          rcv_cli_len;
	int32_t          written_len;
	int32_t          rc;
	struct timeval   timeout;
	bool             reconnect;
	int32_t          l_errno;

	/************************************************/
	/* Init                                         */
	/************************************************/
	/* socket timeout definition */
	timeout.tv_sec  = 0;
	timeout.tv_usec = 100;

	reconnect = false;

	/************************************************/
	/* Connect socket                               */
	/* Wait for client connection / timeout         */
	/* Receive & handle messages                    */
	/************************************************/
	while (*cli_manager_cancel_application_flag == false) {

		/* check if need to reconnect socket */
		if ((newsockfd < 0) || (reconnect)) {

			/* Release socket if needed */
			if (reconnect) {
				close(newsockfd);
			}
			reconnect = false;

			newsockfd = cli_manager_listen_and_accept_socket();
			if (newsockfd < 0) {
				l_errno = errno;
				if (errno == EAGAIN) {
					continue;
				} else {
					write_log(LOG_CRIT, "accept failed. errno  %d", l_errno);
					cli_manager_exit();
				}
			}

			/* Setting timeout for accepted socket FD */
			rc = setsockopt(newsockfd, SOL_SOCKET, SO_RCVTIMEO,
					(char *)&timeout, sizeof(timeout));
			if (rc < 0) {
				write_log(LOG_CRIT, "setsockopt failed to set received timeout. rc %d", rc);
				continue;
			}
		}


		/************************************************/
		/* Read response                                */
		/************************************************/
		write_log(LOG_DEBUG, "New message received. Reading message");
		/* handle different version of message */
		rcv_cli_len = read_message(&rcv_cli, newsockfd);

		/* save errno. errno can overide by other system operation */
		l_errno = errno;

		write_log(LOG_DEBUG, "Received length is %d", rcv_cli_len);
		if (rcv_cli_len == 0) {
			write_log(LOG_DEBUG, "Received zero length");
			reconnect = true;
			continue;
		}

		if (rcv_cli_len < 0) {
			/* If we got interrupted by signal, Just try again.
			 *  Otherwise, assume client disconnected on us.
			 *  and go back to waiting for another connection
			 */

			write_log(LOG_DEBUG, "read_message failed. errno %d", l_errno);

			if (l_errno != EINTR) {
				/* Reconnect & go back to listening */
				write_log(LOG_NOTICE, "read_message failed with EINTR. reconnecting socket");
				reconnect = true;
			}

			continue;
		}
		cli_manager_print_message(&rcv_cli);


		/************************************************/
		/* Handle received message                      */
		/************************************************/
		cli_manager_handle_message(&rcv_cli, &res_cli);


		/************************************************/
		/* Send message                                 */
		/************************************************/
		write_log(LOG_DEBUG, "Sending response");
		cli_manager_print_message(&res_cli);
		written_len = write_message(&res_cli, newsockfd);
		if (written_len < 0) {
			/* handle same as handling error while reading */
			if (errno != EINTR) {
				/* Reconnect & go back to listening */
				write_log(LOG_NOTICE, "write_message failed with EINTR. reconnecting socket");
				reconnect = true;
				continue;
			}

			write_log(LOG_NOTICE, "write_message failed");
			continue;
		}

		write_log(LOG_DEBUG, "message written to socket");
	}
}


/******************************************************************************
 * \brief    TODO
 *
 * \return   void
 */
static
void cli_manager_delete(void)
{
	/* Close sockets */
	if (sockfd >= 0) {
		close(sockfd);
	}
	if (newsockfd >= 0) {
		close(newsockfd);
	}
	unlink(CLI_SOCKET_PATH);
}


/******************************************************************************
 * \brief         CLI thread main application.
 *
 * \return        void
 */
void cli_manager_main(bool *cancel_application_flag)
{
	cli_manager_cancel_application_flag = cancel_application_flag;

	write_log(LOG_DEBUG, "cli_manager_init...");
	cli_manager_init();

	write_log(LOG_INFO, "cli_manager_main_poll...");
	cli_manager_poll();

	write_log(LOG_DEBUG, "Exiting CLI thread...");
	cli_manager_delete();
}


/******************************************************************************
 * \brief    Raises SIGTERM signal to main thread and exits the thread.
 *
 * \return   void
 */
void cli_manager_exit(void)
{
	/* Set global exit variable */
	*cli_manager_cancel_application_flag = true;

	cli_manager_delete();

	/* Exit with temination signal */
	kill(getpid(), SIGTERM);

	/* Terminate calling thread.  */
	pthread_exit(NULL);
}
