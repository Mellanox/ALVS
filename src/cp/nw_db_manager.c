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
*  Project:             NPS400 ALVS application
*  File:                nw_db_manager.c
*  Desc:                Performs the main process of network DB management
*
****************************************************************************/

/* linux includes */
#include <stdio.h>
#include <signal.h>
#include <byteswap.h>
#include <pthread.h>

/* libnl-3 includes */
#include <netlink/netlink.h>
#include <netlink/cache.h>
#include <netlink/route/neighbour.h>
#include <netlink/route/route.h>

#include <netlink/fib_lookup/request.h>
#include <netlink/fib_lookup/lookup.h>

/* Project includes */
#include "log.h"
#include "defs.h"
#include "nw_search_defs.h"
#include "nw_db_manager.h"
#include "infrastructure.h"
#include "nw_api.h"
#include "cfg.h"

/* Function Definition */
void nw_db_manager_init(void);
void nw_db_manager_delete(void);
void nw_db_manager_table_init(void);
void nw_db_manager_poll(void);
void nw_db_manager_if_table_init(void);
void nw_db_manager_arp_table_init(void);
void nw_db_manager_fib_table_init(void);
void nw_db_manager_arp_cb(struct nl_cache *cache, struct nl_object *obj,
			  int action, void *data);
void nw_db_manager_fib_cb(struct nl_cache *cache, struct nl_object *obj,
			  int action, void *data);

void neighbor_to_arp_entry(struct rtnl_neigh *neighbor, struct nw_arp_key *key,
			   struct nw_arp_result *result);
bool valid_neighbor(struct rtnl_neigh *neighbor);
bool valid_route_entry(struct rtnl_route *route_entry);

/* Globals Definition */
struct nl_cache_mngr *network_cache_mngr;

bool *nw_db_manager_cancel_application_flag;

#define NW_DB_MANAGER_NEIGHBOR_FILTERED_STATE \
	(NUD_INCOMPLETE | NUD_FAILED | NUD_NOARP)


/******************************************************************************
 * \brief    a helper function for nl address print
 *
 * \return   void
 */
char *addr_to_str(struct nl_addr *addr)
{
	static int i;
	/* keep 4 buffers in order to avoid collision when we printf more than one address */
	static char bufs[4][256];
	char *buf = bufs[i];

	i++;
	i %= 4;
	memset(buf, 0, sizeof(bufs[0]));
	return nl_addr2str(addr, buf, sizeof(bufs[0]));
}

/******************************************************************************
 * \brief	  Network thread main application.
 *
 * \return	  void
 */
void nw_db_manager_main(bool *cancel_application_flag)
{
	nw_db_manager_cancel_application_flag = cancel_application_flag;
	write_log(LOG_DEBUG, "nw_db_manager_init...");
	nw_db_manager_init();
	write_log(LOG_DEBUG, "nw_db_manager_table_init...");
	nw_db_manager_table_init();
	write_log(LOG_DEBUG, "nw_db_manager_poll...");
	nw_db_manager_poll();
	write_log(LOG_DEBUG, "nw_db_manager_exit...");
	nw_db_manager_delete();
}

/******************************************************************************
 * \brief	  Initialization of network cache manager.
 *            Also masks SIGTERM signal, will be received in main thread only.
 *
 * \return	  void
 */
void nw_db_manager_init(void)
{
	sigset_t sigs_to_block;

	network_cache_mngr = NULL;

	sigemptyset(&sigs_to_block);
	sigaddset(&sigs_to_block, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &sigs_to_block, NULL);

	/* Allocate cache manager */
	nl_cache_mngr_alloc(NULL, NETLINK_ROUTE, 0, &network_cache_mngr);

	if (nw_api_init_db() != NW_API_OK) {
		write_log(LOG_CRIT, "Failed to create NW SQL DB.");
		nw_db_manager_exit_with_error();
	}
}

/******************************************************************************
 * \brief	  Delete network cache manager object
 *
 * \return	  void
 */
