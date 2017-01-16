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
*  File:                nw_ops.c
*  Desc:                Implementation of network operations for NW DB manager.
*
*/

/* Project includes */
#include "log.h"
#include "nw_api.h"
#include "nw_conf.h"
#include "nw_ops.h"

/* linux includes */
#include <linux/if.h>


/******************************************************************************
 * \brief    Convert NL address to NW API address
 *
 * \return	None.
 */
void nl_addr_to_nw_api_addr(struct nw_api_inet_addr *nw_api_addr, struct nl_addr *nl_addr, int af)
{
	nw_api_addr->af = af;
	if (nw_api_addr->af == AF_INET) {
		nw_api_addr->in.s_addr = *(uint32_t *)nl_addr_get_binary_addr(nl_addr);
	} else if (nw_api_addr->af == AF_INET6) {
		memcpy((void *)&nw_api_addr->in6, nl_addr_get_binary_addr(nl_addr), sizeof(struct in6_addr));
	}
}

/******************************************************************************
 * \brief    Convert Linux neighbor entry to nw api arp entry
 *           Check if ARP entry belongs to a relevant IF (valid ARP entry)
 * \return	bool:	true - valid neighbor entry
 *			false- neighbor not valid
 */
bool neighbor_to_arp_entry(struct rtnl_neigh *neighbor, struct nw_api_arp_entry *arp_entry)
{
	struct nl_addr *mac = rtnl_neigh_get_lladdr(neighbor);
	int if_index;

	memset(arp_entry, 0, sizeof(struct nw_api_arp_entry));
	if_index = if_lookup_by_index(rtnl_neigh_get_ifindex(neighbor));
	if (if_index == -1) {
		return false;
	}
	arp_entry->if_index = if_index;
	nl_addr_to_nw_api_addr(&arp_entry->ip_addr, rtnl_neigh_get_dst(neighbor), rtnl_neigh_get_family(neighbor));
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
bool route_to_fib_entry(struct rtnl_route *route_entry, struct nw_api_fib_entry *fib_entry)
{
	struct nl_addr *dst = rtnl_route_get_dst(route_entry);
	int if_index;

	memset(fib_entry, 0, sizeof(struct nw_api_fib_entry));

	fib_entry->route_type = rtnl_route_get_type(route_entry);
	fib_entry->mask_length = nl_addr_get_prefixlen(dst);
	nl_addr_to_nw_api_addr(&fib_entry->dest, dst, rtnl_route_get_family(route_entry));
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
			nl_addr_to_nw_api_addr(&fib_entry->next_hop, next_hop_addr, fib_entry->dest.af);
		}
	}
	return true;
}

/******************************************************************************
 * \brief    Convert Linux link entry to nw api IF entry
 *
 * \return	bool:	true - valid IF entry
 *			false- IF not valid
 */
bool link_to_if_entry(struct rtnl_link *link, struct nw_api_if_entry *if_entry)
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
 * \brief    Convert Linux addr entry to nw api addr entry
 *           Check if addr entry belongs to a relevant IF (valid addr entry)
 * \return	bool:	true - valid addr entry
 *			false- addr not valid
 */
bool addr_to_local_addr_entry(struct rtnl_addr *nl_addr, struct nw_api_local_addr_entry *addr_entry)
{
	int if_index;

	if_index = if_lookup_by_name(rtnl_addr_get_label(nl_addr));
	if (if_index == -1) {
		return false;
	}
	addr_entry->if_index = if_index;
	nl_addr_to_nw_api_addr(&addr_entry->ip_addr, rtnl_addr_get_local(nl_addr), rtnl_addr_get_family(nl_addr));

	return true;
}

bool nw_ops_add_fib_entry(struct rtnl_route *route_entry)
{
	struct nw_api_fib_entry fib_entry;

	write_log(LOG_DEBUG, "FIB ADD entry: %s", nl_addr_to_str(rtnl_route_get_dst(route_entry)));

	if (route_to_fib_entry(route_entry, &fib_entry) == false) {
		return true;
	}

	switch (nw_api_add_fib_entry(&fib_entry)) {
	case NW_API_OK:
		break;
	case NW_API_DB_ERROR:
		write_log(LOG_CRIT, "Received fatal error from DBs when adding FIB entry: addr = %s.", nl_addr_to_str(rtnl_route_get_dst(route_entry)));
		return false;
	default:
		write_log(LOG_NOTICE, "Problem adding FIB entry: addr = %s", nl_addr_to_str(rtnl_route_get_dst(route_entry)));
	}

	return true;
}

