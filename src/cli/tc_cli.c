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
 *  File:                tc_cli.c
 *  Desc:                implementation of TC CLI application function needed
*                        by main.c
 */

#include <stdlib.h>
#include <net/if.h>
#include <popt.h>
#include <errno.h>

#include "tc_cli_defs.h"
#include "cli.h"
#include "cli_common.h"
#include "tc_common_utils.h"

/************************************************/
/* Defines                                      */
/************************************************/
/* defines for popt */
# define NO_POPT_LONG_NAME		NULL
# define NO_POPT_SHORT_NAME		'\0'
# define NO_POPT_ARGS_INFO		0
# define NO_POPT_ARGS			NULL
# define NO_POPT_DESC			NULL

/*
 * command value when using poptGetContext
 */
enum cli_parsing_val {
	CLI_GET_VERSION_VAL		=  0x01,
	CLI_EXIT_VAL,			/* 0x02 */
	CLI_GET_TC_STATS,		/* 0x03 */
};


/************************************************/
/* Extern functions                             */
/************************************************/
const char *ll_proto_n2a(unsigned short id, char *buf, int len);


/************************************************/
/* Global variables                             */
/************************************************/
/*
 * atc -s filter show dev <dev name> parent ffff: [priority <priority ID>]
 * atc -s actions ls action <action type>
*/
static const char * const filter_show_cmd[] = {
	"filter",	/* 0 */
	"show",		/* 1 */
	"dev",		/* 2 */
	"<dev name>",	/* 3: take value */
	"parent",	/* 4 */
	"ffff:",	/* 5 */
	"priority",	/* 6: optional   */
	"<priority ID>"	/* 7: take value */
};
static const char * const action_ls[] = {
	"actions",	/* 0 */
	"ls",		/* 1 */
	"action",	/* 2 */
	"<action type>" /* 3: take value */
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
 * \brief    TODO
 *
 * \param[in]   TODO argc		- argc given by main function
 * \param[in]   TODO argv		- argv given by main function
 * \param[out]  cli		- pointer to CLI message buffer
 *
 * \return   void
 */
void parse_get_tc_filter_stat_arguments(poptContext     *optCon,
					struct cli_msg  *cli)
{
	int       l_errno __unused;
	uint32_t  ifindex;
	uint32_t  priority;
	const char *optarg = NULL;
	uint32_t idx;


	for (idx = 1; idx < 8; idx++) {
		optarg = poptGetArg(*optCon);

		/* check if priority defined */
		if ((idx == 6) && (optarg == NULL)) {
			/* no priority defined            */
			/* define default & exit for loop */
			priority = 0;
			break;
		}

		if (optarg == NULL) {
			print_err("missing arguments\n");
			exit(-1);
		}
		print_dbg("gets: %s\n", optarg);

		/* take dev name */
		if (idx == 3) {
			ifindex = if_nametoindex(optarg);
			/* check for error */
			if (ifindex == 0) {
				l_errno = errno;
				print_err("unknow dev name %s\n", optarg);
				print_dbg("if_nametoindex(%s) failed. errno %d: %s\n",
					  optarg, l_errno, strerror(l_errno));
				exit(-1);
			}
			continue;
		}


		/* take priority ID */
		if (idx == 7) {
			/* TODO: handle error */
			priority = atoi(optarg);
			continue;
		}

		if (strcmp(optarg, filter_show_cmd[idx])) {
			print_err("unexpected parameter %s. expected: %s\n",
				  optarg, filter_show_cmd[idx]);
		}
	}

	/* check if there is unexpected argument */
	optarg = poptGetArg(*optCon);
	if (optarg != NULL) {
		print_err("unexpected parameter %s.\n", optarg);
		exit(-1);
	}



	/* header */
	cli->header.family = CLI_FAMILY_TC_STAT;
	cli->header.op     = CLI_OP_TYPE_GET_FILTER;
	cli->header.len    = sizeof(cli->payload.tc_filter_req);
	/* payload */
	cli->payload.tc_filter_req.ifindex    = ifindex;
	cli->payload.tc_filter_req.priority   = priority;
	cli->payload.tc_filter_req.parent     = TC_INGRESS_PARENT_ID;
	cli->payload.tc_filter_req.handdle_id = 0;

}


/******************************************************************************
 * \brief    TODO
 *
 * \param[in]   TODO argc		- argc given by main function
 * \param[in]   TODO argv		- argv given by main function
 * \param[out]  cli		- pointer to CLI message buffer
 *
 * \return   void
 */
void parse_get_tc_actions_stat_arguments(poptContext     *optCon,
					 struct cli_msg  *cli)
{
	const char *optarg = NULL;
	uint32_t idx;
	int rc;
	enum tc_action_type action_type;


	for (idx = 1; idx < 4; idx++) {
		optarg = poptGetArg(*optCon);
		if (optarg == NULL) {
			print_err("missing arguments\n");
			exit(-1);
		}
		print_dbg("gets: %s\n", optarg);
		/* take action type */
		if (idx == 3) {
			rc = tc_get_action_family_type((char *)optarg, &action_type);
			if (rc != 0) {
				print_err("unknown action type %s.\n", optarg);
				exit(-1);
			}
			continue;
		}

		if (strcmp(optarg, action_ls[idx])) {
			print_err("unexpected parameter %s. expected: %s\n",
				  optarg, action_ls[idx]);
		}
	}



	/* header */
	cli->header.family = CLI_FAMILY_TC_STAT;
	cli->header.op     = CLI_OP_TYPE_GET_FAMILY_ACTIONS;
	cli->header.len    = sizeof(cli->payload.tc_action_req);
	/* payload */
	cli->payload.tc_action_req = action_type;

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
void parse_get_tc_stat_arguments(poptContext     *optCon,
				 struct cli_msg  *cli)
{
	const char *optarg = NULL;

	optarg = poptGetArg(*optCon);
	if (optarg == NULL) {
		print_err("missing arguments\n");
		exit(-1);
	}
	print_dbg("gets: %s\n", optarg);

	if (strcmp(optarg, filter_show_cmd[0]) == 0) {
		parse_get_tc_filter_stat_arguments(optCon, cli);
	} else if (strcmp(optarg, action_ls[0]) == 0) {
		parse_get_tc_actions_stat_arguments(optCon, cli);
	} else {
		print_err("unexpected parameter %s\n", optarg);
		poptPrintUsage(*optCon, stderr, 0);
		exit(-1);
	}
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

	int32_t      opt, opt2;
	poptContext  optCon;   /* context for parsing command-line options */

	/************************************************/
	/* Build options structure                      */
	/************************************************/
	/* struct poptOption:
	 *	const char * longName; // may be NULL                              (use NO_POPT_LONG_NAME for no short name)
	 *	char shortName;        // may be '\0'                              (use NO_POPT_SHORT_NAME for no short name)
	 *	int argInfo;                                                       (use NO_POPT_ARGS_INFO for no info)
	 *	void * arg;            // depends on argInfo
	 *	int val;               // 0 means don't return, just update flag
	 *	char * descrip;        // description for autohelp -- may be NULL  (use NO_POPT_DESC for no description)
	 *	char * argDescrip;     // argument description for autohelp        (use NO_POPT_DESC for no description)
	 */
	struct poptOption options_table[] = {
		/* get version */
		{ "version", NO_POPT_SHORT_NAME, NO_POPT_ARGS_INFO, NO_POPT_ARGS,
			CLI_GET_VERSION_VAL, "Get daemon version", NO_POPT_DESC },

		{ NO_POPT_LONG_NAME, 's', NO_POPT_ARGS_INFO, NO_POPT_ARGS,
			CLI_GET_TC_STATS, "show TC statistic",
			"tc -s filter show dev <dev name> parent ffff: [priority <priority numb>\n"
			"tc -s actions ls action <action family>" },

#ifndef NDEBUG
		{ "exit", 'e', NO_POPT_ARGS_INFO, NO_POPT_ARGS,
			CLI_EXIT_VAL, "exit daemon", NO_POPT_DESC },
#endif

		/* must be the last option */
			POPT_AUTOHELP
		{ NULL, 0, 0, NULL, 0, NO_POPT_DESC, NO_POPT_DESC }
	};

	/************************************************/
	/* Create context from options structure        */
	/************************************************/
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
		if (opt < -1) {
			/* an error occurred during option processing */
			print_err("%s: %s\n",
				  poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
				  poptStrerror(opt));
			poptFreeContext(optCon);
		} else {
			poptPrintUsage(optCon, stderr, 0);
		}
		exit(-1);
	}

	/* forbid more than one command */
	opt2 = poptGetNextOpt(optCon);
	if (opt2 != (-1)) {
		/* handle unexpected option */
		if (opt2 >= 0) {
			/* there is more than one option */
			print_err("multiple options specified\n");
		}
		if (opt2 < -1) {
			/* an error occurred during option processing */
			print_err("%s: %s\n",
				  poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
				  poptStrerror(opt));
			poptFreeContext(optCon);
			exit(-1);
		}
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
	case CLI_GET_TC_STATS:
		print_dbg("Received CLI_GET_TC_STATS\n");
		parse_get_tc_stat_arguments(&optCon, cli);
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
	print_dbg("Received error code %d\n", cli->header.err_code);

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

	case DB_GET_FAILED:
		print_err("Daemon failed to read from DB\n");
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
 * \brief    print filter info exclude action info
 *           prints is according to tc show command
 *
 * \param[in]   action	- TC action structure
 *
 * \return   void
 */
void print_filter_info(struct tc_filter *filter)
{
	char tmp_buff[64];

	printf("filter protocol %s pref %u %s\n",
	       ll_proto_n2a(filter->protocol, tmp_buff, sizeof(tmp_buff)),
	       filter->priority,
	       tc_get_filter_type_str(filter->type));
}


/******************************************************************************
 * \brief    print action info includes statistics
 *           prints is according to tc -s (statistic) show command
 * \param[in]   action	- TC action structure
 *
 * \return   void
 */
void print_action_info(struct tc_action *action)
{
	printf("\t");
	printf("%s action %s\n\t",
	       tc_get_action_family_str(action->general.type),
	       tc_get_action_str(action->general.type));
	printf(" index %u ref %u bind %u installed %u sec used %u sec\n\t",
	       action->general.index,
	       action->general.refcnt,
	       action->general.bindcnt,
	       action->action_statistics.created_timestamp,
	       action->action_statistics.use_timestamp);

	printf("Action statistics:\n\t");
	printf("Sent %lu bytes %lu pkt\n\t",
	       action->action_statistics.bytes_statictics,
	       action->action_statistics.packet_statictics);

	printf("\n");
}


/******************************************************************************
 * \brief    Handle TC statistics family response message from daemon
 *
 * \param[in]   cli	- pointer to CLI message
 *
 * \return   true	- success
 *           false	- failure
 */
bool handle_tc_stats_response(struct cli_msg  *cli)
{
	uint32_t idx;

	switch (cli->header.op) {
	case CLI_OP_TYPE_GET_FILTER:
		/* check if filter exist */
		if (cli->header.len != 0) {
			print_filter_info(&cli->payload.tc_filter_res);
			for (idx = 0; idx < cli->payload.tc_filter_res.actions.num_of_actions; idx++) {
				print_action_info(&cli->payload.tc_filter_res.actions.action[idx]);
			}
		} else {
			print_dbg("No filter exist. msg.len=%d\n", cli->header.len);
		}
		return true;

	case CLI_OP_TYPE_GET_FAMILY_ACTIONS:
		/* check if action exist */
		if (cli->header.len != 0) {
			print_action_info(&cli->payload.tc_action_res);
		} else {
			print_dbg("No action exist. msg.len=%d\n", cli->header.len);
		}
		return true;

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

	case CLI_FAMILY_TC_STAT:
		return handle_tc_stats_response(cli);

	default:
		print_err("received unsupported command type %d\n",
		       cli->header.family);
		return false;
	}

	/* finish successfully */
	return true;
}