void nw_db_manager_delete(void)
{
	write_log(LOG_DEBUG, "Delete NW DB manager");
	if (network_cache_mngr) {
		nl_cache_mngr_free(network_cache_mngr);
	}
	nw_api_destroy_db();
}

/******************************************************************************
 * \brief       Initialization of all tables handled by the network DB manager
 *
 * \return      void
 */
void nw_db_manager_table_init(void)
{
	nw_db_manager_if_table_init();
	nw_db_manager_fib_table_init();
	nw_db_manager_arp_table_init();
}

/******************************************************************************
 * \brief       Poll on all network BD changes
 *
 * \note        reads all received changes since table initialization and only
 *              after that, starts polling.
 *
 * \return      void
 */
void nw_db_manager_poll(void)
{
	while (*nw_db_manager_cancel_application_flag == false) {
		/* Get waiting updates received since previous cache read */
		nl_cache_mngr_data_ready(network_cache_mngr);
		/* Poll on cache updates */
		nl_cache_mngr_poll(network_cache_mngr, 0x1000);
	}
}

/******************************************************************************
 * \brief       Build application bitmap.
 *
 * \param[out]  nw_if_apps - reference to application bitmap
 *
 * \return      void
 */
static void build_nw_if_apps(struct nw_if_apps *nw_if_apps)
{
	nw_if_apps->alvs_en = (system_cfg_is_alvs_app_en() == true) ? 1 : 0;
	nw_if_apps->tc_en = (system_cfg_is_tc_app_en() == true) ? 1 : 0;
	nw_if_apps->routing_en = (system_cfg_is_routing_app_en() == true) ? 1 : 0;
	nw_if_apps->qos_en = (system_cfg_is_qos_app_en() == true) ? 1 : 0;
	nw_if_apps->firewall_en = (system_cfg_is_firewall_app_en() == true) ? 1 : 0;
}


/******************************************************************************
 * \brief       Interface table init.
 *
 * \return      void
 */
bool nw_db_manager_if_table_init_host(struct ether_addr  *mac_address,
				      bool               sft_en,
				      struct nw_if_apps  *app_bitmap)
{
	struct nw_if_key           if_key;
	struct nw_if_result        if_result;
	bool                       rc;

	/************************************************/
	/* Add DP entry                                 */
	/************************************************/

	/* set key */
	if_key.logical_id    = USER_HOST_LOGICAL_ID;

	/* set result */
	if_result.sft_en = sft_en;
	memcpy(&if_result.app_bitmap,  app_bitmap,  sizeof(if_result.app_bitmap));
	memcpy(&if_result.mac_address, mac_address, sizeof(if_result.mac_address));
	if_result.path_type  = DP_PATH_FROM_HOST_PATH;
	if_result.stats_base = bswap_32(BUILD_SUM_ADDR(EZDP_EXTERNAL_MS,
						       EMEM_IF_STATS_POSTED_MSID,
						       EMEM_HOST_IF_STATS_POSTED_OFFSET));
	if_result.output_channel       = 24 | (1 << 7);
	if_result.direct_output_if     = USER_NW_BASE_LOGICAL_ID;
	if_result.is_direct_output_lag = (system_cfg_is_lag_en() == true) ? 1 : 0;
	if_result.admin_state = true;

	/* add entry */
	rc = infra_add_entry(STRUCT_ID_NW_INTERFACES,
			     &if_key,
			     sizeof(if_key),
			     &if_result,
			     sizeof(if_result));
	if (rc == false) {
		write_log(LOG_CRIT, "nw_db_manager_if_table_init: Adding host if entry to if DB failed.");
		return false;
	}




	/************************************************/
	/* Add CP entry (SQL)                      todo */
	/************************************************/

	/* finish successfully */
	return true;
}

/******************************************************************************
 * \brief       Interface table init.
 *
 * \return      void
 */
bool nw_db_manager_if_table_init_nw(struct ether_addr  *mac_address,
				    bool               sft_en,
				    struct nw_if_apps  *app_bitmap)
{
	struct nw_db_nw_interface  interface_data;
	struct nw_if_key           if_key;
	struct nw_if_result        if_result;
	uint32_t                   ind;
	bool                       rc;
	enum nw_api_rc             nw_api_rc;

