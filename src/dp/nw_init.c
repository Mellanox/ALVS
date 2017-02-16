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
*  Project:             NPS400 ALVS application
*  File:                nw.h
*  Desc:                init shared cmem
*/
#include <stdint.h>
#include "nw_conf.h"
#include "global_defs.h"
#include "application_search_defs.h"

/******************************************************************************
 * \brief         Initialize nw shared CMEM constructor
 *
 * \return        true/false (success / failed )
 */
bool init_nw_shared_cmem(void)
{
	uint32_t  result;

	/*Init interfaces DB*/
	result = ezdp_init_table_struct_desc(STRUCT_ID_NW_INTERFACES,
					     &shared_cmem_nw.interface_struct_desc,
					     cmem_wa.nw_wa.table_work_area,
					     sizeof(cmem_wa.nw_wa.table_work_area));
	if (result != 0) {
		printf("ezdp_init_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_NW_INTERFACES, result, ezdp_get_err_msg());
		return false;
	}

	result = ezdp_validate_table_struct_desc(&shared_cmem_nw.interface_struct_desc,
						 sizeof(struct nw_if_result));
	if (result != 0) {
		printf("ezdp_validate_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_NW_INTERFACES, result, ezdp_get_err_msg());
		return false;
	}

	/*Init interface addresses DB*/
	result = ezdp_init_table_struct_desc(STRUCT_ID_NW_INTERFACE_ADDRESSES,
					     &shared_cmem_nw.interface_addresses_struct_desc,
					     cmem_wa.nw_wa.table_work_area,
					     sizeof(cmem_wa.nw_wa.table_work_area));
	if (result != 0) {
		printf("ezdp_init_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_NW_INTERFACE_ADDRESSES, result, ezdp_get_err_msg());
		return false;
	}

	result = ezdp_validate_table_struct_desc(&shared_cmem_nw.interface_addresses_struct_desc,
						 sizeof(struct nw_if_addresses_result));
	if (result != 0) {
		printf("ezdp_validate_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_NW_INTERFACE_ADDRESSES, result, ezdp_get_err_msg());
		return false;
	}

	/*Init lag group DB*/
	result = ezdp_init_table_struct_desc(STRUCT_ID_NW_LAG_GROUPS,
					     &shared_cmem_nw.lag_group_info_struct_desc,
					     cmem_wa.nw_wa.table_work_area,
					     sizeof(cmem_wa.nw_wa.table_work_area));
	if (result != 0) {
		printf("ezdp_init_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_NW_LAG_GROUPS, result, ezdp_get_err_msg());
		return false;
	}

	result = ezdp_validate_table_struct_desc(&shared_cmem_nw.lag_group_info_struct_desc,
						 sizeof(struct nw_lag_group_result));
	if (result != 0) {
		printf("ezdp_validate_table_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_NW_LAG_GROUPS, result, ezdp_get_err_msg());
		return false;
	}

	/*Init arp DB*/
	result = ezdp_init_hash_struct_desc(STRUCT_ID_NW_ARP,
					    &shared_cmem_nw.arp_struct_desc,
					    cmem_wa.nw_wa.arp_prm_hash_wa,
					    sizeof(cmem_wa.nw_wa.arp_prm_hash_wa));
	if (result != 0) {
		printf("ezdp_init_hash_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_NW_ARP, result, ezdp_get_err_msg());
		return false;
	}

	result = ezdp_validate_hash_struct_desc(&shared_cmem_nw.arp_struct_desc,
						true,
						sizeof(struct nw_arp_key),
						sizeof(struct nw_arp_result));
	if (result != 0) {
		printf("ezdp_validate_hash_struct_desc of %d struct fail. Error Code %d. Error String %s\n",
		       STRUCT_ID_NW_ARP, result, ezdp_get_err_msg());
		return false;
	}

	return true;
}

