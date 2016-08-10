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
*  File:                infrastructure.h
*  Desc:                Infrastructure API.
*
*/

#ifndef _NW_DB_H_
#define _NW_DB_H_

#include <stdbool.h>
#include <netinet/in.h>
#include <linux/ip_vs.h>
#include <netlink/route/route.h>
#include "nw_search_defs.h"

enum nw_db_rc {
	NW_DB_OK,
	NW_DB_FAILURE,
	NW_DB_NOT_SUPPORTED,
	NW_DB_INTERNAL_ERROR,
	NW_DB_NPS_ERROR,
	NW_DB_FATAL_ERROR,
};

/**************************************************************************//**
 * \brief       Initialize internal DB
 *
 * \return      success, failure or fatal error.
 */
enum nw_db_rc nw_db_init(void);

/**************************************************************************//**
 * \brief       Destroy internal DB
 *
 * \return      bool - success or failure
 */
void nw_db_destroy(void);

/**************************************************************************//**
 * \brief       Add a fib_entry to NW DB
 *
 * \param[in]   route_entry   - reference to route entry
 *
 * \return      NW_DB_OK - fib entry entered successfully
 *              NW_DB_INTERNAL_ERROR - failed to communicate with DB
 *              NW_DB_NPS_ERROR - failed to communicate with NPS
 */
enum nw_db_rc nw_db_add_fib_entry(struct rtnl_route *route_entry, bool reorder);

/**************************************************************************//**
 * \brief       Add a fib_entry to internal DB
 *
 * \param[in]   fib_entry   - reference to service
 *
 * \return      NW_DB_OK - service exists (info is filled with fib entry
 *                           information if it not NULL).
 *              NW_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum nw_db_rc nw_db_delete_fib_entry(struct rtnl_route *route_entry);

/**************************************************************************//**
 * \brief       Modify a fib entry in NW DB
 *
 * \param[in]   route_entry   - reference to route entry
 *
 * \return      NW_DB_OK - fib entry modified successfully
 *              NW_DB_INTERNAL_ERROR - failed to communicate with DB
 *              NW_DB_NPS_ERROR - failed to communicate with NPS
 *              NW_DB_FAILURE - fib entry do not exist
 */
enum nw_db_rc nw_db_modify_fib_entry(struct rtnl_route *route_entry);

#endif /* _NW_DB_H_ */