	/* set common result */
	if_result.sft_en = sft_en;
	memcpy(&if_result.app_bitmap,  app_bitmap,  sizeof(if_result.app_bitmap));
	memcpy(&if_result.mac_address, mac_address, sizeof(if_result.mac_address));
	if_result.path_type = DP_PATH_FROM_NW_PATH;


	for (ind = 0; ind < USER_NW_IF_NUM; ind++) {

		/************************************************/
		/* Add DP entry                                 */
		/************************************************/

		/* set key */
		if_key.logical_id = USER_NW_BASE_LOGICAL_ID + ind;

		/* set result */
		if_result.stats_base = bswap_32(BUILD_SUM_ADDR(EZDP_EXTERNAL_MS,
							       EMEM_IF_STATS_POSTED_MSID,
							       EMEM_NW_IF_STATS_POSTED_OFFSET + ind * NW_NUM_OF_IF_STATS));
		if_result.output_channel       = ((ind % 2) * 12) | (ind < 2 ? 0 : (1 << 7));
		if_result.direct_output_if     = (system_cfg_is_lag_en() == true) ? USER_HOST_LOGICAL_ID : (ind + USER_REMOTE_BASE_LOGICAL_ID);
		if_result.is_direct_output_lag = false;
		if_result.admin_state          = true;

		/* add entry in DP */
		rc = infra_add_entry(STRUCT_ID_NW_INTERFACES,
				     &if_key,
				     sizeof(if_key),
				     &if_result,
				     sizeof(if_result));
		if (rc == false) {
			write_log(LOG_CRIT, "nw_db_manager_if_table_init: Adding NW if (%d) entry to if DB failed.", ind);
			return false;
		}

		/************************************************/
		/* Add CP entry (SQL)                           */
		/************************************************/
		memset(&interface_data, 0, sizeof(interface_data));
		interface_data.interface_id = ind;
		interface_data.is_lag       = false; /* will be update when ading interface to LAG group */
		interface_data.lag_group_id = LAG_GROUP_NULL;     /* N/A - will be update when ading interface to LAG group */
		interface_data.admin_state  = true;
		memcpy(&interface_data.mac_address.ether_addr_octet, &if_result.mac_address.ether_addr_octet, ETH_ALEN);
		interface_data.app_bitmap = if_result.app_bitmap;
		interface_data.direct_output_if = if_result.direct_output_if;
		interface_data.is_direct_output_lag = if_result.is_direct_output_lag;
		interface_data.output_channel = if_result.output_channel;
		interface_data.path_type = if_result.path_type;
		interface_data.sft_en = if_result.sft_en;
		interface_data.stats_base = if_result.stats_base;

		/* add to internal db */
		nw_api_rc = internal_db_add_entry(NW_INTERFACES_INTERNAL_DB, (void *)&interface_data);
		if (nw_api_rc != NW_API_OK) {
			write_log(LOG_ERR, "add interface id %d on internal table was failed, failure on internal db", interface_data.interface_id);
			return false;
		}
	}


	/* finish successfully */
	return true;
}

/******************************************************************************
 * \brief       Interface table init.
 *
 * \return      void
 */
bool nw_db_manager_if_table_init_remote(struct ether_addr  *mac_address,
					bool               sft_en,
					struct nw_if_apps  *app_bitmap)
{
	struct nw_if_key           if_key;
	struct nw_if_result        if_result;
	uint32_t                   ind;
	bool                       rc;

	if (system_cfg_is_remote_control_en() != true) {
		/* Remote IF is not enabled - do nothing */
		return true;
	}


	/* set common result */
	if_result.sft_en = sft_en;
	memcpy(&if_result.app_bitmap,  app_bitmap,  sizeof(if_result.app_bitmap));
	memcpy(&if_result.mac_address, mac_address, sizeof(if_result.mac_address));
	if_result.path_type = DP_PATH_FROM_REMOTE_PATH;

