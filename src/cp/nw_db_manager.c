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
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/if.h>

/* libnl-3 includes */
#include <netlink/netlink.h>
#include <netlink/cache.h>
#include <netlink/route/neighbour.h>
#include <netlink/route/link.h>
#include <netlink/route/route.h>

#include <netlink/fib_lookup/request.h>
#include <netlink/fib_lookup/lookup.h>

/* Project includes */
#include "log.h"
#include "nw_conf.h"
#include "infrastructure_utils.h"
#include "nw_api.h"
#include "nw_db_manager.h"
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
void nw_db_manager_ifc_cb(struct nl_cache *cache, struct nl_object *obj,
			  int action, void *data);

bool valid_neighbor(struct rtnl_neigh *neighbor);
bool valid_route_entry(struct rtnl_route *route_entry);

/* Globals Definition */
struct nl_cache_mngr *network_cache_mngr;

bool *nw_db_manager_cancel_application_flag;

#define NW_DB_MANAGER_NEIGHBOR_FILTERED_STATE \
	(NUD_INCOMPLETE | NUD_FAILED | NUD_NOARP)

/* interface mapping if_map_by_name[interface-id] = interface name */
char *if_map_by_name[NW_IF_NUM] = {"eth0", "eth1", "eth2", "eth3"};
/* interface mapping if_map_by_index[interface-id] = linux interface index */
int if_map_by_index[NW_IF_NUM] = {-1, -1, -1, -1};

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
		/* Poll on cache updates */
		nl_cache_mngr_poll(network_cache_mngr, 0x1000);
	}
}

/******************************************************************************
 * \brief       interface id lookup using interface name
 *
 * \param[in]   if_name - interface name
 *
 * \return      interface id.
 *		valid interface id: 0-3 for interfaces eth0-3
 *		otherwise return -1
 */
int32_t if_lookup_by_name(char *if_name)
{
	int i;
	char *name;

	for (i = 0; i < NW_IF_NUM; i++) {
		name = if_map_by_name[i];
		if (!strcmp(if_name, name)) {
			return i;
		}
	}
	return -1;
}

/******************************************************************************
 * \brief       interface id lookup using interface linux index
 *
 * \param[in]   linux_index - interface linux index
 *
 * \return      interface id.
 *		valid interface id: 0-3 for interfaces eth0-3
 *		otherwise return -1
 */
int32_t if_lookup_by_index(int  __attribute__((__unused__))linux_index)
{
	/* TODO: this is a workaround for ALVS until real implementation of eth mapping */
#ifdef CONFIG_ALVS
	return 0;
#else
	int i;

	for (i = 0; i < NW_IF_NUM; i++) {
		if (if_map_by_index[i] == linux_index) {
			return i;
		}
	}
	return -1;
#endif
}

/******************************************************************************
 * \brief    Convert Linux link entry to nw api IF entry
 *
 * \return	bool:	true - valid IF entry
 *			false- IF not valid
 */
bool nw_db_manager_link_to_if_entry(struct rtnl_link *link, struct nw_api_if_entry *if_entry)
{
	struct nl_addr *mac;
	char *ifname;
	int32_t if_index, linux_if_index;

	memset(if_entry, 0, sizeof(struct nw_api_if_entry));
	ifname = rtnl_link_get_name(link);
	if_index = if_lookup_by_name(ifname);
	if (if_index == -1) {
		return false;
	}
	linux_if_index = rtnl_link_get_ifindex(link);
	/* Set Linux IF mapping to internal IF index */
	if_map_by_index[if_index] = linux_if_index;
	if_entry->if_index = if_index;
	mac = rtnl_link_get_addr(link);
	if (mac != NULL) {
		memcpy(if_entry->mac_addr.ether_addr_octet, nl_addr_get_binary_addr(mac), ETH_ALEN);
	}
	return true;
}

/******************************************************************************
 * \brief    Convert Linux neighbor entry to nw api arp entry
 *           Check if ARP entry belongs to a relevant IF (valid ARP entry)
 * \return	bool:	true - valid neighbor entry
 *			false- neighbor not valid
 */
