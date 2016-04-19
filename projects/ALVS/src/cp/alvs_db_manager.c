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
*  Project:	 	NPS400 ALVS application
*  File:		alvs_db_manager.c
*  Desc:		Performs the main process of ALVS DB management
*
****************************************************************************/

/* linux includes */
#include <stdio.h>
#include <string.h>
/* libnl-3 includes */
//#include <netlink/netlink.h>
//#include <netlink/cache.h>
//#include <netlink/route/neighbour.h>
//#include <netlink/route/route.h>

/* Project includes */
#include "alvs_db_manager.h"
#include "search.h"


void alvs_db_manager_table_init(void);
void alvs_db_manager_classification_table_init(void);

/******************************************************************************
 * \brief	  Main ALVS process
 *
 * \return	  void
 */
void alvs_db_manager_process()
{
//	printf("alvs_db_manager_init ... \n");
//	nw_db_manager_init();
	printf("alvs_db_manager_table_init ... \n");
	alvs_db_manager_table_init();
}

/******************************************************************************
 * \brief	  Delete
 *
 * \return	  void
 */
void alvs_db_manager_delete()
{
}

/******************************************************************************
 * \brief	  Initialization of all tables handled by the DB manager
 *
 * \return	  void
 */
void alvs_db_manager_table_init()
{
	alvs_db_manager_classification_table_init();
}
/******************************************************************************
 * \brief	  Initialization of classification table
 *
 * \return	  void
 */
void alvs_db_manager_classification_table_init()
{
	struct alvs_service_key key;
	struct alvs_service_result result;
	bool ret_code;

	key.service_address = 0xc86b890a; //10.137.107.200
	key.service_port = 80;
	key.service_protocol = IPPROTO_TCP;
	memset(&result, 0, sizeof(struct  alvs_service_result));
	result.real_server_ip = 0x066b890a; //10.137.107.6
	printf("Add entry to classification table service_address = 10.137.107.200 service_port = 80 service_protocol = 6 result = 10.137.107.6\n");
	ret_code = add_classification_entry(&key, &result);
	if(!ret_code){
		printf("Error - cannot add entry to classification table service_address = 10.137.107.200 service_port = 80 service_protocol = 6 result = 10.137.107.6\n");
	}
}