	for (ind = 0; ind < USER_REMOTE_IF_NUM; ind++) {

		/************************************************/
		/* Add DP entry                                 */
		/************************************************/

		/* set key */
		if_key.logical_id = USER_REMOTE_BASE_LOGICAL_ID + ind;

		/* set result */
		if_result.stats_base = bswap_32(BUILD_SUM_ADDR(EZDP_EXTERNAL_MS,
							       EMEM_IF_STATS_POSTED_MSID,
							       EMEM_REMOTE_IF_STATS_POSTED_OFFSET + ind * REMOTE_NUM_OF_IF_STATS));

		if_result.output_channel   = ((ind % 2) * 12) | (ind < 2 ? 0 : (1 << 7));
		if_result.direct_output_if = ind + USER_NW_BASE_LOGICAL_ID;

		if (system_cfg_is_lag_en() == true) {
			if_result.direct_output_if     = USER_NW_BASE_LOGICAL_ID;
			if_result.is_direct_output_lag = true;
		} else {
			if_result.direct_output_if     =  + ind;
			if_result.is_direct_output_lag = false;
		}

		rc = infra_add_entry(STRUCT_ID_NW_INTERFACES,
				     &if_key,
				     sizeof(if_key),
				     &if_result,
				     sizeof(if_result));
		if (rc == false) {
			write_log(LOG_CRIT, "nw_db_manager_if_table_init: Adding remote if (%d) entry to if DB failed.", ind);
			return false;
		}


		/************************************************/
		/* Add CP entry (SQL)                           */
		/************************************************/
		/* TODO: implement */

	}


	/* finish successfully */
	return true;
}

/******************************************************************************
 * \brief       Interface table init.
 *
 * \return      void
 */
void nw_db_manager_if_table_init(void)
{
	struct ether_addr  mac_address;
	struct nw_if_apps  app_bitmap;
	bool               sft_en;
	bool               rc;
	int ind = 0;

	/* initialize common fields for all interfaces */
	/* initialize my MAC in interface result */
	rc = infra_get_my_mac(&mac_address);
	if (rc == false) {
		write_log(LOG_CRIT, "nw_db_manager_if_table_init: Retrieving my MAC failed.");
		nw_db_manager_exit_with_error();
	}
	sft_en = (system_cfg_is_qos_app_en() == true || system_cfg_is_firewall_app_en() == true) ? 1 : 0;
	build_nw_if_apps(&app_bitmap);


	/* Adding Host Interface */
	rc = nw_db_manager_if_table_init_host(&mac_address, sft_en, &app_bitmap);
	if (rc == false) {
		write_log(LOG_CRIT, "nw_db_manager_if_table_init_host failed");
		nw_db_manager_exit_with_error();
	}

	/* Adding Network Interfaces */
	rc = nw_db_manager_if_table_init_nw(&mac_address, sft_en, &app_bitmap);
	if (rc == false) {
		write_log(LOG_CRIT, "nw_db_manager_if_table_init_nw failed");
		nw_db_manager_exit_with_error();
	}

	/* Adding Remote Interfaces */
	rc = nw_db_manager_if_table_init_remote(&mac_address, sft_en, &app_bitmap);
	if (rc == false) {
		write_log(LOG_CRIT, "nw_db_manager_if_table_init_remote failed");
		nw_db_manager_exit_with_error();
	}

	if (system_cfg_is_lag_en() == true) {
		if (nw_api_add_lag_group_entry(0) != NW_API_OK) {
			write_log(LOG_CRIT, "Failed to create main lag group");
			nw_db_manager_exit_with_error();
		}

		for (ind = 0; ind < USER_NW_IF_NUM; ind++) {
			if (nw_api_add_port_to_lag_group(0, ind) != NW_API_OK) {
				write_log(LOG_CRIT, "Failed to add port %d to main lag group", ind);
				nw_db_manager_exit_with_error();
			}

		}
	}

}

/******************************************************************************
 * \brief       Convert route entry to fib entry
 *
 * \return      void
 */
