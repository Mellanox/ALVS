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
#include <pthread.h>

/* libnl-3 includes */
#include <netlink/netlink.h>
#include <netlink/cache.h>
#include <netlink/route/neighbour.h>
#include <netlink/route/route.h>

/* Project includes */
#include "defs.h"
#include "nw_search_defs.h"
#include "nw_db_manager.h"
#include "infrastructure.h"

/* Function Definition */
void nw_db_manager_init(void);
void nw_db_manager_delete(void);
void nw_db_manager_table_init(void);
void nw_db_manager_poll(void);
void nw_db_manager_if_table_init(void);
void nw_db_manager_arp_table_init(void);
void nw_db_manager_arp_cb(struct nl_cache *cache, struct nl_object *obj,
			  int action, void *data);
void neighbor_to_arp_entry(struct rtnl_neigh *neighbor, struct nw_arp_key *key,
			   struct nw_arp_result *result);
char *addr_to_str(struct nl_addr *addr);
void add_entry_to_arp_table(struct rtnl_neigh *neighbor);
void remove_entry_from_arp_table(struct rtnl_neigh *neighbor);

/* Globals Definition */
struct nl_cache_mngr *network_cache_mngr;
bool is_nw_db_manager_cancel_thread;

#define NW_DB_MANAGER_NEIGHBOR_FILTERED_STATE \
	(NUD_INCOMPLETE | NUD_FAILED | NUD_NOARP | NUD_NONE)


/******************************************************************************
 * \brief	  Network thread main application.
 *
 * \return	  void
 */
void nw_db_manager_main(void)
{
	printf("nw_db_manager_init...\n");
	nw_db_manager_init();
	printf("nw_db_manager_table_init...\n");
	nw_db_manager_table_init();
	printf("nw_db_manager_poll...\n");
	nw_db_manager_poll();
	nw_db_manager_delete();
}

/******************************************************************************
 * \brief	  Signals the network thread to stop and exit.
 *
 * \return	  void
 */
void nw_db_manager_set_cancel_thread(void)
{
	is_nw_db_manager_cancel_thread = true;
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
}

/******************************************************************************
 * \brief	  Delete network cache manager object
 *
 * \return	  void
 */
void nw_db_manager_delete(void)
{
	printf("delete cache manager\n");
	if (network_cache_mngr)
		nl_cache_mngr_free(network_cache_mngr);
}

/******************************************************************************
 * \brief       Initialization of all tables handled by the network DB manager
 *
 * \return      void
 */
void nw_db_manager_table_init(void)
{
	nw_db_manager_if_table_init();
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
	while (!is_nw_db_manager_cancel_thread) {
		/* Get waiting updates received since previous cache read */
		nl_cache_mngr_data_ready(network_cache_mngr);
		/* Poll on cache updates */
		nl_cache_mngr_poll(network_cache_mngr, 0x1000);
	}
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

	if (infra_get_my_mac(&if_result.mac_address) == false) {
		printf("nw_db_manager_if_table_init: Retrieving my MAC failed.\n");
		nw_db_manager_exit_with_error();
	}

	if_key.logical_id = INFRA_BASE_LOGICAL_ID + INFRA_NW_IF_NUM;
	if_result.path_type = DP_PATH_SEND_TO_NW_NA;
	if (infra_add_entry(STRUCT_ID_NW_INTERFACES, &if_key, sizeof(if_key), &if_result, sizeof(if_result)) == false) {
		printf("nw_db_manager_if_table_init: Adding host if entry to if DB failed.\n");
		nw_db_manager_exit_with_error();
	}

	if_result.path_type = DP_PATH_SEND_TO_HOST_NA;
	for (ind = 0; ind < INFRA_NW_IF_NUM; ind++) {
		if_key.logical_id = INFRA_BASE_LOGICAL_ID + ind;
		if (infra_add_entry(STRUCT_ID_NW_INTERFACES, &if_key, sizeof(if_key), &if_result, sizeof(if_result)) == false) {
			printf("nw_db_manager_if_table_init: Adding NW if (%d) entry to if DB failed.\n", ind);
			nw_db_manager_exit_with_error();
		}
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
		printf("Unable to add cache route/neigh: %s", nl_geterror(ret));
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
		if (!(rtnl_neigh_get_state(neighbor) & NW_DB_MANAGER_NEIGHBOR_FILTERED_STATE)) {
			/* Add neighbor to table */
			add_entry_to_arp_table(neighbor);
		}
		/* Next neighbor */
		neighbor = (struct rtnl_neigh *)nl_cache_get_next((struct nl_object *)neighbor);
	}
}

