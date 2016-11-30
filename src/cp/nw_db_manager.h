/*
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
*  Project:             NPS400 ALVS application
*  File:                nw_db_manager.h
*  Desc:                Network DB management include file
*/

#ifndef CP_NW_DB_MANAGER_H_
#define CP_NW_DB_MANAGER_H_

#include <netlink/route/route.h>
#include <netlink/route/neighbour.h>
#include <netlink/route/link.h>
#include <netlink/route/addr.h>

/* NW DB manager operations for ARP\FIB\IFC control.
 * Called on netlink changes with libnl structs and should initiate the relevant NW APIs.
 * On false return value, nw_db_manager will exit with an error.
 */
struct nw_db_manager_ops {
	bool (*add_fib_entry)(struct rtnl_route *);
	/*called when new fib entry is added*/
	bool (*remove_fib_entry)(struct rtnl_route *);
	/*called when fib entry is deleted*/
	bool (*modify_fib_entry)(struct rtnl_route *);
	/*called when fib entry is modified*/
	bool (*add_arp_entry)(struct rtnl_neigh *);
	/*called when arp entry is added*/
	bool (*remove_arp_entry)(struct rtnl_neigh *);
	/*called when arp entry is deleted*/
	bool (*modify_arp_entry)(struct rtnl_neigh *);
	/*called when arp entry is modified*/
	bool (*add_if)(struct rtnl_link *link);
	/*called when interface is enabled\created*/
	bool (*remove_if)(struct rtnl_link *link);
	/*called when interface is disabled*/
	bool (*modify_if)(struct rtnl_link *link);
	/*called when interface is modified*/
	bool (*add_if_addr)(struct rtnl_addr *addr);
	/*called when address is added*/
	bool (*remove_if_addr)(struct rtnl_addr *addr);
	/*called when address is deleted*/
};

/******************************************************************************
 * \brief    nl address print
 *
 * \return   void
 */
char *nl_addr_to_str(struct nl_addr *addr);

/******************************************************************************
 * \brief	  Network thread main application.
 *
 * \return	  void
 */
void nw_db_manager_main(struct nw_db_manager_ops *nw_dp_ops);

/******************************************************************************
 * \brief    Raises SIGTERM signal to main thread and exits the thread.
 *           Deletes the DB manager.
 *
 * \return   void
 */
void nw_db_manager_exit_with_error(void);

#endif /* CP_NW_DB_MANAGER_H_ */