void nw_db_manager_route_to_fib_entry(struct rtnl_route *route_entry, struct nw_api_fib_entry *fib_entry)
{
	struct nl_addr *dst = rtnl_route_get_dst(route_entry);

	memset(fib_entry, 0, sizeof(struct nw_api_fib_entry));

	fib_entry->route_type = rtnl_route_get_type(route_entry);
	fib_entry->mask_length = nl_addr_get_prefixlen(dst);
	fib_entry->dest.af = rtnl_route_get_family(route_entry);
	if (fib_entry->dest.af == AF_INET) {
		fib_entry->dest.in.s_addr = *(uint32_t *)nl_addr_get_binary_addr(dst);
	} else if (fib_entry->dest.af == AF_INET6) {
		/* For future support in ipv6 */
		memcpy((void *)&fib_entry->dest.in6, nl_addr_get_binary_addr(dst), sizeof(struct in6_addr));
	}
	if (fib_entry->route_type == RTN_UNICAST) {
		struct rtnl_nexthop *next_hop = rtnl_route_nexthop_n(route_entry, 0);
		struct nl_addr *next_hop_addr = rtnl_route_nh_get_gateway(next_hop);

		if (next_hop_addr == NULL) {
			fib_entry->next_hop_count = 0;
		} else {
			/* build next hop */
			fib_entry->next_hop_count = rtnl_route_get_nnexthops(route_entry);
			fib_entry->next_hop.af = fib_entry->dest.af;
			if (fib_entry->next_hop.af == AF_INET) {
				fib_entry->next_hop.in.s_addr = *(uint32_t *)nl_addr_get_binary_addr(next_hop_addr);
			} else if (fib_entry->next_hop.af == AF_INET6) {
				/* For future support in ipv6 */
				memcpy((void *)&fib_entry->next_hop.in6, nl_addr_get_binary_addr(next_hop_addr), sizeof(struct in6_addr));
			}
			fib_entry->next_hop_if = rtnl_route_nh_get_ifindex(next_hop);
		}
	}
}

/******************************************************************************
 * \brief       FIB table init.
 *              Registers a callback function for table changes.
 *              Reads current FIB table and writes all entries to NPS FIB table.
 *
 * \return      void
 */
void nw_db_manager_fib_table_init(void)
{
	int ret;
	struct nl_cache *filtered_route_cache;
	struct nl_cache *route_cache;
	struct nw_api_fib_entry fib_entry;
	struct rtnl_route *route_entry = rtnl_route_alloc();

	ret = nl_cache_mngr_add(network_cache_mngr, "route/route", &nw_db_manager_fib_cb, NULL, &route_cache);
	if (ret < 0) {
		write_log(LOG_CRIT, "Unable to add cache route/neigh: %s", nl_geterror(ret));
		nw_db_manager_exit_with_error();
	}

	/* Take only IPv4 entries.
	 * TODO: when adding IPv6 capabilities, this should be revisited.
	 */
	rtnl_route_set_family(route_entry, AF_INET);
	filtered_route_cache = nl_cache_subset(route_cache, (struct nl_object *)route_entry);

	/* Iterate on route cache */
	route_entry = (struct rtnl_route *)nl_cache_get_first(filtered_route_cache);
	while (route_entry != NULL) {
		if (valid_route_entry(route_entry)) {
			/* Add route to FIB table */
			nw_db_manager_route_to_fib_entry(route_entry, &fib_entry);
			switch (nw_api_add_fib_entry(&fib_entry)) {
			case NW_API_OK:
				break;
			case NW_API_DB_ERROR:
				write_log(LOG_CRIT, "Received fatal error from DBs when adding FIB entry: addr = %s .exiting.", addr_to_str(rtnl_route_get_dst(route_entry)));
				nw_db_manager_exit_with_error();
				break;
			default:
				write_log(LOG_NOTICE, "Problem adding FIB entry: addr = %s", addr_to_str(rtnl_route_get_dst(route_entry)));
			}
		}
		/* Next route */
		route_entry = (struct rtnl_route *)nl_cache_get_next((struct nl_object *)route_entry);
	}
}
/******************************************************************************
 * \brief       ARP table init.
 *              Registers a callback function for table changes.
 *              Reads current ARP table and writes all entries to NPS ARP table.
 *
 * \return      void
 */