bool nw_db_manager_neighbor_to_arp_entry(struct rtnl_neigh *neighbor, struct nw_api_arp_entry *arp_entry)
{
	struct nl_addr *dst = rtnl_neigh_get_dst(neighbor);
	struct nl_addr *mac = rtnl_neigh_get_lladdr(neighbor);
	int if_index;

	memset(arp_entry, 0, sizeof(struct nw_api_arp_entry));
	if_index = if_lookup_by_index(rtnl_neigh_get_ifindex(neighbor));
	if (if_index == -1) {
		return false;
	}
	arp_entry->if_index = if_index;
	arp_entry->ip_addr.af = rtnl_neigh_get_family(neighbor);
	if (arp_entry->ip_addr.af == AF_INET) {
		arp_entry->ip_addr.in.s_addr = *(uint32_t *)nl_addr_get_binary_addr(dst);
	} else if (arp_entry->ip_addr.af == AF_INET6) {
		memcpy((void *)&arp_entry->ip_addr.in6, nl_addr_get_binary_addr(dst), sizeof(struct in6_addr));
	}
	if (mac != NULL) {
		memcpy(arp_entry->mac_addr.ether_addr_octet, nl_addr_get_binary_addr(mac), ETH_ALEN);
	}
	return true;
}

/******************************************************************************
 * \brief       Convert route entry to nw api fib entry
 *              Check if FIB entry belongs to a relevant IF (valid FIB entry)
 *
 * \return	bool:	true - valid FIB entry
 *			false- FIB not valid
 */
bool nw_db_manager_route_to_fib_entry(struct rtnl_route *route_entry, struct nw_api_fib_entry *fib_entry)
{
	struct nl_addr *dst = rtnl_route_get_dst(route_entry);
	int if_index;

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
		/* next hop index in case of neighbor or GW will indicate output interface index */
		if_index = if_lookup_by_index(rtnl_route_nh_get_ifindex(next_hop));
		if (if_index == -1) {
			return false;
		}
		fib_entry->output_if_index = if_index;

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
		}
	}
	return true;
}

/******************************************************************************
 * \brief       Interface table init.
 *
 * \return      void
 */
