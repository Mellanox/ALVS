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
#include <netlink/route/neighbour.h> /* todo */
#include <arpa/inet.h>

#include "nw_api_defs.h"
#include "nw_search_defs.h"

enum internal_db_table_name {
	ARP_ENTRIES_INTERNAL_DB,
	FIB_ENTRIES_INTERNAL_DB,
	LAG_GROUPS_INTERNAL_DB,
	NW_INTERFACES_INTERNAL_DB
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
};
struct nw_db_lag_group_entry {
	uint32_t                   lag_group_id;
	uint8_t                    admin_state;
	struct ether_addr          mac_address;
};

struct nw_db_nw_interface {
	uint32_t                   interface_id;
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
};

#define LAG_GROUP_NULL 0xFFFFFFFF

enum nw_api_rc internal_db_add_entry(enum internal_db_table_name table_name, void *entry_data);

enum nw_api_rc nw_db_add_arp_entry(struct rtnl_neigh *neighbor);
enum nw_api_rc nw_db_remove_arp_entry(struct rtnl_neigh *neighbor);
enum nw_api_rc nw_db_modify_arp_entry(struct rtnl_neigh *neighbor);
enum nw_api_rc nw_api_enable_nw_interface(unsigned int interface_id);
enum nw_api_rc nw_api_add_port_to_lag_group(unsigned int lag_group_id, unsigned int interface_id);
enum nw_api_rc nw_api_add_lag_group_entry(unsigned int lag_group_id);

/**************************************************************************//**
 * \brief       Initialize internal DB
 *
 * \return      success, failure or fatal error.
 */
enum nw_api_rc nw_api_init_db(void);

/**************************************************************************//**
 * \brief       Destroy internal DB
 *
 * \return      none
 */
void nw_api_destroy_db(void);

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
 * \brief       print all interfaces statistics
 *
 * \return	NW_API_OK - - operation succeeded
 *		NW_API_DB_ERROR - fail to read statistics
 */
enum nw_api_rc nw_api_print_if_stats(void);

#endif /* _NW_API_H_ */
