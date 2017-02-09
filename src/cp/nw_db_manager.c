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
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/if.h>

/* libnl-3 includes */
#include <netlink/netlink.h>
#include <netlink/cache.h>

#include <netlink/fib_lookup/request.h>
#include <netlink/fib_lookup/lookup.h>

/* Project includes */
#include "log.h"
#include "nw_db_manager.h"


#ifdef CONFIG_TC
#include "tc_api.h"
#include "tc_manager.h"
#endif  /* def CONFIG_TC */

/* Function Definition */
void nw_db_manager_init(void);
void nw_db_manager_delete(void);
void nw_db_manager_table_init(void);
void nw_db_manager_poll(void);
void nw_db_manager_if_table_init(void);
void nw_db_manager_addr_table_init(void);
void nw_db_manager_arp_table_init(void);
void nw_db_manager_fib_table_init(void);
void nw_db_manager_arp_cb(struct nl_cache *cache, struct nl_object *obj,
			  int action, void *data);
void nw_db_manager_fib_cb(struct nl_cache *cache, struct nl_object *obj,
			  int action, void *data);
void nw_db_manager_ifc_cb(struct nl_cache *cache, struct nl_object *obj,
			  int action, void *data);
void nw_db_manager_addr_cb(struct nl_cache *cache, struct nl_object *obj,
			  int action, void *data);

bool valid_neighbor(struct rtnl_neigh *neighbor);
bool valid_route_entry(struct rtnl_route *route_entry);

/* Globals Definition */
struct nl_cache_mngr *network_cache_mngr;
struct nw_db_manager_ops *nw_db_manager_ops;
extern bool cancel_application_flag;

#define NW_DB_MANAGER_NEIGHBOR_FILTERED_STATE \
	(NUD_INCOMPLETE | NUD_FAILED | NUD_NOARP)


/******************************************************************************
 * \brief    a helper function for nl address print
 *
 * \return   void
 */