void nw_db_manager_if_table_init(void)
{
	int ret;
	struct nl_cache *ifc_cache;
	struct rtnl_link *link_entry;
	struct nw_api_if_entry if_entry;

	ret = nl_cache_mngr_add(network_cache_mngr, "route/link", &nw_db_manager_ifc_cb, NULL, &ifc_cache);
	if (ret < 0) {
		write_log(LOG_CRIT, "Unable to add cache route/link: %s", nl_geterror(ret));
		nw_db_manager_exit_with_error();
	}

	/* Iterate on ifc cache */
	link_entry = (struct rtnl_link *)nl_cache_get_first(ifc_cache);
	while (link_entry != NULL) {
		if ((nw_db_manager_link_to_if_entry(link_entry, &if_entry) == true) && (rtnl_link_get_flags(link_entry) & IFF_UP)) {
			switch (nw_api_modify_if_entry(&if_entry)) {
			case NW_API_OK:
				write_log(LOG_INFO, "IF modified successfully. IF ID = %d, MAC = %02X:%02X:%02X:%02X:%02X:%02X", if_entry.if_index, if_entry.mac_addr.ether_addr_octet[0], if_entry.mac_addr.ether_addr_octet[1], if_entry.mac_addr.ether_addr_octet[2], if_entry.mac_addr.ether_addr_octet[3], if_entry.mac_addr.ether_addr_octet[4], if_entry.mac_addr.ether_addr_octet[5]);
				switch (nw_api_enable_if_entry(&if_entry)) {
				case NW_API_OK:
					write_log(LOG_INFO, "IF enabled successfully. IF ID = %d, MAC = %02X:%02X:%02X:%02X:%02X:%02X", if_entry.if_index, if_entry.mac_addr.ether_addr_octet[0], if_entry.mac_addr.ether_addr_octet[1], if_entry.mac_addr.ether_addr_octet[2], if_entry.mac_addr.ether_addr_octet[3], if_entry.mac_addr.ether_addr_octet[4], if_entry.mac_addr.ether_addr_octet[5]);
					break;
				case NW_API_FAILURE:
					write_log(LOG_NOTICE, "Failed to enable IF. IF ID = %d, MAC = %02X:%02X:%02X:%02X:%02X:%02X", if_entry.if_index, if_entry.mac_addr.ether_addr_octet[0], if_entry.mac_addr.ether_addr_octet[1], if_entry.mac_addr.ether_addr_octet[2], if_entry.mac_addr.ether_addr_octet[3], if_entry.mac_addr.ether_addr_octet[4], if_entry.mac_addr.ether_addr_octet[5]);
					break;
				case NW_API_DB_ERROR:
					write_log(LOG_CRIT, "Received fatal error from DBs when enabling IF: IF ID = %d, MAC = %02X:%02X:%02X:%02X:%02X:%02X", if_entry.if_index, if_entry.mac_addr.ether_addr_octet[0], if_entry.mac_addr.ether_addr_octet[1], if_entry.mac_addr.ether_addr_octet[2], if_entry.mac_addr.ether_addr_octet[3], if_entry.mac_addr.ether_addr_octet[4], if_entry.mac_addr.ether_addr_octet[5]);
					nw_db_manager_exit_with_error();
					break;
				}
				break;
			case NW_API_FAILURE:
				write_log(LOG_NOTICE, "Failed to modify IF. IF ID = %d, MAC = %02X:%02X:%02X:%02X:%02X:%02X", if_entry.if_index, if_entry.mac_addr.ether_addr_octet[0], if_entry.mac_addr.ether_addr_octet[1], if_entry.mac_addr.ether_addr_octet[2], if_entry.mac_addr.ether_addr_octet[3], if_entry.mac_addr.ether_addr_octet[4], if_entry.mac_addr.ether_addr_octet[5]);
				break;
			case NW_API_DB_ERROR:
				write_log(LOG_CRIT, "Received fatal error from DBs when modifying IF: IF ID = %d, MAC = %02X:%02X:%02X:%02X:%02X:%02X", if_entry.if_index, if_entry.mac_addr.ether_addr_octet[0], if_entry.mac_addr.ether_addr_octet[1], if_entry.mac_addr.ether_addr_octet[2], if_entry.mac_addr.ether_addr_octet[3], if_entry.mac_addr.ether_addr_octet[4], if_entry.mac_addr.ether_addr_octet[5]);
				nw_db_manager_exit_with_error();
				break;
			}
		}
		/* Next link */
		link_entry = (struct rtnl_link *)nl_cache_get_next((struct nl_object *)link_entry);
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
			if (nw_db_manager_route_to_fib_entry(route_entry, &fib_entry)) {
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
	struct nw_api_arp_entry arp_entry;

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
			if (nw_db_manager_neighbor_to_arp_entry(neighbor, &arp_entry)) {
				if (nw_api_add_arp_entry(&arp_entry) == NW_API_DB_ERROR) {
					write_log(LOG_CRIT, "Received fatal error from NW DBs. exiting.");
					nw_db_manager_exit_with_error();
				}
			}
		}
		/* Next neighbor */
		neighbor = (struct rtnl_neigh *)nl_cache_get_next((struct nl_object *)neighbor);
	}
}

/******************************************************************************
 * \brief       IFC table callback function.
 *              Updates the NPS IF table according to the received action.
 *
 * \return      void
 */