bool nw_ops_remove_fib_entry(struct rtnl_route *route_entry)
{
	struct nw_api_fib_entry fib_entry;

	write_log(LOG_DEBUG, "FIB DELETE entry: %s", nl_addr_to_str(rtnl_route_get_dst(route_entry)));
	if (route_to_fib_entry(route_entry, &fib_entry) == false) {
		return true;
	}

	switch (nw_api_remove_fib_entry(&fib_entry)) {
	case NW_API_OK:
		break;
	case NW_API_DB_ERROR:
		write_log(LOG_CRIT, "Received fatal error from DBs when deleting FIB entry: addr = %s.", nl_addr_to_str(rtnl_route_get_dst(route_entry)));
		return false;
	default:
		write_log(LOG_NOTICE, "Problem deleting FIB entry: addr = %s", nl_addr_to_str(rtnl_route_get_dst(route_entry)));
	}

	return true;
}

bool nw_ops_modify_fib_entry(struct rtnl_route *route_entry)
{
	struct nw_api_fib_entry fib_entry;

	write_log(LOG_DEBUG, "FIB CHANGE entry: %s", nl_addr_to_str(rtnl_route_get_dst(route_entry)));
	if (route_to_fib_entry(route_entry, &fib_entry) == false) {
		return true;
	}

	switch (nw_api_modify_fib_entry(&fib_entry)) {
	case NW_API_OK:
		break;
	case NW_API_DB_ERROR:
		write_log(LOG_CRIT, "Received fatal error from DBs when modifying FIB entry: addr = %s.", nl_addr_to_str(rtnl_route_get_dst(route_entry)));
		return false;
	default:
		write_log(LOG_NOTICE, "Problem modifying FIB entry: addr = %s", nl_addr_to_str(rtnl_route_get_dst(route_entry)));
	}

	return true;
}

bool nw_ops_add_arp_entry(struct rtnl_neigh *neighbor)
{
	struct nw_api_arp_entry arp_entry;

	if (neighbor_to_arp_entry(neighbor, &arp_entry) == false) {
		return true;
	}

	if (nw_api_add_arp_entry(&arp_entry) == NW_API_DB_ERROR) {
		write_log(LOG_CRIT, "Received fatal error from NW DBs.");
		return false;
	}

	return true;
}

bool nw_ops_remove_arp_entry(struct rtnl_neigh *neighbor)
{
	struct nw_api_arp_entry arp_entry;

	if (neighbor_to_arp_entry(neighbor, &arp_entry) == false) {
		return true;
	}

	if (nw_api_remove_arp_entry(&arp_entry) == NW_API_DB_ERROR) {
		write_log(LOG_CRIT, "Received fatal error from NW DBs.");
		return false;
	}

	return true;
}

bool nw_ops_modify_arp_entry(struct rtnl_neigh *neighbor)
{
	struct nw_api_arp_entry arp_entry;

	if (neighbor_to_arp_entry(neighbor, &arp_entry) == false) {
		return true;
	}

	if (nw_api_modify_arp_entry(&arp_entry) == NW_API_DB_ERROR) {
		write_log(LOG_CRIT, "Received fatal error from NW DBs.");
		return false;
	}

	return true;
}

bool nw_ops_enable_if(struct rtnl_link *link)
{
	struct nw_api_if_entry if_entry;
	enum nw_api_rc nw_ret = NW_API_OK;

	if (link_to_if_entry(link, &if_entry) == false) {
		return true;
	}

	nw_ret = nw_api_enable_if_entry(&if_entry);
	if (nw_ret == NW_API_OK) {
		write_log(LOG_INFO, "IF enabled successfully. if_id %d, MAC = %02X:%02X:%02X:%02X:%02X:%02X", if_entry.if_index, if_entry.mac_addr.ether_addr_octet[0], if_entry.mac_addr.ether_addr_octet[1], if_entry.mac_addr.ether_addr_octet[2], if_entry.mac_addr.ether_addr_octet[3], if_entry.mac_addr.ether_addr_octet[4], if_entry.mac_addr.ether_addr_octet[5]);
	}

	if (nw_ret == NW_API_DB_ERROR) {
		write_log(LOG_CRIT, "Received fatal error from NW DBs while enabling IF. IF ID = %d.", if_entry.if_index);
		return false;
	}

	return true;
}

