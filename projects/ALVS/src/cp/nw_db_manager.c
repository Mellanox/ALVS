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
*  File:		nw_db_manager.c
*  Desc:		Performs the main process of network DB management
*
****************************************************************************/

/* linux includes */
#include <stdio.h>

/* libnl-3 includes */
#include <netlink/netlink.h>
#include <netlink/cache.h>
#include <netlink/route/neighbour.h>
#include <netlink/route/route.h>

/* Project includes */
#include "defs.h"
#include "nw_db_manager.h"
#include "infrastructure_conf.h"
#include "infrastructure.h"

/* Function Definition */
void nw_db_manager_init(void);
void nw_db_manager_table_init(void);
void nw_db_manager_poll(void);
void nw_db_manager_if_table_init(void);
void nw_db_manager_arp_table_init(void);
void nw_db_manager_arp_cb(struct nl_cache *cache, struct nl_object *obj, int action, void *data);
void neighbor_to_arp_entry(struct rtnl_neigh *neighbor, struct nw_arp_key *key, struct nw_arp_result *result);
char* addr_to_str(struct nl_addr *addr);
void add_entry_to_arp_table(struct rtnl_neigh *neighbor);
void remove_entry_from_arp_table(struct rtnl_neigh *neighbor);
/* Globals Definition */
struct nl_cache_mngr* network_cache_mngr;

#define NW_DB_MANAGER_NEIGHBOR_FILTERED_STATE (NUD_INCOMPLETE | NUD_FAILED | NUD_NOARP)


/******************************************************************************
 * \brief	  Main process
 *
 * \return	  void
 */
void nw_db_manager_process()
{
	printf("nw_db_manager_init ... \n");
	nw_db_manager_init();
	printf("nw_db_manager_table_init ... \n");
	nw_db_manager_table_init();
	printf("nw_db_manager_poll ... \n");
	nw_db_manager_poll();
}

/******************************************************************************
 * \brief	  Initialization of network socket and cache manager
 *
 * \return	  void
 */
void nw_db_manager_init()
{
	/* Allocate cache manager */
	network_cache_mngr = NULL;
	nl_cache_mngr_alloc(NULL , NETLINK_ROUTE , 0 , &network_cache_mngr);
}

/******************************************************************************
 * \brief	  Delete network socket and cache manager objects
 *
 * \return	  void
 */
void nw_db_manager_delete()
{
	printf("delete cache manager \n");
	nl_cache_mngr_free(network_cache_mngr);
}

/******************************************************************************
 * \brief	  Initialization of all tables handled by the DB manager
 *
 * \return	  void
 */
void nw_db_manager_table_init()
{
	nw_db_manager_if_table_init();
	nw_db_manager_arp_table_init();
}

/******************************************************************************
 * \brief	Poll on all BD changes
 *
 * \note	reads all received changes since table initialization and only
 * 		after that, starts polling.
 *
 * \return	void
 */
void nw_db_manager_poll()
{
	int err;
	/* Get waiting updates received since previous cache read */
	err = nl_cache_mngr_data_ready(network_cache_mngr);
	if (err < 0 && err != -NLE_INTR){
		printf("ERROR - Get data ready failed: %s", nl_geterror(err));
		return;
	}
	/* Poll on cache updates */
	while(1){
		err = nl_cache_mngr_poll(network_cache_mngr, 0x1FFFFFFF);
		if (err < 0 && err != -NLE_INTR){
			printf("ERROR - Polling failed: %s", nl_geterror(err));
			return;
		}
	}
}

/******************************************************************************
 * \brief	Interface table init.
 *
 * \return	void
 */