void nw_db_manager_arp_table_init(void)
{
	int ret, i;
	struct nl_cache *neighbor_cache;
	struct rtnl_neigh *neighbor = rtnl_neigh_alloc();
	struct nl_cache *filtered_neighbor_cache;

	/* Allocate neighbor (ARP) cache */
	ret = nl_cache_mngr_add(network_cache_mngr, "route/neigh", &nw_db_manager_arp_cb, NULL, &neighbor_cache);
	if (ret < 0) {
		write_log(LOG_CRIT, "Unable to add cache route/neigh: %s", nl_geterror(ret));
		nw_db_manager_exit_with_error();
	}
	/* Take only IPv4 entries.
	 * TODO: when adding IPv6 capabilities, this should be revisited.
	 */
	rtnl_neigh_set_family(neighbor, AF_INET);
	filtered_neighbor_cache = nl_cache_subset(neighbor_cache, (struct nl_object *)neighbor);

	/* Iterate on neighbor cache */
	neighbor = (struct rtnl_neigh *)nl_cache_get_first(filtered_neighbor_cache);
	for (i = 0; neighbor != NULL; i++) {
		if (valid_neighbor(neighbor)) {
			/* Add neighbor to table */
			if (nw_db_add_arp_entry(neighbor) != NW_API_OK) {
				write_log(LOG_NOTICE, "Problem adding ARP entry: addr = %s", addr_to_str(rtnl_neigh_get_dst(neighbor)));
			}
		}
		/* Next neighbor */
		neighbor = (struct rtnl_neigh *)nl_cache_get_next((struct nl_object *)neighbor);
	}
}

/******************************************************************************
 * \brief       FIB table callback function.
 *              Updates the NPS FIB table according to the received action.
 *
 * \return      void
 */
void nw_db_manager_fib_cb(struct nl_cache __attribute__((__unused__))*cache, struct nl_object *obj, int action, void __attribute__((__unused__))*data)
{
	struct rtnl_route *route_entry = (struct rtnl_route *)obj;
	struct nw_api_fib_entry fib_entry;
	enum nw_api_rc nw_ret = NW_API_OK;

	if (valid_route_entry(route_entry)) {
		/* Take only IPv4 entries.
		 * TODO: when adding IPv6 capabilities, this should be revisited.
		 */
		if (rtnl_route_get_family(route_entry) == AF_INET) {
			nw_db_manager_route_to_fib_entry(route_entry, &fib_entry);
			switch (action) {
			case NL_ACT_NEW:
				write_log(LOG_DEBUG, "FIB ADD entry: %s", addr_to_str(rtnl_route_get_dst(route_entry)));
				nw_ret = nw_api_add_fib_entry(&fib_entry);
				if (nw_ret != NW_API_OK) {
					write_log(LOG_NOTICE, "Problem adding FIB entry: addr = %s", addr_to_str(rtnl_route_get_dst(route_entry)));
				}
				break;
			case NL_ACT_DEL:
				write_log(LOG_DEBUG, "FIB DELETE entry: %s", addr_to_str(rtnl_route_get_dst(route_entry)));
				nw_ret = nw_api_remove_fib_entry(&fib_entry);
				if (nw_ret != NW_API_OK) {
					write_log(LOG_NOTICE, "Problem deleting FIB entry: addr = %s", addr_to_str(rtnl_route_get_dst(route_entry)));
				}
				break;
			case NL_ACT_CHANGE:
				write_log(LOG_DEBUG, "FIB CHANGE entry: %s", addr_to_str(rtnl_route_get_dst(route_entry)));
				nw_ret = nw_api_modify_fib_entry(&fib_entry);
				if (nw_ret != NW_API_OK) {
					write_log(LOG_NOTICE, "Problem modifying FIB entry: addr = %s", addr_to_str(rtnl_route_get_dst(route_entry)));
				}
				break;
			}
			if (nw_ret == NW_API_DB_ERROR) {
				write_log(LOG_CRIT, "Received fatal error from NW DBs. exiting.");
				nw_db_manager_exit_with_error();
			}
		} else {
			switch (action) {
			case NL_ACT_NEW:
				write_log(LOG_NOTICE, "info: FIB entry from address family %d was not added. Address = %s", rtnl_route_get_family(route_entry), addr_to_str(rtnl_route_get_dst(route_entry)));
				break;
			case NL_ACT_DEL:
				write_log(LOG_NOTICE, "info: FIB entry from address family %d was not deleted. Address = %s", rtnl_route_get_family(route_entry), addr_to_str(rtnl_route_get_dst(route_entry)));
				break;
			case NL_ACT_CHANGE:
				write_log(LOG_NOTICE, "info: FIB entry from address family %d has not changed. Address = %s", rtnl_route_get_family(route_entry), addr_to_str(rtnl_route_get_dst(route_entry)));
				break;
			}
		}
	}
}
/******************************************************************************
 * \brief       ARP table callback function.
 *              Updates the NPS ARP table according to the received action.
 *
 * \return      void
 */