bool nw_ops_disable_if(struct rtnl_link *link)
{
	struct nw_api_if_entry if_entry;
	enum nw_api_rc nw_ret = NW_API_OK;

	if (link_to_if_entry(link, &if_entry) == false) {
		return true;
	}

	nw_ret = nw_api_disable_if_entry(&if_entry);
	if (nw_ret == NW_API_OK) {
		write_log(LOG_INFO, "IF disabled successfully. IF ID = %d", if_entry.if_index);
	}

	if (nw_ret == NW_API_DB_ERROR) {
		write_log(LOG_CRIT, "Received fatal error from NW DBs while disabling IF. IF ID = %d.", if_entry.if_index);
		return false;
	}

	return true;
}

bool nw_ops_modify_if(struct rtnl_link *link)
{
	struct nw_api_if_entry if_entry;
	enum nw_api_rc nw_ret;

	if (link_to_if_entry(link, &if_entry) == false) {
		return true;
	}

	nw_ret = nw_api_modify_if_entry(&if_entry);
	if (nw_ret == NW_API_OK) {
		write_log(LOG_INFO, "IF modified successfully. if_id %d, MAC = %02X:%02X:%02X:%02X:%02X:%02X", if_entry.if_index, if_entry.mac_addr.ether_addr_octet[0], if_entry.mac_addr.ether_addr_octet[1], if_entry.mac_addr.ether_addr_octet[2], if_entry.mac_addr.ether_addr_octet[3], if_entry.mac_addr.ether_addr_octet[4], if_entry.mac_addr.ether_addr_octet[5]);
	}

	if (nw_ret == NW_API_DB_ERROR) {
		write_log(LOG_CRIT, "Received fatal error from NW DBs while modifying IF. IF ID = %d.", if_entry.if_index);
		return false;
	}

	return true;
}

bool nw_ops_add_if_addr(struct rtnl_addr *addr_entry)
{
	struct nw_api_local_addr_entry local_addr_entry;
	struct nl_addr *addr = rtnl_addr_get_local(addr_entry);
	enum nw_api_rc nw_ret;

	write_log(LOG_DEBUG, "New Address received: IF %d, IP %s, label %s", rtnl_addr_get_ifindex(addr_entry), nl_addr_to_str(addr), rtnl_addr_get_label(addr_entry));
	if (addr_to_local_addr_entry(addr_entry, &local_addr_entry) == false) {
		write_log(LOG_DEBUG, "Address will not be handled");
		return true;
	}

	nw_ret = nw_api_add_local_addr_entry(&local_addr_entry);
	if (nw_ret == NW_API_OK) {
		write_log(LOG_INFO, "Address was added to IF successfully. IF %d, IP %s", local_addr_entry.if_index, nl_addr_to_str(addr));
	} else if (nw_ret == NW_API_DB_ERROR) {
		write_log(LOG_CRIT, "Received fatal error from NW DBs while adding address to IF. IF %d, IP %s", local_addr_entry.if_index, nl_addr_to_str(addr));
		return false;
	}

	return true;
}
bool nw_ops_remove_if_addr(struct rtnl_addr *addr_entry)
{
	struct nw_api_local_addr_entry local_addr_entry;
	struct nl_addr *addr = rtnl_addr_get_local(addr_entry);
	enum nw_api_rc nw_ret;

	write_log(LOG_DEBUG, "Remove Address received: IF %d, IP %s, label %s", rtnl_addr_get_ifindex(addr_entry), nl_addr_to_str(addr), rtnl_addr_get_label(addr_entry));
	if (addr_to_local_addr_entry(addr_entry, &local_addr_entry) == false) {
		write_log(LOG_DEBUG, "Address will not be handled");
		return true;
	}

	nw_ret = nw_api_remove_local_addr_entry(&local_addr_entry);
	if (nw_ret == NW_API_OK) {
		write_log(LOG_INFO, "Address was removed from IF successfully. IF %d, IP %s", local_addr_entry.if_index, nl_addr_to_str(addr));
	} else if (nw_ret == NW_API_DB_ERROR) {
		write_log(LOG_CRIT, "Received fatal error from NW DBs while removing address from IF. IF %d, IP %s", local_addr_entry.if_index, nl_addr_to_str(addr));
		return false;
	}

	return true;
}