void nw_db_manager_if_table_init(void)
{
	struct dp_interface_key if_key;
	struct dp_interface_result if_result;
	FILE *fd;
	uint32_t ind;

	fd = fopen("/sys/class/net/"INFRA_HOST_INTERFACE"/address","r");
	if(fd == NULL) {
		printf("initialize_dbs: Opening eth address file failed.\n");
		return;
	}
	fscanf(fd, "%2hhx%*c%2hhx%*c%2hhx%*c%2hhx%*c%2hhx%*c%2hhx",
	       &if_result.mac_address.ether_addr_octet[0],
	       &if_result.mac_address.ether_addr_octet[1],
	       &if_result.mac_address.ether_addr_octet[2],
	       &if_result.mac_address.ether_addr_octet[3],
	       &if_result.mac_address.ether_addr_octet[4],
	       &if_result.mac_address.ether_addr_octet[5]);
	fclose(fd);

	if_key.logical_id = INFRA_HOST_IF_LOGICAL_ID;
	if_result.path_type = DP_PATH_SEND_TO_NW_NA;
	if (infra_add_entry(ALVS_STRUCT_ID_INTERFACES, &if_key, sizeof(if_key), &if_result, sizeof(if_result)) == false) {
		printf("initialize_dbs: Adding host if entry to if DB failed.\n");
		return;
	}

	if_result.path_type = DP_PATH_SEND_TO_HOST_NA;
	for (ind = 0; ind < INFRA_NW_IF_NUM; ind++) {
		if_key.logical_id = INFRA_NW_IF_BASE_LOGICAL_ID + ind;
		if (infra_add_entry(ALVS_STRUCT_ID_INTERFACES, &if_key, sizeof(if_key), &if_result, sizeof(if_result)) == false) {
			printf("initialize_dbs: Adding NW if (%d) entry to if DB failed.\n",ind);
			return;
		}
	}
}

/******************************************************************************
 * \brief	ARP table init.
 * 		Registers a callback function for table changes.
 * 		Reads current ARP table and writes all entries to NPS ARP table.
 *
 * \return	void
 */
void nw_db_manager_arp_table_init()
{
	int ret,i;
	struct nl_cache   *neighbor_cache;
	struct rtnl_neigh *neighbor = rtnl_neigh_alloc();
	struct nl_cache* filtered_neighbor_cache;

	/* Allocate neighbor (ARP) cache */
	ret = nl_cache_mngr_add(network_cache_mngr, "route/neigh", &nw_db_manager_arp_cb, NULL, &neighbor_cache);
	if (ret < 0){
		printf("Unable to add cache route/neigh: %s", nl_geterror(ret));
		exit(1);
	}
	rtnl_neigh_set_family(neighbor,AF_INET);
	filtered_neighbor_cache = nl_cache_subset(neighbor_cache,(struct nl_object*)neighbor);

	/* Iterate on neighbor cache */
	neighbor = (struct rtnl_neigh *)nl_cache_get_first(filtered_neighbor_cache);
	for (i = 0; neighbor != NULL ; i++){
		if(!(rtnl_neigh_get_state(neighbor) & NW_DB_MANAGER_NEIGHBOR_FILTERED_STATE)){
			/* Add neighbor to table */
			add_entry_to_arp_table(neighbor);
		}
		/* Next neighbor */
		neighbor=(struct rtnl_neigh *)nl_cache_get_next((struct nl_object*)neighbor);
	}
}

/******************************************************************************
 * \brief	ARP table callback function.
 * 		Updates the NPS ARP table according to the received action.
 *
 * \return	void
 */
void nw_db_manager_arp_cb(struct nl_cache *cache, struct nl_object *obj, int action, void *data)
{
	struct rtnl_neigh *neighbor = (struct rtnl_neigh *)obj;
	if(rtnl_neigh_get_family(neighbor) == AF_INET){
		switch(action){
		case NL_ACT_NEW:
			if(!(rtnl_neigh_get_state(neighbor) & NW_DB_MANAGER_NEIGHBOR_FILTERED_STATE)){
				add_entry_to_arp_table(neighbor);
			}
			break;
		case NL_ACT_DEL:
			if(!(rtnl_neigh_get_state(neighbor) & NW_DB_MANAGER_NEIGHBOR_FILTERED_STATE)){
				remove_entry_from_arp_table(neighbor);
			}
			break;
		case NL_ACT_CHANGE:
			if(rtnl_neigh_get_state(neighbor) & NW_DB_MANAGER_NEIGHBOR_FILTERED_STATE){
				remove_entry_from_arp_table(neighbor);
			}
			else{
				add_entry_to_arp_table(neighbor);
			}
		}

	}
}

