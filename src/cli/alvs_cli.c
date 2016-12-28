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
 *  File:                alvs_cli.c
 *  Desc:                implementation of ALVS CLI application function needed
*                        by main.c
 */

#include <stdlib.h>
#include <popt.h>

#include "alvs_cli_defs.h"
#include "cli.h"
#include "cli_common.h"


/************************************************/
/* Defines                                      */
/************************************************/
/* defines for popt */
# define NO_POPT_ARGS			NULL
# define NO_POPT_ARGS_INFO		0
# define NO_POPT_DESC			NULL

/*
 * command value when using poptGetContext
 */
enum cli_parsing_val {
	CLI_GET_VERSION_VAL		=  0x01,
	CLI_EXIT_VAL,			/* 0x02 */
};


/************************************************/
/* Functions                                    */
/************************************************/

/******************************************************************************
 * \brief    Print CLI message format number
 *
 * \return   void
 */
static void print_format_version(void)
{
	printf("%s (CLI) format version: %03d\n", ANL_CLI_APP_NAME, CLI_FORMAT_VERSION);
}



/******************************************************************************
 * \brief    parse main function argument and set CLI message accordingly
 *
 * \param[in]   argc		- argc given by main function
 * \param[in]   argv		- argv given by main function
 * \param[out]  cli		- pointer to CLI message buffer
 *
 * \return   void
 */
void parse_arguments(int		 argc,
		     char		*argv[],
		     struct cli_msg	*cli)
{
	print_dbg("parsing arguments\n");

	int32_t opt;
	poptContext optCon;   /* context for parsing command-line options */

	/************************************************/
	/* Build options structure                      */
	/************************************************/
	/* struct poptOption:
	 *	const char * longName; // may be NULL
	 *	char shortName;        // may be '\0'
	 *	int argInfo;                                                       (use NO_POPT_ARGS_INFO
	 *	void * arg;            // depends on argInfo
	 *	int val;               // 0 means don't return, just update flag
	 *	char * descrip;        // description for autohelp -- may be NULL  (use NO_POPT_DESC for no description)
	 *	char * argDescrip;     // argument description for autohelp        (use NO_POPT_DESC for no description)
	 */
	struct poptOption options_table[] = {
		/* get version */
		{ "version", '\0', NO_POPT_ARGS_INFO, NO_POPT_ARGS,
			CLI_GET_VERSION_VAL, "Get daemon version", NO_POPT_DESC },

#ifndef NDEBUG
		{ "exit", 'e', NO_POPT_ARGS_INFO, NO_POPT_ARGS,
			CLI_EXIT_VAL, "Get daemon version", NO_POPT_DESC },
#endif

		/* must be the last option */
			POPT_AUTOHELP
		{ NULL, 0, 0, NULL, 0, NO_POPT_DESC, NO_POPT_DESC }
	};

	/************************************************/
	/* Create context from options structure        */
	/************************************************/
	/* TODO: forbid more than one command */
	optCon = poptGetContext(ANL_CLI_APP_NAME, argc, (const char **)argv,
				 options_table, 0);

	if (argc < 2) {
		poptPrintUsage(optCon, stderr, 0);
		exit(1);
	}


	/************************************************/
	/* Handle options (CLI)                         */
	/************************************************/
	opt = poptGetNextOpt(optCon);
	if (opt < 0) {
		poptPrintUsage(optCon, stderr, 0);
		exit(-1);
	}

	/* General command header init */
	memset(cli, 0, sizeof(struct cli_msg));
	cli->header.version  = CLI_FORMAT_VERSION;
	cli->header.len      = 0;
	cli->header.msg_type = REQUEST_MESSAGE;

	switch (opt) {
	case CLI_GET_VERSION_VAL:
		print_dbg("Received GET_VERSION\n");
		cli->header.family = CLI_FAMILY_COMMON;
		cli->header.op     = CLI_OP_TYPE_GET_VERSION;
		break;
#ifndef NDEBUG
	case CLI_EXIT_VAL:
		print_dbg("Received EXIT\n");
		cli->header.family = CLI_FAMILY_COMMON;
		cli->header.op     = CLI_OP_TYPE_EXIT;
		break;
#endif
	default:
		print_dbg("Default option\n");
		poptPrintUsage(optCon, stderr, 0);
		exit(-1);
	}


	poptFreeContext(optCon);
}


/******************************************************************************
 * \brief    Handle CLI message in case of error code different
 *           from NO_ERROR_CODE
 *
 * \param[in]   cli	- pointer to CLI message
 *
 * \return   void
 */
void handle_error_response(struct cli_msg  *cli)
{
	print_dbg("sub-type %d\n", cli->header.op);

	switch (cli->header.err_code) {
	case DIFFERENT_FORMAT_VERSION:
		print_err("Daemon format (%d) is different form app\n",
		       cli->header.version);
		print_format_version();
		break;

	case UNSUPPORTED_CLI_FAMILY:
		print_err("Family type (%d) is not supported by daemon\n",
		       cli->header.family);
		break;

	case UNSUPPORTED_CLI_OP_TYPE:
		print_err("operation type (%d) is not supported by daemon\n",
		       cli->header.op);
		break;

	case PAYLOAD_LENGTH_EXCEED_MAX_SIZE:
		print_err("Payload length exceed max side\n");
		break;

	case RECEIVED_UNEXPECTED_MSG_TYPE:
		print_err("Daemon received unexpected message type\n");
		break;

	default:
		print_err("received unknown error %d\n", cli->header.err_code);
		break;
	}
}


/******************************************************************************
 * \brief    Handle common family response message from daemon
 *
 * \param[in]   cli	- pointer to CLI message
 *
 * \return   true	- success
 *           false	- failure
 */
bool handle_common_response(struct cli_msg  *cli)
{

	print_dbg("sub-type %d\n", cli->header.op);

	switch (cli->header.op) {
	case CLI_OP_TYPE_GET_VERSION:
		/* set NULL at the end of the string for protect print */
		cli->payload.raw_data[cli->header.len] = 0;

		/* print daemon version */
		print_format_version();
		printf("%s daemon version : %s\n",
		       ANL_CLI_APP_NAME, cli->payload.raw_data);
		printf("%s daemon format version: %03d\n",
		       ANL_CLI_APP_NAME, cli->header.version);
		return true;

#ifndef NDEBUG
	case CLI_OP_TYPE_EXIT:
		printf("daemon respond for exit command\n");
		return true;
#endif
	default:
		print_err("received unsupported sub-type command %d\n",
		       cli->header.op);
		return false;
	}

	/* finish successfully */
	return true;
}


/******************************************************************************
 * \brief    Handle response message from daemon
 *
 * \param[in]   cli	- pointer to CLI message
 *
 * \return   true	- success
 *           false	- failure
 */
bool handle_response(struct cli_msg  *cli)
{
	enum cli_error_code  err_code;

	print_dbg("received type %d sub-type %d\n",
		  cli->header.family, cli->header.op);

	err_code = validate_message(cli, RESPONSE_MESSAGE);
	if (err_code != NO_ERROR_CODE) {
		print_err("validate message. failed return error code %d\n", err_code);
		return false;
	}

	if (cli->header.err_code != NO_ERROR_CODE) {
		handle_error_response(cli);
		return true;
	}

	switch (cli->header.family) {
	case CLI_FAMILY_COMMON:
		return handle_common_response(cli);

	default:
		print_err("received unsupported command type %d\n",
		       cli->header.family);
		return false;
	}

	/* finish successfully */
	return true;
}

