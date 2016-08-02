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

#ifndef _ALVS_DB_H_
#define _ALVS_DB_H_

#include <stdbool.h>
#include <netinet/in.h>
#include <linux/ip_vs.h>
#include "alvs_search_defs.h"


enum alvs_db_rc {
	ALVS_DB_OK,
	ALVS_DB_FAILURE,
	ALVS_DB_NOT_SUPPORTED,
	ALVS_DB_INTERNAL_ERROR,
	ALVS_DB_NPS_ERROR,
	ALVS_DB_FATAL_ERROR,
};

char *my_inet_ntoa(in_addr_t ip);

/**************************************************************************//**
 * \brief       Initialize internal DB
 *
 * \return      success, failure or fatal error.
 */
enum alvs_db_rc alvs_db_init(bool *cancel_application_flag);

/**************************************************************************//**
 * \brief       Destroy internal DB
 *
 * \return      bool - success or failure
 */
void alvs_db_destroy(void);

/**************************************************************************//**
 * \brief       Add a service
 *
 * \param[in]   service   - reference to ipvs service
 *
 * \return      success, failure or fatal error.
 */
enum alvs_db_rc alvs_db_add_service(struct ip_vs_service_user *ip_vs_service);

/**************************************************************************//**
 * \brief       Modify a service
 *
 * \param[in]   service   - reference to ipvs service
 *
 * \return      success, failure or fatal error.
 */
enum alvs_db_rc alvs_db_modify_service(struct ip_vs_service_user *ip_vs_service);

/**************************************************************************//**
 * \brief       Delete a service
 *
 * \param[in]   service   - reference to ipvs service
 *
 * \return      success, failure or fatal error.
 */
enum alvs_db_rc alvs_db_delete_service(struct ip_vs_service_user *ip_vs_service);

/**************************************************************************//**
 * \brief       Retrieve service flags
 *
 * \param[in/out] service   - reference to ipvs service
 *
 * \return        success, failure or fatal error.
 */
enum alvs_db_rc alvs_db_get_service_flags(struct ip_vs_service_user *ip_vs_service);

/**************************************************************************//**
 * \brief       Add a server to a service
 *
 * \param[in]   service   - reference to service
 * \param[in]   server    - reference to server
 * \param[in]   info      - reference to server information
 *
 * \return      success, failure or fatal error.
 */
enum alvs_db_rc alvs_db_add_server(struct ip_vs_service_user *ip_vs_service,
				   struct ip_vs_dest_user *ip_vs_dest);

/**************************************************************************//**
 * \brief       Modify a server in a service
 *
 * \param[in]   service   - reference to service
 * \param[in]   server    - reference to server
 * \param[in]   info      - reference to server information
 *
 * \return      success, failure or fatal error.
 */
enum alvs_db_rc alvs_db_modify_server(struct ip_vs_service_user *ip_vs_service,
				      struct ip_vs_dest_user *ip_vs_dest);

/**************************************************************************//**
 * \brief       Delete a server from a service
 *
 * \param[in]   service   - reference to service
 * \param[in]   server    - reference to server
 * \param[in]   info      - reference to server information
 *
 * \return      success, failure or fatal error.
 */
enum alvs_db_rc alvs_db_delete_server(struct ip_vs_service_user *ip_vs_service,
				      struct ip_vs_dest_user *ip_vs_dest);

/**************************************************************************//**
 * \brief       Delete all services and servers
 *
 * \return      success, failure or fatal error.
 */
enum alvs_db_rc alvs_db_clear(void);

#endif /* _ALVS_DB_H_ */
