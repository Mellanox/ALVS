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
*  File:                nw_db.h
*  Desc:                Network DB APIs.
*
*/
#ifndef NW_DB_H_
#define NW_DB_H_

#include <stdbool.h>

enum internal_db_table_name {
	ARP_ENTRIES_INTERNAL_DB,
	FIB_ENTRIES_INTERNAL_DB,
	LAG_GROUPS_INTERNAL_DB,
	NW_INTERFACES_INTERNAL_DB,
};

enum nw_db_rc {
	NW_DB_OK,
	NW_DB_FAILURE,
	NW_DB_ERROR,
};

struct nw_db_arp_entry {
	in_addr_t                  entry_ip;
	bool                       is_lag;
	uint32_t                   output_index;	/* lag index or output_interface */
	struct ether_addr          dest_mac_address;
};

struct nw_db_fib_entry {
	in_addr_t                  dest_ip;
	uint16_t                   mask_length;
	enum nw_fib_type           result_type;
	in_addr_t                  next_hop;
	uint16_t                   nps_index;
	bool                       is_lag;
	uint32_t                   output_index;	/* lag index or output_interface */
};

struct nw_db_lag_group_entry {
	uint32_t                   lag_group_id;
	uint8_t                    admin_state;
	struct ether_addr          mac_address;
};

struct nw_db_nw_interface {
	uint32_t                   interface_id;
	uint8_t                    logical_id;
	uint8_t                    admin_state;
	bool                       is_lag;
	uint32_t                   lag_group_id; /* use this value if is_lag is enable */
	struct ether_addr          mac_address;
	uint8_t                    path_type;
	uint8_t                    is_direct_output_lag;
	uint8_t                    direct_output_if;
	struct nw_if_apps          app_bitmap;
	uint8_t                    output_channel;
	uint8_t                    sft_en;
	ezdp_sum_addr_t            stats_base;
	in_addr_t                  local_ip_addr;
};

#define LAG_GROUP_NULL    0xFFFFFFFF
#define LAG_GROUP_DEFAULT 0

/**************************************************************************//**
 * \brief       Create internal DB
 *
 * \param[in]   none
 *
 * \return      false - Failed to create internal DB
 *              true  - Created successfully
 */
bool nw_db_create(void);

/**************************************************************************//**
 * \brief       Destroy internal DB
 *
 * \return      none
 */
void nw_db_destroy(void);

/**************************************************************************//**
 * \brief       Add an entry to internal DB
 *
 * \param[in]   entry_data   - reference to entry data
 *
 * \return      NW_DB_OK - Add to internal DB succeed
 *              NW_DB_ERROR - failed to communicate with DB
 *
 */
enum nw_db_rc internal_db_add_entry(enum internal_db_table_name table_name, void *entry_data);

/**************************************************************************//**
 * \brief       Remove an_entry from internal DB
 *
 * \param[in]   entry_data   - reference to entry data
 *

 * \return      NW_DB_OK - remove from internal db succeed
 *              NW_DB_ERROR - failed to communicate with DB
 */
enum nw_db_rc internal_db_remove_entry(enum internal_db_table_name table_name, void *entry_data);

/**************************************************************************//**
 * \brief       Modify an entry in internal DB
 *
 * \param[in,out]   entry_data   - reference to entry data
 *
 * \return      NW_DB_OK - Modify of internal DB succeed
 *              NW_DB_ERROR - failed to communicate with DB
 */
enum nw_db_rc internal_db_modify_entry(enum internal_db_table_name table_name, void *entry_data);

/**************************************************************************//**
 * \brief       Searches an entry in internal DB
 *
 * \param[in,out]   entry_data   - reference to entry data
 *
 * \return      NW_DB_OK - entry found
 *              NW_DB_FAILURE - entry not found
 *              NW_DB_ERROR - failed to communicate with DB
 */
enum nw_db_rc internal_db_get_entry(enum internal_db_table_name table_name, void *entry_data);

#endif /* NW_DB_H_ */