void neighbor_to_arp_entry(struct rtnl_neigh *neighbor, struct nw_arp_key *key, struct nw_arp_result *result)
{
	if(key){
		key->real_server_address = htonl(*(uint32_t*)nl_addr_get_binary_addr(rtnl_neigh_get_dst(neighbor)));
	}
	if(result){
		memcpy ( result->dest_mac_addr.ether_addr_octet , nl_addr_get_binary_addr(rtnl_neigh_get_lladdr(neighbor)) , 6);

		//TODO: what is the output channel ???
		result->base_output_channel = 0;
	}
}

char* addr_to_str(struct nl_addr *addr)
{
	static int i=0;
	static char bufs[4][256]; // keep 4 buffers in order to avoid collision when we printf more than one address
	char* buf=bufs[i];
	i++;
	i%=4;
	memset(buf,0,sizeof(bufs[0]));
	return nl_addr2str(addr, buf , sizeof(bufs[0]));
}

void add_entry_to_arp_table(struct rtnl_neigh *neighbor)
{
	struct nw_arp_result result;
	struct nw_arp_key key;
	printf("Add neighbor to table    IP = %s MAC = %s \n" , addr_to_str(rtnl_neigh_get_dst(neighbor)), addr_to_str(rtnl_neigh_get_lladdr(neighbor)));
	neighbor_to_arp_entry(neighbor, &key, &result);
	if(!infra_add_entry(ALVS_STRUCT_ID_ARP, &key, sizeof(key), &result, sizeof(result))){
		printf("Error - cannot add entry to ARP table key= 0x%X08 result= %02x:%02x:%02x:%02x:%02x:%02x \n", key.real_server_address,  result.dest_mac_addr.ether_addr_octet[0], result.dest_mac_addr.ether_addr_octet[1], result.dest_mac_addr.ether_addr_octet[2], result.dest_mac_addr.ether_addr_octet[3], result.dest_mac_addr.ether_addr_octet[4], result.dest_mac_addr.ether_addr_octet[5]);
	}
}
void remove_entry_from_arp_table(struct rtnl_neigh *neighbor)
{
	struct nw_arp_key key;
	printf("Delete neighbor from table IP = %s MAC = %s \n" , addr_to_str(rtnl_neigh_get_dst(neighbor)), addr_to_str(rtnl_neigh_get_lladdr(neighbor)));
	neighbor_to_arp_entry(neighbor, &key, NULL);
	if(!infra_delete_entry(ALVS_STRUCT_ID_ARP, &key, sizeof(key))){
		printf("Error - cannot remove entry from ARP table key= 0x%X08 \n", key.real_server_address);
	}
}

bool nw_db_constructor(void)
{
	struct infra_hash_params hash_params;
	struct infra_table_params table_params;

	printf("Creating ARP table.\n");

	hash_params.key_size = sizeof(struct nw_arp_key);
	hash_params.result_size = sizeof(struct nw_arp_result);
	hash_params.max_num_of_entries = 65536;  // TODO - define?
	hash_params.updated_from_dp = false;
	if (infra_create_hash(ALVS_STRUCT_ID_ARP, INFRA_EMEM_SEARCH_HEAP, &hash_params) == false) {
		printf("Error - Failed creating ARP table.\n");
		return false;
	}

	printf("Creating interface table.\n");

	table_params.key_size = sizeof(struct dp_interface_key);
	table_params.result_size = sizeof(struct dp_interface_result);
	table_params.max_num_of_entries = 256;  // TODO - define?
	if (infra_create_table(ALVS_STRUCT_ID_INTERFACES, INFRA_X1_CLUSTER_SEARCH_HEAP, &table_params) == false) {
		printf("Error - Failed creating interface table.\n");
		return false;
	}

	return true;
}
