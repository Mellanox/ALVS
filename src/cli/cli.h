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
*	file - cli.h
*	description - contains definitions for cli src files
*/

#ifndef _CLI_H_
#define _CLI_H_

#include <stdbool.h>

#include "cli_defs.h"

/************************************************/
/* MACROs                                       */
/************************************************/
#ifdef NDEBUG
#define print_dbg(format, ...)
#else
#define print_dbg(format, ...)  \
	printf("DBG: Line %d %s: "format, __LINE__, __func__, ##__VA_ARGS__)
#endif

#define print_err(format, ...)  \
	printf("ERROR: "format, ##__VA_ARGS__)



/************************************************/
/* APIs used by main                            */
/* Each application must implement those APIs   */
/************************************************/

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
		     struct cli_msg	*cli);


/******************************************************************************
 * \brief    Handle response message from daemon
 *
 * \param[in]   cli	- pointer to CLI message
 *
 * \return   true	- success
 *           false	- failure
 */
bool handle_response(struct cli_msg  *cli);

#endif /* _CLI_H_ */
