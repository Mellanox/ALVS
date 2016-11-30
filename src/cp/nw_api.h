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
*  File:                nw_api.h
*  Desc:                Network API.
*
*/

#ifndef _NW_API_H_
#define _NW_API_H_

#include <stdbool.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "nw_api_defs.h"
#include "nw_search_defs.h"

/**************************************************************************//**
 * \brief       Add a fib_entry to NW DB
 *
 * \param[in]   fib_entry   - reference to fib entry
 *
 * \return      NW_API_OK - fib entry added successfully
 *              NW_API_DB_ERROR - failed to communicate with DB
 *              NW_API_FAILURE - entry already exists
 */
enum nw_api_rc nw_api_add_fib_entry(struct nw_api_fib_entry *fib_entry);

/**************************************************************************//**
 * \brief       Remove fib_entry from NW DB
 *
 * \param[in]   fib_entry   - reference to entry
 *
 * \return      NW_API_OK - fib entry removed successfully
 *              NW_API_DB_ERROR - failed to communicate with DB
 *              NW_API_FAILURE - entry does not exist
 */
enum nw_api_rc nw_api_remove_fib_entry(struct nw_api_fib_entry *fib_entry);

/**************************************************************************//**
 * \brief       Modify fib entry in NW DB
 *
 * \param[in]   fib_entry   - reference to entry
 *
 * \return      NW_API_OK - fib entry modified successfully
 *              NW_API_DB_ERROR - failed to communicate with DB
 *              NW_API_FAILURE - entry does not exist
 */
enum nw_api_rc nw_api_modify_fib_entry(struct nw_api_fib_entry *fib_entry);

/**************************************************************************//**
 * \brief       Add arp entry to NW DB
 *
 * \param[in]   arp_entry   - reference to arp entry
 *
 * \return      NW_API_OK - arp entry added successfully
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc nw_api_add_arp_entry(struct nw_api_arp_entry *arp_entry);

/**************************************************************************//**
 * \brief       Modify arp entry in NW DB
 *
 * \param[in]   arp_entry   - reference to arp entry
 *
 * \return      NW_API_OK - arp entry added successfully
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc nw_api_modify_arp_entry(struct nw_api_arp_entry *arp_entry);

/**************************************************************************//**
 * \brief       Remove arp entry from NW DB
 *
 * \param[in]   arp_entry   - reference to arp entry
 *
 * \return      NW_API_OK - arp entry removed successfully
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc nw_api_remove_arp_entry(struct nw_api_arp_entry *arp_entry);

/**************************************************************************//**
 * \brief       Enable IF entry
 *
 * \param[in]   if_entry   - reference to IF entry
 *
 * \return      NW_API_OK - IF entry enabled successfully
 *              NW_API_FAILURE - IF entry does not exist
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc nw_api_enable_if_entry(struct nw_api_if_entry *if_entry);

/**************************************************************************//**
 * \brief       Disable IF entry
 *
 * \param[in]   if_entry   - reference to IF entry
 *
 * \return      NW_API_OK - IF entry disabled successfully
 *              NW_API_FAILURE - IF entry does not exist
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc nw_api_disable_if_entry(struct nw_api_if_entry *if_entry);

/**************************************************************************//**
 * \brief       Modify IF entry - For disabled IF only.
 *
 * \param[in]   if_entry   - reference to IF entry
 *
 * \return      NW_API_OK - IF entry Modified successfully
 *              NW_API_FAILURE - IF entry does not exist
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc nw_api_modify_if_entry(struct nw_api_if_entry *if_entry);

/**************************************************************************//**
 * \brief       Add local address entry to NW DB
 *
 * \param[in]   local_addr_entry   - reference to local addr entry
 *
 * \return      NW_API_OK - addr entry added successfully
 *              NW_API_DB_ERROR - failed to communicate with DB
 *              NW_API_FAILURE - addr entry does not exist
 */
enum nw_api_rc nw_api_add_local_addr_entry(struct nw_api_local_addr_entry *local_addr_entry);

/**************************************************************************//**
 * \brief       Remove local address entry to NW DB
 *
 * \param[in]   local_addr_entry   - reference to local addr entry
 *
 * \return      NW_API_OK - addr entry removed successfully
 *              NW_API_DB_ERROR - failed to communicate with DB
 *              NW_API_FAILURE - addr entry does not exist
 */
enum nw_api_rc nw_api_remove_local_addr_entry(struct nw_api_local_addr_entry *local_addr_entry);

/**************************************************************************//**
 * \brief       print all interfaces statistics
 *
 * \return	NW_API_OK - - operation succeeded
 *		NW_API_DB_ERROR - fail to read statistics
 */

enum nw_api_rc nw_api_print_if_stats(void);

#endif /* _NW_API_H_ */
