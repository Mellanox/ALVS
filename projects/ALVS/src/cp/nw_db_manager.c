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
#include "nw_db_manager.h"
#include "search.h"

/* Function Definition */
void nw_db_manager_init(void);
void nw_db_manager_table_init(void);
void nw_db_manager_poll(void);
void nw_db_manager_arp_table_init(void);
void nw_db_manager_arp_cb(struct nl_cache *cache, struct nl_object *obj, int action, void *data);
void neighbor_to_arp_entry(struct rtnl_neigh *neighbor, struct alvs_arp_key *key, struct alvs_arp_result *result);
char* addr_to_str(struct nl_addr *addr);

/* Globals Definition */
struct nl_sock *network_socket;
struct nl_cache_mngr* network_cache_mngr;



/******************************************************************************
 * \brief	  Main process
 *
 * \return	  void
 */
void nw_db_manager_process()
{
	nw_db_manager_init();
	nw_db_manager_table_init();
	nw_db_manager_poll();
}

/******************************************************************************
 * \brief	  Initialization of network socket and cache manager
 *
 * \return	  void
 */
void nw_db_manager_init()
{
	/* Allocate net link socket and bind.  */
	network_socket= nl_socket_alloc();
	nl_connect(network_socket, NETLINK_ROUTE);
	/* Allocate cache manager */
	network_cache_mngr = NULL;
	nl_cache_mngr_alloc(network_socket , NETLINK_ROUTE , 0 , &network_cache_mngr);
}

/******************************************************************************
 * \brief	  Delete network socket and cache manager objects
 *
 * \return	  void
 */
void nw_db_manager_delete()
{
	nl_cache_mngr_free(network_cache_mngr);
	nl_socket_free(network_socket);
}

/******************************************************************************
 * \brief	  Initialization of all tables handled by the DB manager
 *
 * \return	  void
 */
void nw_db_manager_table_init()
{
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
	struct rtnl_neigh *neighbor;
	struct alvs_arp_result result;
	struct alvs_arp_key key;
	bool ret_code;

	/* Allocate neighbor (ARP) cache */
	ret = nl_cache_mngr_add(network_cache_mngr, "route/neigh", &nw_db_manager_arp_cb, NULL, &neighbor_cache);
	if (ret < 0)
		printf("Unable to add cache route/neigh: %s", nl_geterror(ret));
	else
		printf("Number of neighbors in ARP table = %d \n" , nl_cache_nitems(neighbor_cache) );

	/* Iterate on neighbor cache */
	neighbor = (struct rtnl_neigh *)nl_cache_get_first(neighbor_cache);
	for (i = 0; i < nl_cache_nitems(neighbor_cache); i++){
		/* Add neighbor to table */
		printf("Add neighbor to table IP = %s MAC = %s \n " , addr_to_str(rtnl_neigh_get_dst(neighbor)), addr_to_str(rtnl_neigh_get_lladdr(neighbor)));
		neighbor_to_arp_entry(neighbor, &key, &result);
		ret_code = add_arp_entry(&key, &result);
		if(!ret_code){
			printf("Error - cannot add entry to ARP table key= 0x%X08 result= %02x:%02x:%02x:%02x:%02x:%02x \n", key.real_server_address,  result.dest_mac_addr.ether_addr_octet[0], result.dest_mac_addr.ether_addr_octet[1], result.dest_mac_addr.ether_addr_octet[2], result.dest_mac_addr.ether_addr_octet[3], result.dest_mac_addr.ether_addr_octet[4], result.dest_mac_addr.ether_addr_octet[5]);
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
	struct alvs_arp_result result;
	struct alvs_arp_key key;
	bool ret_code;
	switch(action){
	case NL_ACT_NEW:
		printf("Added neighbor to table    IP = %s MAC = %s \n " , addr_to_str(rtnl_neigh_get_dst(neighbor)), addr_to_str(rtnl_neigh_get_lladdr(neighbor)));
		neighbor_to_arp_entry(neighbor, &key, &result);
		ret_code = add_arp_entry(&key, &result);
		if(!ret_code){
			printf("Error - cannot add entry to ARP table \n");
		}
		break;
	case NL_ACT_DEL:
		printf("Delete neighbor from table IP = %s MAC = %s \n " , addr_to_str(rtnl_neigh_get_dst(neighbor)), addr_to_str(rtnl_neigh_get_lladdr(neighbor)));
		neighbor_to_arp_entry(neighbor, &key, &result);
		ret_code = delete_arp_entry(&key);
		if(!ret_code){
			printf("Error - cannot remove entry from ARP table \n");
		}
		break;
	default:
		printf("CHANGE - ???");

	}
}

void neighbor_to_arp_entry(struct rtnl_neigh *neighbor, struct alvs_arp_key *key, struct alvs_arp_result *result)
{
	key->real_server_address = *(uint32_t*)nl_addr_get_binary_addr(rtnl_neigh_get_dst(neighbor));
	memcpy ( result->dest_mac_addr.ether_addr_octet , nl_addr_get_binary_addr(rtnl_neigh_get_lladdr(neighbor)) , 6);
	//TODO: what is the output channel ???
	result->base_output_channel = 0;
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