void nw_db_manager_arp_cb(struct nl_cache __attribute__((__unused__))*cache, struct nl_object *obj, int action, void __attribute__((__unused__))*data)
{
	enum nw_api_rc nw_ret = NW_API_OK;
	struct rtnl_neigh *neighbor = (struct rtnl_neigh *)obj;
	/* Take only IPv4 entries.
	 * TODO: when adding IPv6 capabilities, this should be revisited.
	 */
	if (rtnl_neigh_get_family(neighbor) == AF_INET) {
		switch (action) {
		case NL_ACT_NEW:
			if (valid_neighbor(neighbor)) {
				nw_ret = nw_db_add_arp_entry(neighbor);
				if (nw_ret != NW_API_OK) {
					write_log(LOG_NOTICE, "Problem adding ARP entry: addr = %s", addr_to_str(rtnl_neigh_get_dst(neighbor)));
				}
			}
			break;
		case NL_ACT_DEL:
			nw_ret = nw_db_remove_arp_entry(neighbor);
			if (nw_ret != NW_API_OK) {
				write_log(LOG_NOTICE, "Problem deleting ARP entry: addr = %s", addr_to_str(rtnl_neigh_get_dst(neighbor)));
			}
			break;
		case NL_ACT_CHANGE:
			if (valid_neighbor(neighbor)) {
				nw_ret = nw_db_modify_arp_entry(neighbor);
				if (nw_ret != NW_API_OK) {
					write_log(LOG_NOTICE, "Problem modifying ARP entry: addr = %s", addr_to_str(rtnl_neigh_get_dst(neighbor)));
				}
			} else {
				write_log(LOG_DEBUG, "Changing entry to invalid state - removing ARP entry");
				nw_ret = nw_db_remove_arp_entry(neighbor);
				if (nw_ret == NW_API_DB_ERROR) {
					write_log(LOG_NOTICE, "Problem removing ARP entry: addr = %s", addr_to_str(rtnl_neigh_get_dst(neighbor)));
				}
			}
			break;
		default:
			write_log(LOG_NOTICE, "Unknown action of netlink rtnl neighbour");
		}
		if (nw_ret == NW_API_DB_ERROR) {
			write_log(LOG_CRIT, "Received fatal error from NW DBs. exiting.");
			nw_db_manager_exit_with_error();
		}

	} else {
		switch (action) {
		case NL_ACT_NEW:
			write_log(LOG_NOTICE, "info: ARP entry from address family %d was not added. Address = %s", rtnl_neigh_get_family(neighbor), addr_to_str(rtnl_neigh_get_dst(neighbor)));
			break;
		case NL_ACT_DEL:
			write_log(LOG_NOTICE, "info: ARP entry from address family %d was not deleted. Address = %s", rtnl_neigh_get_family(neighbor), addr_to_str(rtnl_neigh_get_dst(neighbor)));
			break;
		case NL_ACT_CHANGE:
			write_log(LOG_NOTICE, "info: ARP entry from address family %d has not changed. Address = %s", rtnl_neigh_get_family(neighbor), addr_to_str(rtnl_neigh_get_dst(neighbor)));
			break;
		}
	}
}