/******************************************************************************
 * \brief       ARP table callback function.
 *              Updates the NPS ARP table according to the received action.
 *
 * \return      void
 */
void nw_db_manager_arp_cb(struct nl_cache *cache, struct nl_object *obj, int action, void *data)
{
	struct rtnl_neigh *neighbor = (struct rtnl_neigh *)obj;
	/* Take only IPv4 entries.
	 * TODO: when adding IPv6 capabilities, this should be revisited.
	 */
	if (rtnl_neigh_get_family(neighbor) == AF_INET) {
		switch (action) {
		case NL_ACT_NEW:
			if (!(rtnl_neigh_get_state(neighbor) & NW_DB_MANAGER_NEIGHBOR_FILTERED_STATE)) {
				add_entry_to_arp_table(neighbor);
			}
			break;
		case NL_ACT_DEL:
			if (!(rtnl_neigh_get_state(neighbor) & NW_DB_MANAGER_NEIGHBOR_FILTERED_STATE)) {
				remove_entry_from_arp_table(neighbor);
			}
			break;
		case NL_ACT_CHANGE:
			if (rtnl_neigh_get_state(neighbor) & NW_DB_MANAGER_NEIGHBOR_FILTERED_STATE) {
				remove_entry_from_arp_table(neighbor);
			} else {
				add_entry_to_arp_table(neighbor);
			}
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

		result->base_logical_id = INFRA_BASE_LOGICAL_ID;
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

	printf("Add neighbor to table    IP = %s MAC = %s\n", addr_to_str(rtnl_neigh_get_dst(neighbor)), addr_to_str(rtnl_neigh_get_lladdr(neighbor)));
	neighbor_to_arp_entry(neighbor, &key, &result);
	if (!infra_add_entry(STRUCT_ID_NW_ARP, &key, sizeof(key), &result, sizeof(result))) {
		printf("Error - cannot add entry to ARP table key= 0x%X08 result= %02x:%02x:%02x:%02x:%02x:%02x\n", key.real_server_address,  result.dest_mac_addr.ether_addr_octet[0], result.dest_mac_addr.ether_addr_octet[1], result.dest_mac_addr.ether_addr_octet[2], result.dest_mac_addr.ether_addr_octet[3], result.dest_mac_addr.ether_addr_octet[4], result.dest_mac_addr.ether_addr_octet[5]);
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

	printf("Delete neighbor from table IP = %s MAC = %s\n", addr_to_str(rtnl_neigh_get_dst(neighbor)), addr_to_str(rtnl_neigh_get_lladdr(neighbor)));
	neighbor_to_arp_entry(neighbor, &key, NULL);
	if (!infra_delete_entry(STRUCT_ID_NW_ARP, &key, sizeof(key))) {
		printf("Error - cannot remove entry from ARP table key= 0x%X08\n", key.real_server_address);
		nw_db_manager_exit_with_error();
	}
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

	printf("Creating ARP table.\n");

	hash_params.key_size = sizeof(struct nw_arp_key);
	hash_params.result_size = sizeof(struct nw_arp_result);
	hash_params.max_num_of_entries = 65536;  /* TODO - define? */
	hash_params.updated_from_dp = false;
	if (infra_create_hash(STRUCT_ID_NW_ARP, INFRA_EMEM_SEARCH_HEAP,
			      &hash_params) == false) {
		printf("Error - Failed creating ARP table.\n");
		return false;
	}

	printf("Creating interface table.\n");

	table_params.key_size = sizeof(struct nw_if_key);
	table_params.result_size = sizeof(struct nw_if_result);
	table_params.max_num_of_entries = 256;  /* TODO - define? */
	if (infra_create_table(STRUCT_ID_NW_INTERFACES,
			       INFRA_X1_CLUSTER_SEARCH_HEAP,
			       &table_params) == false) {
		printf("Error - Failed creating interface table.\n");
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
	nw_db_manager_delete();
	raise(SIGTERM);
	pthread_exit(NULL);
}
