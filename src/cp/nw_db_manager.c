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
#include "nw_db.h"
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
char *addr_to_str(struct nl_addr *addr);
void add_entry_to_arp_table(struct rtnl_neigh *neighbor);
void remove_entry_from_arp_table(struct rtnl_neigh *neighbor);
bool valid_neighbor(struct rtnl_neigh *neighbor);
bool valid_route_entry(struct rtnl_route *route_entry);

/* Globals Definition */
struct nl_cache_mngr *network_cache_mngr;

bool *nw_db_manager_cancel_application_flag;

#define NW_DB_MANAGER_NEIGHBOR_FILTERED_STATE \
	(NUD_INCOMPLETE | NUD_FAILED | NUD_NOARP)



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

	if (nw_db_init() != NW_DB_OK) {
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
	nw_db_destroy();
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
void nw_db_manager_if_table_init(void)
{
	struct nw_if_key if_key;
	struct nw_if_result if_result;
	uint32_t ind;

	/* initialize my MAC in interface result */
	if (infra_get_my_mac(&if_result.mac_address) == false) {
		write_log(LOG_CRIT, "nw_db_manager_if_table_init: Retrieving my MAC failed.");
		nw_db_manager_exit_with_error();
	}

	/* build host interface entry */
	if_key.logical_id = USER_BASE_LOGICAL_ID + USER_NW_IF_NUM;
	if_result.path_type = DP_PATH_FROM_HOST_PATH;
	if_result.nw_stats_base = bswap_32((EZDP_EXTERNAL_MS << EZDP_SUM_ADDR_MEM_TYPE_OFFSET) |
					   (EMEM_IF_STATS_POSTED_MSID << EZDP_SUM_ADDR_MSID_OFFSET) |
					   ((EMEM_IF_STATS_POSTED_OFFSET + if_key.logical_id * NW_NUM_OF_IF_STATS) << EZDP_SUM_ADDR_ELEMENT_INDEX_OFFSET));
	if_result.output_channel = 24 | (1 << 7);
	build_nw_if_apps(&if_result.app_bitmap);
	if_result.sft_en = (system_cfg_is_qos_app_en() == true || system_cfg_is_firewall_app_en() == true) ? 1 : 0;

	if (infra_add_entry(STRUCT_ID_NW_INTERFACES, &if_key, sizeof(if_key), &if_result, sizeof(if_result)) == false) {
		write_log(LOG_CRIT, "nw_db_manager_if_table_init: Adding host if entry to if DB failed.");
		nw_db_manager_exit_with_error();
	}

	/* build network interfaces entries */
	if_result.path_type = DP_PATH_FROM_NW_PATH;
	for (ind = 0; ind < USER_NW_IF_NUM; ind++) {
		if_key.logical_id = USER_BASE_LOGICAL_ID + ind;
		if_result.nw_stats_base = bswap_32((EZDP_EXTERNAL_MS << EZDP_SUM_ADDR_MEM_TYPE_OFFSET) |
						   (EMEM_IF_STATS_POSTED_MSID << EZDP_SUM_ADDR_MSID_OFFSET) |
						   ((EMEM_IF_STATS_POSTED_OFFSET + if_key.logical_id * NW_NUM_OF_IF_STATS) << EZDP_SUM_ADDR_ELEMENT_INDEX_OFFSET));
		if_result.output_channel = ((ind % 2) * 12) | (ind < 2 ? 0 : (1 << 7));
		build_nw_if_apps(&if_result.app_bitmap);
		if_result.sft_en = (system_cfg_is_qos_app_en() == true || system_cfg_is_firewall_app_en() == true) ? 1 : 0;
		if (infra_add_entry(STRUCT_ID_NW_INTERFACES, &if_key, sizeof(if_key), &if_result, sizeof(if_result)) == false) {
			write_log(LOG_CRIT, "nw_db_manager_if_table_init: Adding NW if (%d) entry to if DB failed.", ind);
			nw_db_manager_exit_with_error();
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
			switch (nw_db_add_fib_entry(route_entry, true)) {
			case NW_DB_OK:
				break;
			case NW_DB_INTERNAL_ERROR:
			case NW_DB_NPS_ERROR:
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
			add_entry_to_arp_table(neighbor);
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
	enum nw_db_rc nw_ret = NW_DB_OK;
	/* Take only IPv4 entries.
	 * TODO: when adding IPv6 capabilities, this should be revisited.
	 */
	if (rtnl_route_get_family(route_entry) == AF_INET) {
		switch (action) {
		case NL_ACT_NEW:
			write_log(LOG_DEBUG, "FIB ADD entry: %s", addr_to_str(rtnl_route_get_dst(route_entry)));
			if (valid_route_entry(route_entry)) {
				/* Add route to FIB table */
				nw_ret = nw_db_add_fib_entry(route_entry, true);
				if (nw_ret != NW_DB_OK) {
					write_log(LOG_NOTICE, "Problem adding FIB entry: addr = %s", addr_to_str(rtnl_route_get_dst(route_entry)));
				}
			}
			break;
		case NL_ACT_DEL:
			write_log(LOG_DEBUG, "FIB DELETE entry: %s", addr_to_str(rtnl_route_get_dst(route_entry)));
			if (valid_route_entry(route_entry)) {
				nw_ret = nw_db_delete_fib_entry(route_entry);
				if (nw_ret != NW_DB_OK) {
					write_log(LOG_NOTICE, "Problem deleting FIB entry: addr = %s", addr_to_str(rtnl_route_get_dst(route_entry)));
				}
			}
			break;
		case NL_ACT_CHANGE:
			write_log(LOG_DEBUG, "FIB CHANGE entry: %s", addr_to_str(rtnl_route_get_dst(route_entry)));
			if (valid_route_entry(route_entry)) {
				nw_ret = nw_db_modify_fib_entry(route_entry);
				if (nw_ret != NW_DB_OK) {
					write_log(LOG_NOTICE, "Problem modifying FIB entry: addr = %s", addr_to_str(rtnl_route_get_dst(route_entry)));
				}
			}
			break;
		}
		if (nw_ret == NW_DB_INTERNAL_ERROR || nw_ret == NW_DB_NPS_ERROR) {
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
/******************************************************************************
 * \brief       ARP table callback function.
 *              Updates the NPS ARP table according to the received action.
 *
 * \return      void
 */
void nw_db_manager_arp_cb(struct nl_cache __attribute__((__unused__))*cache, struct nl_object *obj, int action, void __attribute__((__unused__))*data)
{
	struct rtnl_neigh *neighbor = (struct rtnl_neigh *)obj;
	/* Take only IPv4 entries.
	 * TODO: when adding IPv6 capabilities, this should be revisited.
	 */
	if (rtnl_neigh_get_family(neighbor) == AF_INET) {
		switch (action) {
		case NL_ACT_NEW:
			if (valid_neighbor(neighbor)) {
				add_entry_to_arp_table(neighbor);
			}
			break;
		case NL_ACT_DEL:
			remove_entry_from_arp_table(neighbor);
			break;
		case NL_ACT_CHANGE:
			if (valid_neighbor(neighbor)) {
				add_entry_to_arp_table(neighbor);
			} else {
				remove_entry_from_arp_table(neighbor);
			}
			break;
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

/******************************************************************************
 * \brief    translate Linux neighbor entry to ARP table key & result
 *
 * \return   void
 */
void neighbor_to_arp_entry(struct rtnl_neigh *neighbor, struct nw_arp_key *key, struct nw_arp_result *result)
{
	if (key) {
		key->real_server_address = *(uint32_t *)nl_addr_get_binary_addr(rtnl_neigh_get_dst(neighbor));
	}
	if (result) {
		memcpy(result->dest_mac_addr.ether_addr_octet, nl_addr_get_binary_addr(rtnl_neigh_get_lladdr(neighbor)), 6);

		result->base_logical_id = USER_BASE_LOGICAL_ID;
	}
}

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
 * \brief    Receives a neighbor and adds it as an entry to ARP table
 *           If an error is received from CP, exits the application.
 *
 * \return   void
 */
void add_entry_to_arp_table(struct rtnl_neigh *neighbor)
{
	struct nw_arp_result result;
	struct nw_arp_key key;

	write_log(LOG_DEBUG, "Add neighbor to table    IP = %s MAC = %s", addr_to_str(rtnl_neigh_get_dst(neighbor)), addr_to_str(rtnl_neigh_get_lladdr(neighbor)));
	neighbor_to_arp_entry(neighbor, &key, &result);
	if (!infra_add_entry(STRUCT_ID_NW_ARP, &key, sizeof(key), &result, sizeof(result))) {
		write_log(LOG_ERR, "Cannot add entry to ARP table key= 0x%X08 result= %02x:%02x:%02x:%02x:%02x:%02x", key.real_server_address,  result.dest_mac_addr.ether_addr_octet[0], result.dest_mac_addr.ether_addr_octet[1], result.dest_mac_addr.ether_addr_octet[2], result.dest_mac_addr.ether_addr_octet[3], result.dest_mac_addr.ether_addr_octet[4], result.dest_mac_addr.ether_addr_octet[5]);
		nw_db_manager_exit_with_error();
	}
}

/******************************************************************************
 * \brief    Receives a neighbor and removes the corresponding entry from
 *           ARP table.
 *           If an error is received from CP, exits the application.
 *
 * \return   void
 */
void remove_entry_from_arp_table(struct rtnl_neigh *neighbor)
{
	struct nw_arp_key key;

	write_log(LOG_DEBUG, "Delete neighbor from table IP = %s MAC = %s", addr_to_str(rtnl_neigh_get_dst(neighbor)), addr_to_str(rtnl_neigh_get_lladdr(neighbor)));
	neighbor_to_arp_entry(neighbor, &key, NULL);
	if (!infra_delete_entry(STRUCT_ID_NW_ARP, &key, sizeof(key))) {
		write_log(LOG_ERR, "Cannot remove entry from ARP table key= 0x%X08", key.real_server_address);
		nw_db_manager_exit_with_error();
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
	hash_params.max_num_of_entries = 65536;  /* TODO - define? */
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
	table_params.max_num_of_entries = 256;  /* TODO - define? */
	table_params.updated_from_dp = false;
	table_params.search_mem_heap = INFRA_X1_CLUSTER_SEARCH_HEAP;
	if (infra_create_table(STRUCT_ID_NW_INTERFACES,
			       &table_params) == false) {
		write_log(LOG_CRIT, "Error - Failed creating interface table.");
		return false;
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