void nw_db_manager_ifc_cb(struct nl_cache __attribute__((__unused__))*cache, struct nl_object *obj, int action, void __attribute__((__unused__))*data)
{
	struct rtnl_link *link = (struct rtnl_link *) obj;
	struct nw_api_if_entry if_entry;
	bool is_if_up;
	enum nw_api_rc nw_ret = NW_API_OK;

	if (nw_db_manager_link_to_if_entry(link, &if_entry) == true) {
		is_if_up = rtnl_link_get_flags(link) & IFF_UP;
		switch (action) {
		case NL_ACT_NEW:
			nw_ret = nw_api_modify_if_entry(&if_entry);
			if (nw_ret == NW_API_OK) {
				write_log(LOG_INFO, "IF modified successfully. if_id %d, MAC = %02X:%02X:%02X:%02X:%02X:%02X", if_entry.if_index, if_entry.mac_addr.ether_addr_octet[0], if_entry.mac_addr.ether_addr_octet[1], if_entry.mac_addr.ether_addr_octet[2], if_entry.mac_addr.ether_addr_octet[3], if_entry.mac_addr.ether_addr_octet[4], if_entry.mac_addr.ether_addr_octet[5]);
				if (is_if_up) {
					nw_ret = nw_api_enable_if_entry(&if_entry);
					if (nw_ret == NW_API_OK) {
						write_log(LOG_INFO, "IF enabled successfully. if_id %d, MAC = %02X:%02X:%02X:%02X:%02X:%02X", if_entry.if_index, if_entry.mac_addr.ether_addr_octet[0], if_entry.mac_addr.ether_addr_octet[1], if_entry.mac_addr.ether_addr_octet[2], if_entry.mac_addr.ether_addr_octet[3], if_entry.mac_addr.ether_addr_octet[4], if_entry.mac_addr.ether_addr_octet[5]);
					}
				}
			}
			break;
		case NL_ACT_DEL:
			write_log(LOG_INFO, "Disable IF. if_id %d", if_entry.if_index);
			nw_ret = nw_api_disable_if_entry(&if_entry);
			if (nw_ret == NW_API_OK) {
				write_log(LOG_INFO, "IF disabled successfully. IF ID = %d", if_entry.if_index);
			}
			break;
		case NL_ACT_CHANGE:
			nw_ret = nw_api_modify_if_entry(&if_entry);
			if (nw_ret == NW_API_OK) {
				write_log(LOG_INFO, "IF modified successfully. if_id %d, MAC = %02X:%02X:%02X:%02X:%02X:%02X", if_entry.if_index, if_entry.mac_addr.ether_addr_octet[0], if_entry.mac_addr.ether_addr_octet[1], if_entry.mac_addr.ether_addr_octet[2], if_entry.mac_addr.ether_addr_octet[3], if_entry.mac_addr.ether_addr_octet[4], if_entry.mac_addr.ether_addr_octet[5]);
				if (is_if_up) {
					nw_ret = nw_api_enable_if_entry(&if_entry);
					if (nw_ret == NW_API_OK) {
						write_log(LOG_INFO, "IF enabled successfully. if_id %d, MAC = %02X:%02X:%02X:%02X:%02X:%02X", if_entry.if_index, if_entry.mac_addr.ether_addr_octet[0], if_entry.mac_addr.ether_addr_octet[1], if_entry.mac_addr.ether_addr_octet[2], if_entry.mac_addr.ether_addr_octet[3], if_entry.mac_addr.ether_addr_octet[4], if_entry.mac_addr.ether_addr_octet[5]);
					}
				} else {
					nw_ret = nw_api_disable_if_entry(&if_entry);
					if (nw_ret == NW_API_OK) {
						write_log(LOG_INFO, "IF disabled successfully. IF ID = %d", if_entry.if_index);
					}
				}
			}
			break;
		}
		if (nw_ret == NW_API_DB_ERROR) {
			write_log(LOG_CRIT, "Received fatal error from NW DBs while updating IF entry. IF ID = %d. exiting.", if_entry.if_index);
			nw_db_manager_exit_with_error();
		}
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
			if (nw_db_manager_route_to_fib_entry(route_entry, &fib_entry)) {
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
	struct rtnl_neigh *neighbor = (struct rtnl_neigh *)obj;
	struct nw_api_arp_entry arp_entry;
	enum nw_api_rc rc = NW_API_OK;
	/* Take only IPv4 entries.
	 * TODO: when adding IPv6 capabilities, this should be revisited.
	 */
	if (rtnl_neigh_get_family(neighbor) == AF_INET) {
		if (nw_db_manager_neighbor_to_arp_entry(neighbor, &arp_entry)) {
			switch (action) {
			case NL_ACT_NEW:
				if (valid_neighbor(neighbor)) {
					rc = nw_api_add_arp_entry(&arp_entry);
				}
				break;
			case NL_ACT_DEL:
				rc = nw_api_remove_arp_entry(&arp_entry);
				break;
			case NL_ACT_CHANGE:
				if (valid_neighbor(neighbor)) {
					rc = nw_api_modify_arp_entry(&arp_entry);
				} else {
					rc = nw_api_remove_arp_entry(&arp_entry);
				}
				break;
			}
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
	if (rc == NW_API_DB_ERROR) {
		write_log(LOG_CRIT, "Received fatal error from NW DBs. exiting.");
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