bool valid_route_entry(struct rtnl_route *route_entry)
{
	return (rtnl_route_get_table(route_entry) == RT_TABLE_MAIN);
}

bool valid_neighbor(struct rtnl_neigh *neighbor)
{
	int state = rtnl_neigh_get_state(neighbor);

	return (!(state & NW_DB_MANAGER_NEIGHBOR_FILTERED_STATE) && state != NUD_NONE);

}
/******************************************************************************
 * \brief    Constructor function for all network data bases.
 *           this function is called not from the network thread but from the
 *           main thread on NPS configuration bringup.
 *
 * \return   void
 */
bool nw_db_constructor(void)
{
	struct infra_hash_params hash_params;
	struct infra_table_params table_params;
	struct infra_tcam_params tcam_params;

	write_log(LOG_DEBUG, "Creating ARP table.");

	hash_params.key_size = sizeof(struct nw_arp_key);
	hash_params.result_size = sizeof(struct nw_arp_result);
	hash_params.max_num_of_entries = NW_ARP_TABLE_MAX_ENTRIES;
	hash_params.hash_size = 0;
	hash_params.updated_from_dp = false;
	hash_params.main_table_search_mem_heap = INFRA_EMEM_SEARCH_HASH_HEAP;
	hash_params.sig_table_search_mem_heap = INFRA_EMEM_SEARCH_HASH_HEAP;
	hash_params.res_table_search_mem_heap = INFRA_EMEM_SEARCH_1_TABLE_HEAP;
	if (infra_create_hash(STRUCT_ID_NW_ARP,
			      &hash_params) == false) {
		write_log(LOG_CRIT, "Error - Failed creating ARP table.");
		return false;
	}

	write_log(LOG_DEBUG, "Creating FIB table.");

	tcam_params.key_size = sizeof(struct nw_fib_key);
	tcam_params.max_num_of_entries = NW_FIB_TCAM_MAX_SIZE;
	tcam_params.profile = NW_FIB_TCAM_PROFILE;
	tcam_params.result_size = sizeof(struct nw_fib_result);
	tcam_params.side = NW_FIB_TCAM_SIDE;
	tcam_params.lookup_table_count = NW_FIB_TCAM_LOOKUP_TABLE_COUNT;
	tcam_params.table = NW_FIB_TCAM_TABLE;

	if (infra_create_tcam(&tcam_params) == false) {
		write_log(LOG_CRIT, "Error - Failed creating FIB TCAM.");
		return false;
	}

	write_log(LOG_DEBUG, "Creating interface table.");

	table_params.key_size = sizeof(struct nw_if_key);
	table_params.result_size = sizeof(struct nw_if_result);
	table_params.max_num_of_entries = NW_INTERFACES_TABLE_MAX_ENTRIES;
	table_params.updated_from_dp = false;
	table_params.search_mem_heap = INFRA_X1_CLUSTER_SEARCH_HEAP;
	if (infra_create_table(STRUCT_ID_NW_INTERFACES,
			       &table_params) == false) {
		write_log(LOG_CRIT, "Error - Failed creating interface table.");
		return false;
	}

	write_log(LOG_DEBUG, "Creating Lag Group table.");

	table_params.key_size = sizeof(struct nw_lag_group_key);
	table_params.result_size = sizeof(struct nw_lag_group_result);
	table_params.max_num_of_entries = NW_LAG_GROUPS_TABLE_MAX_ENTRIES;
	table_params.updated_from_dp = false;
	table_params.search_mem_heap = INFRA_X1_CLUSTER_SEARCH_HEAP;
	if (infra_create_table(STRUCT_ID_NW_LAG_GROUPS,
			       &table_params) == false) {

	}

	return true;
}

/******************************************************************************
 * \brief    Raises SIGTERM signal to main thread and exits the thread.
 *           Deletes the DB manager.
 *
 * \return   void
 */
void nw_db_manager_exit_with_error(void)
{
	*nw_db_manager_cancel_application_flag = true;
	nw_db_manager_delete();
	kill(getpid(), SIGTERM);
	pthread_exit(NULL);
}
