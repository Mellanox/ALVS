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
*  File:                nw_ops.h
*  Desc:                Network operations for NW DB manager.
*
*/

#ifndef _NW_OPS_H_
#define _NW_OPS_H_

#include <stdbool.h>
#include <netlink/route/route.h>
#include <nw_db_manager.h>

/******************************************************************************
 * \brief    Add\Remove\Modify FIB entry.
 *           Get libnl rtnl_route and return false only on fatal error.
 */
bool nw_ops_add_fib_entry(struct rtnl_route *route_entry);
bool nw_ops_remove_fib_entry(struct rtnl_route *route_entry);
bool nw_ops_modify_fib_entry(struct rtnl_route *route_entry);

/******************************************************************************
 * \brief    Add\Remove\Modify ARP entry.
 *           Get libnl rtnl_neigh and return false only on fatal error.
 */
bool nw_ops_add_arp_entry(struct rtnl_neigh *neighbor);
bool nw_ops_remove_arp_entry(struct rtnl_neigh *neighbor);
bool nw_ops_modify_arp_entry(struct rtnl_neigh *neighbor);

/******************************************************************************
 * \brief    Add\Remove\Modify IF entry.
 *           Get libnl rtnl_link and return false only on fatal error.
 */
bool nw_ops_add_if(struct rtnl_link *link);
bool nw_ops_remove_if(struct rtnl_link *link);
bool nw_ops_modify_if(struct rtnl_link *link);

/******************************************************************************
 * \brief    Add\Remove IF address.
 *           Get libnl rtnl_addr and return false only on fatal error.
 */
bool nw_ops_add_if_addr(struct rtnl_addr *addr_entry);
bool nw_ops_remove_if_addr(struct rtnl_addr *addr_entry);

/*init NW DB manager ops with nw_ops*/
struct nw_db_manager_ops nw_ops = {
	.add_fib_entry = &nw_ops_add_fib_entry,
	.remove_fib_entry = &nw_ops_remove_fib_entry,
	.modify_fib_entry = &nw_ops_modify_fib_entry,
	.add_arp_entry = &nw_ops_add_arp_entry,
	.remove_arp_entry = &nw_ops_remove_arp_entry,
	.modify_arp_entry = &nw_ops_modify_arp_entry,
	.add_if = &nw_ops_add_if,
	.remove_if = &nw_ops_remove_if,
	.modify_if = &nw_ops_modify_if,
	.add_if_addr = &nw_ops_add_if_addr,
	.remove_if_addr = &nw_ops_remove_if_addr
};

#endif /* _NW_OPS_H_ */