char *nl_addr_to_str(struct nl_addr *addr)
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
void nw_db_manager_main(struct nw_db_manager_ops *nw_ops)
{
	nw_db_manager_ops = nw_ops;
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
	nw_db_manager_addr_table_init();
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
	while (cancel_application_flag == false) {
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
	int ret;
	bool rc;
	struct nl_cache *ifc_cache;
	struct rtnl_link *link_entry;

	ret = nl_cache_mngr_add(network_cache_mngr, "route/link", &nw_db_manager_ifc_cb, NULL, &ifc_cache);
	if (ret < 0) {
		write_log(LOG_CRIT, "Unable to add cache route/link: %s", nl_geterror(ret));
		nw_db_manager_exit_with_error();
	}

	/* Iterate on ifc cache */
	link_entry = (struct rtnl_link *)nl_cache_get_first(ifc_cache);
	while ((link_entry != NULL)) {
		if (rtnl_link_get_flags(link_entry) & IFF_UP) {
			rc = nw_db_manager_ops->modify_if(link_entry);
			if (rc == true) {
				rc = nw_db_manager_ops->enable_if(link_entry);
			}
			if (rc == false) {
				write_log(LOG_CRIT, "Fatal error. exiting.");
				nw_db_manager_exit_with_error();
			}
		}

		/* Next link */
		link_entry = (struct rtnl_link *)nl_cache_get_next((struct nl_object *)link_entry);
	}

}

/******************************************************************************
 * \brief       Address table init.
 *              Registers a callback function for table changes.
 *              Reads current address table and writes all entries to NPS
 *              Interface table.
 *
 * \return      void
 */
void nw_db_manager_addr_table_init(void)
{
	int ret;
	struct nl_cache *filtered_addr_cache;
	struct nl_cache *addr_cache;
	struct rtnl_addr *addr_entry = rtnl_addr_alloc();

	ret = nl_cache_mngr_add(network_cache_mngr, "route/addr", &nw_db_manager_addr_cb, NULL, &addr_cache);
	if (ret < 0) {
		write_log(LOG_CRIT, "Unable to add cache route/addr: %s", nl_geterror(ret));
		nw_db_manager_exit_with_error();
	}

	/* Take only IPv4 entries.
	 * TODO: when adding IPv6 capabilities, this should be revisited.
	 */
	rtnl_addr_set_family(addr_entry, AF_INET);
	filtered_addr_cache = nl_cache_subset(addr_cache, (struct nl_object *)addr_entry);

	/* Iterate on addr cache */
	addr_entry = (struct rtnl_addr *)nl_cache_get_first(filtered_addr_cache);
	while (addr_entry != NULL) {
		 /* Add address to IF table */
		if (nw_db_manager_ops->add_if_addr(addr_entry) == false) {
			write_log(LOG_CRIT, "Fatal error. exiting.");
			nw_db_manager_exit_with_error();
		}
		/* Next addr */
		addr_entry = (struct rtnl_addr *)nl_cache_get_next((struct nl_object *)addr_entry);
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
		write_log(LOG_CRIT, "Unable to add cache route/route: %s", nl_geterror(ret));
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
			if (nw_db_manager_ops->add_fib_entry(route_entry) == false) {
				write_log(LOG_CRIT, "Fatal error. exiting.");
				nw_db_manager_exit_with_error();
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
			if (nw_db_manager_ops->add_arp_entry(neighbor) == false) {
				write_log(LOG_CRIT, "Fatal error. exiting.");
				nw_db_manager_exit_with_error();
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
	bool rc = true;
	bool is_if_up = rtnl_link_get_flags(link) & IFF_UP;

	switch (action) {
	case NL_ACT_NEW:
		rc = nw_db_manager_ops->modify_if(link);
		if (rc == true && is_if_up) {
			rc = nw_db_manager_ops->enable_if(link);
		}
		break;
	case NL_ACT_DEL:
		rc = nw_db_manager_ops->disable_if(link);
		break;
	case NL_ACT_CHANGE:
		rc = nw_db_manager_ops->modify_if(link);
		if (rc == true) {
			if (is_if_up) {
				rc = nw_db_manager_ops->enable_if(link);
			} else {
				rc = nw_db_manager_ops->disable_if(link);
			}
		}
		break;
	}

	if (rc == false) {
		write_log(LOG_CRIT, "Fatal error. exiting.");
		nw_db_manager_exit_with_error();
	}
}

/******************************************************************************
 * \brief       Address table callback function.
 *              Updates the NPS IF table according to the received action.
 *
 * \return      void
 */
void nw_db_manager_addr_cb(struct nl_cache __attribute__((__unused__))*cache, struct nl_object *obj, int action, void __attribute__((__unused__))*data)
{
	struct rtnl_addr *addr = (struct rtnl_addr *) obj;
	bool rc = true;

	if (rtnl_addr_get_family(addr) == AF_INET) {
		switch (action) {
		case NL_ACT_NEW:
			rc = nw_db_manager_ops->add_if_addr(addr);
			break;
		case NL_ACT_DEL:
			rc = nw_db_manager_ops->remove_if_addr(addr);
			break;
		}
	}

	if (rc == false) {
		write_log(LOG_CRIT, "Fatal error. exiting.");
		nw_db_manager_exit_with_error();
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
	bool rc = true;

	if (valid_route_entry(route_entry)) {
		/* Take only IPv4 entries.
		 * TODO: when adding IPv6 capabilities, this should be revisited.
		 */
		if (rtnl_route_get_family(route_entry) == AF_INET) {
			switch (action) {
			case NL_ACT_NEW:
				rc = nw_db_manager_ops->add_fib_entry(route_entry);
				break;
			case NL_ACT_DEL:
				rc = nw_db_manager_ops->remove_fib_entry(route_entry);
				break;
			case NL_ACT_CHANGE:
				rc = nw_db_manager_ops->modify_fib_entry(route_entry);
				break;
			}
			if (rc == false) {
				write_log(LOG_CRIT, "Fatal error. exiting.");
				nw_db_manager_exit_with_error();
			}
		} else {
			switch (action) {
			case NL_ACT_NEW:
				write_log(LOG_NOTICE, "info: FIB entry from address family %d was not added. Address = %s", rtnl_route_get_family(route_entry), nl_addr_to_str(rtnl_route_get_dst(route_entry)));
				break;
			case NL_ACT_DEL:
				write_log(LOG_NOTICE, "info: FIB entry from address family %d was not deleted. Address = %s", rtnl_route_get_family(route_entry), nl_addr_to_str(rtnl_route_get_dst(route_entry)));
				break;
			case NL_ACT_CHANGE:
				write_log(LOG_NOTICE, "info: FIB entry from address family %d has not changed. Address = %s", rtnl_route_get_family(route_entry), nl_addr_to_str(rtnl_route_get_dst(route_entry)));
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
	bool rc = true;

	/* Take only IPv4 entries.
	 * TODO: when adding IPv6 capabilities, this should be revisited.
	 */
	if (rtnl_neigh_get_family(neighbor) == AF_INET) {
		switch (action) {
		case NL_ACT_NEW:
			if (valid_neighbor(neighbor)) {
				rc = nw_db_manager_ops->add_arp_entry(neighbor);
			}
			break;
		case NL_ACT_DEL:
			rc = nw_db_manager_ops->remove_arp_entry(neighbor);
			break;
		case NL_ACT_CHANGE:
			if (valid_neighbor(neighbor)) {
				rc = nw_db_manager_ops->modify_arp_entry(neighbor);
			} else {
				rc = nw_db_manager_ops->remove_arp_entry(neighbor);
			}
		}
	} else {
		switch (action) {
		case NL_ACT_NEW:
			write_log(LOG_NOTICE, "info: ARP entry from address family %d was not added. Address = %s", rtnl_neigh_get_family(neighbor), nl_addr_to_str(rtnl_neigh_get_dst(neighbor)));
			break;
		case NL_ACT_DEL:
			write_log(LOG_NOTICE, "info: ARP entry from address family %d was not deleted. Address = %s", rtnl_neigh_get_family(neighbor), nl_addr_to_str(rtnl_neigh_get_dst(neighbor)));
			break;
		case NL_ACT_CHANGE:
			write_log(LOG_NOTICE, "info: ARP entry from address family %d has not changed. Address = %s", rtnl_neigh_get_family(neighbor), nl_addr_to_str(rtnl_neigh_get_dst(neighbor)));
			break;
		}
	}

	if (rc == false) {
		write_log(LOG_CRIT, "Fatal error. exiting.");
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
	cancel_application_flag = true;
	nw_db_manager_delete();
	kill(getpid(), SIGTERM);
	pthread_exit(NULL);
}

