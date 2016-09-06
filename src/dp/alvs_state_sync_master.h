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
*  Project:             NPS400 ALVS application
*  File:                alvs_state_sync_master.h
*  Desc:                state sync of alvs - master functionality
*/

#ifndef ALVS_STATE_SYNC_MASTER_H_
#define ALVS_STATE_SYNC_MASTER_H_

#include "defs.h"
#include "alvs_defs.h"
#include "alvs_utils.h"
#include "alvs_server.h"
#include "application_search_defs.h"
#include "nw_routing.h"

/******************************************************************************
 * \brief       update network header (ipv4, udp) length fields according to
 *              number of connections and update ipv4 checksum.
 *
 * \return      void
 *
 */
static __always_inline
void alvs_state_sync_update_net_hdr_len(struct net_hdr *net_hdr_info, int conn_count)
{
	/*update udp and ipv4 length fields*/
	net_hdr_info->udp.len = sizeof(struct udphdr)
		+ sizeof(struct alvs_state_sync_header)
		+ (conn_count * sizeof(struct alvs_state_sync_conn));

	net_hdr_info->ipv4.tot_len = sizeof(struct iphdr) + net_hdr_info->udp.len;

	/*update ipv4 checksum*/
	ezframe_update_ipv4_checksum(&net_hdr_info->ipv4);
}

/******************************************************************************
 * \brief       update network header (ipv4, udp) with ALVS state sync details.
 *              add the given source ip and update ipv4 checksum.
 *
 * \return      void
 *
 */
static __always_inline
void alvs_state_sync_set_net_hdr(struct net_hdr *net_hdr_info, int conn_count, in_addr_t source_ip)
{
	struct in_addr dest_ip;

	/*fill fields of UDP header*/
	net_hdr_info->udp.source = ALVS_STATE_SYNC_DST_PORT;
	net_hdr_info->udp.dest = ALVS_STATE_SYNC_DST_PORT;
	net_hdr_info->udp.check = 0;

	/*fill fields of IPV4 header*/
	net_hdr_info->ipv4.version = IPVERSION;
	net_hdr_info->ipv4.ihl = sizeof(struct iphdr)/4;
	net_hdr_info->ipv4.tos = 0;
	net_hdr_info->ipv4.id = 0;
	net_hdr_info->ipv4.frag_off = IP_DF;
	net_hdr_info->ipv4.ttl = 1;
	net_hdr_info->ipv4.protocol = IPPROTO_UDP;
	net_hdr_info->ipv4.check = 0;

	/*get IP from string*/
	inet_aton(ALVS_STATE_SYNC_DST_IP, &dest_ip);

	net_hdr_info->ipv4.saddr = source_ip;
	net_hdr_info->ipv4.daddr = dest_ip.s_addr;

	/*update length fields and checksum*/
	alvs_state_sync_update_net_hdr_len(net_hdr_info, conn_count);
}

/******************************************************************************
 * \brief       perform network interface lookup and update ethernet header
 *              with source and destination mac addresses.
 *
 * \return      void
 *
 */
/*TODO move this logic to a new nw multicast routing module.
 * it should perform nw interface lookup, update L2 headers and route.
 * port logical id should be taken from application info table.
 */
static __always_inline
void alvs_state_sync_set_eth_hdr(struct ether_header *eth_header)
{
	uint8_t dst_eth_addr[6] = ALVS_STATE_SYNC_DST_MAC;

	/*lookup for network interface*/
	if (unlikely(nw_interface_lookup(USER_BASE_LOGICAL_ID) != 0)) {
		alvs_write_log(LOG_DEBUG, "sync master fail nw interface lookup - port id =%d!", 0);
		return;
	}

	/*fill ethernet source MAC*/
	ezdp_mem_copy((uint8_t *)eth_header->ether_shost,
		      cmem_nw.interface_result.mac_address.ether_addr_octet,
		      sizeof(struct ether_addr));

	eth_header->ether_type = ETHERTYPE_IP;

	/*fill ethernet destination MAC*/
	ezdp_mem_copy((uint8_t *)eth_header, dst_eth_addr, sizeof(struct ether_addr));
}

/******************************************************************************
 * \brief       update state synchronization header length fields according to
 *              number of connections.
 *
 * \return      void
 *
 */
static __always_inline
void alvs_state_sync_update_sync_hdr_len(struct alvs_state_sync_header *hdr, int conn_count)
{
	/*update sync header length fields*/
	hdr->size = sizeof(struct alvs_state_sync_header) +
		(conn_count * sizeof(struct alvs_state_sync_conn));
	hdr->conn_count = conn_count;
}

/******************************************************************************
 * \brief       update state synchronization header with the provided amount
 *              of connections to be followed and sync id.
 *
 * \return      void
 *
 */
static __always_inline
void alvs_state_sync_set_sync_hdr(struct alvs_state_sync_header *hdr, int conn_count, uint8_t sync_id)
{
	hdr->syncid = sync_id;
	hdr->version = ALVS_STATE_SYNC_PROTO_VER;
	hdr->spare = 0;
	alvs_state_sync_update_sync_hdr_len(hdr, conn_count);
}

/******************************************************************************
 * \brief       update sync connection with current connection from
 *              conn_info_result (do not perform another lookup).
 *              take server ip & port from server_info_result, if bounded,
 *              without performing another lookup.
 *
 * \return      void
 *
 */
static __always_inline
void alvs_state_sync_set_sync_conn(struct alvs_state_sync_conn *sync_conn)
{
	sync_conn->type = IP_V4;
	sync_conn->protocol = cmem_alvs.conn_info_result.conn_class_key.protocol;
	sync_conn->version = 0;
	sync_conn->size = sizeof(struct alvs_state_sync_conn);
	sync_conn->flags = cmem_alvs.conn_info_result.conn_flags;
	sync_conn->state = cmem_alvs.conn_info_result.conn_state;
	sync_conn->timeout = alvs_util_get_conn_iterations((enum alvs_tcp_conn_state)sync_conn->state)
		* ALVS_TIMER_INTERVAL_SEC;
	sync_conn->fwmark = 0;
	sync_conn->client_port = cmem_alvs.conn_info_result.conn_class_key.client_port;
	sync_conn->virtual_port = cmem_alvs.conn_info_result.conn_class_key.virtual_port;
	sync_conn->client_addr = cmem_alvs.conn_info_result.conn_class_key.client_ip;
	sync_conn->virtual_addr = cmem_alvs.conn_info_result.conn_class_key.virtual_ip;
	if (likely(cmem_alvs.conn_info_result.bound == true)) {
		sync_conn->server_addr = cmem_alvs.server_info_result.server_ip;
		sync_conn->server_port = cmem_alvs.server_info_result.server_port;
	} else {
		sync_conn->server_addr = cmem_alvs.conn_info_result.server_addr;
		sync_conn->server_port = cmem_alvs.conn_info_result.server_port;
	}

	alvs_write_log(LOG_DEBUG, "sync connection state (caddr=0x%x, cport=%d, saddr=0x%x, sport=%d, vip=0x%x, state=%d, flags=0x%x",
		       sync_conn->client_addr,
		       sync_conn->client_port,
		       sync_conn->server_addr,
		       sync_conn->server_port,
		       sync_conn->virtual_addr,
		       sync_conn->state,
		       sync_conn->flags);
}

/******************************************************************************
 * \brief       update the first buffer of a new sync frame.
 *              include ethernet, network, sync headers and the
 *              current connection.
 *              add sync_id and source_ip to the relevant headers.
 *
 * \return      void
 *
 */
static __always_inline
void alvs_state_sync_first(in_addr_t source_ip, uint8_t sync_id)
{
	struct net_hdr  *net_hdr_info;
	struct alvs_state_sync_conn *sync_conn;
	struct alvs_state_sync_header *sync_hdr;
	struct ether_header *eth_header;

	cmem_alvs.conn_sync_state.amount_buffers = 1;
	cmem_alvs.conn_sync_state.conn_count = 1;

	/*partition first buffer*/
	eth_header = (struct ether_header *)frame_data;
	net_hdr_info = (struct net_hdr *)((uint8_t *)eth_header + sizeof(struct ether_header));
	sync_hdr = (struct alvs_state_sync_header *)((uint8_t *)net_hdr_info + sizeof(struct net_hdr));
	sync_conn = (struct alvs_state_sync_conn *)((uint8_t *)sync_hdr + sizeof(struct alvs_state_sync_header));
	cmem_alvs.conn_sync_state.current_base = (uint8_t *)sync_conn + sizeof(struct alvs_state_sync_conn);

	/*set length*/
	cmem_alvs.conn_sync_state.current_len = sizeof(struct ether_header) +
		sizeof(struct net_hdr) +
		sizeof(struct alvs_state_sync_header) +
		sizeof(struct alvs_state_sync_conn);

	/*set eth header*/
	alvs_state_sync_set_eth_hdr(eth_header);

	/*set net header*/
	alvs_state_sync_set_net_hdr(net_hdr_info, 1, source_ip);

	/*set sync msg header*/
	alvs_state_sync_set_sync_hdr(sync_hdr, 1, sync_id);

	/*add connection*/
	alvs_state_sync_set_sync_conn(sync_conn);
}

/******************************************************************************
 * \brief       send a sync frame with the current connection details.
 *              should be used when syncing connection due to state change.
 *
 * \return      void
 *
 */
static __always_inline
void alvs_state_sync_send_single(in_addr_t source_ip, uint8_t sync_id)
{
	/*create first sync frame buffer with current connection*/
	alvs_state_sync_first(source_ip, sync_id);

	/*verify buffer length*/
	assert(cmem_alvs.conn_sync_state.current_len <= (EZFRAME_BUF_DATA_SIZE - ALVS_STATE_SYNC_HEADROOM));

	/*create new frame*/
	if (unlikely(ezframe_new(&frame, frame_data, cmem_alvs.conn_sync_state.current_len, ALVS_STATE_SYNC_HEADROOM, 0) != 0)) {
		alvs_write_log(LOG_ERR, "sync master failed to create new sync frame");
		return;
	}

	alvs_write_log(LOG_DEBUG, "send single conn sync frame");
	/*TODO should be replaced with a call to nw multicast module*/
	nw_send_frame_to_network(&frame, frame_data, USER_BASE_LOGICAL_ID);
}

/******************************************************************************
 * \brief       add current buffer to sync frame. create new frame if it's
 *              the first buffer, or append if it's not the first.
 *              prepare conn_sync for the next buffer.
 *
 *              if failed to append buffer, free the current frame and reset
 *              the conn_sync state.
 *
 * \return      return the new\append buffer return code.
 *
 */
static __always_inline
uint32_t alvs_state_sync_add_buffer(void)
{
	int rc;

	/*verify proper amount of buffers*/
	assert((cmem_alvs.conn_sync_state.amount_buffers > 0) && (cmem_alvs.conn_sync_state.amount_buffers <= ALVS_STATE_SYNC_BUFFERS_LIMIT));

	if (unlikely(cmem_alvs.conn_sync_state.amount_buffers == 1)) {
		/*reduce the added headroom*/
		rc = ezframe_new(&frame, frame_data, cmem_alvs.conn_sync_state.current_len - ALVS_STATE_SYNC_HEADROOM, ALVS_STATE_SYNC_HEADROOM, 0);
		if (rc != 0) {
			alvs_write_log(LOG_ERR, "sync master failed to create new sync frame");
		}
	} else {
		rc = ezframe_append_buf(&frame, frame_data, cmem_alvs.conn_sync_state.current_len, 0);
		if (rc != 0) {
			alvs_write_log(LOG_ERR, "sync master failed to append buffer to sync frame");
			ezframe_free(&frame, 0);
		}
	}
	if (likely(rc == 0)) {
		cmem_alvs.conn_sync_state.amount_buffers += 1;
	} else {
		cmem_alvs.conn_sync_state.amount_buffers = 0;
		cmem_alvs.conn_sync_state.conn_count = 0;
	}
	cmem_alvs.conn_sync_state.current_base = frame_data;
	cmem_alvs.conn_sync_state.current_len = 0;
	return rc;
}

/******************************************************************************
 * \brief       send the aggregated sync frame.
 *              add the current buffer, update sync header with the amount of
 *              connection in the sync frame and send to network.
 *
 * \return      void
 *
 */
static __always_inline
void alvs_state_sync_send_aggr(void)
{
	int rc;
	uint8_t *frame_base;
	uint32_t frame_length;
	struct net_hdr  *net_hdr_info;
	struct alvs_state_sync_header *sync_hdr;

	/*check if there is anything to sync*/
	if (unlikely(cmem_alvs.conn_sync_state.amount_buffers == 0)) {
		alvs_write_log(LOG_DEBUG, "no aggregated connections to send");
		return;
	}

	/*add the last buffer*/
	if (likely(cmem_alvs.conn_sync_state.current_len > 0)) {
		if (unlikely(alvs_state_sync_add_buffer())) {
			alvs_write_log(LOG_DEBUG, "adding last buffer to sync frame");
			return;
		}
	}

	/*go back to first buffer*/
	if (likely(cmem_alvs.conn_sync_state.amount_buffers > 1)) {
		alvs_write_log(LOG_DEBUG, "switch back to sync frame first buffer");
		rc = ezframe_first_buf(&frame, 0);
		if (rc != 0) {
			ezframe_free(&frame, 0);
			return;
		}
	}

	/*update sync header with amount of connections*/
	if (likely(cmem_alvs.conn_sync_state.conn_count > 1)) {
		alvs_write_log(LOG_DEBUG, "update net header and sync header length fields");
		frame_base = ezframe_load_buf(&frame, frame_data, &frame_length, 0);
		net_hdr_info = (struct net_hdr *)(frame_base + sizeof(struct ether_header));
		sync_hdr = (struct alvs_state_sync_header *)((uint8_t *)net_hdr_info + sizeof(struct net_hdr));
		/*update net header length*/
		alvs_state_sync_update_net_hdr_len(net_hdr_info, cmem_alvs.conn_sync_state.conn_count);
		/*update sync header length*/
		alvs_state_sync_update_sync_hdr_len(sync_hdr, cmem_alvs.conn_sync_state.conn_count);
		ezframe_store_buf(&frame, frame_base, frame_length, 0);
	}

	alvs_write_log(LOG_DEBUG, "send aggregated sync frame (conn_count = %d)", cmem_alvs.conn_sync_state.conn_count);
	/*TODO should be replaced with a call to nw multicast module*/
	nw_send_frame_to_network(&frame, frame_data, USER_BASE_LOGICAL_ID);
}

/******************************************************************************
 * \brief       aggregate current connection within the sync frame.
 *              if the connection is bound, perform server info lookup.
 *              create a new sync frame if there is no existing frame, or if
 *              current buffer is full and number of buffers reached the limit.
 *              add new buffer if there is not enough space in current buffer
 *              and limit not reached.
 *              use provided source ip and sync id.
 *
 * \return      void
 *
 */
static __always_inline
void alvs_state_sync_aggr(in_addr_t source_ip, uint8_t sync_id)
{
	/*lookup server info for current connection*/
	if (likely(cmem_alvs.conn_info_result.bound == true)) {
		/*get destination server info. lookup is needed when reaching here from aging*/
		if (unlikely(alvs_server_info_lookup(cmem_alvs.conn_info_result.server_index) != 0)) {
			alvs_write_log(LOG_ERR, "sync master server info lookup failed ");
			goto exit;
		}
	}

	/*check if there is an existing sync frame*/
	if (unlikely(cmem_alvs.conn_sync_state.amount_buffers == 0)) {
		/*no existing sync frame, create a new one with current connection*/
		goto new_frame;
	}

	/*check if there is not enough room in current buffer*/
	if (unlikely((cmem_alvs.conn_sync_state.current_len + sizeof(struct alvs_state_sync_conn)) > EZFRAME_BUF_DATA_SIZE)) {
		if (unlikely(cmem_alvs.conn_sync_state.amount_buffers == ALVS_STATE_SYNC_BUFFERS_LIMIT)) {
			alvs_write_log(LOG_DEBUG, "sync frame full. sending and create a new one");
			/*send the full sync frame*/
			alvs_state_sync_send_aggr();
			goto new_frame;
		}
		/*current sync frame is not full, add connection to a new buffer*/
		alvs_write_log(LOG_DEBUG, "adding another buffer to current sync frame");
		if (unlikely(alvs_state_sync_add_buffer() != 0)) {
			goto exit;
		}
	}

	/*add connection*/
	alvs_write_log(LOG_DEBUG, "aggregate current connection for state sync");
	alvs_state_sync_set_sync_conn((struct alvs_state_sync_conn *)cmem_alvs.conn_sync_state.current_base);
	cmem_alvs.conn_sync_state.current_base += sizeof(struct alvs_state_sync_conn);
	cmem_alvs.conn_sync_state.current_len += sizeof(struct alvs_state_sync_conn);
	cmem_alvs.conn_sync_state.conn_count++;
	goto exit;

new_frame:
	/*create new sync frame*/
	alvs_write_log(LOG_DEBUG, "create a new aggregated sync frame");
	alvs_state_sync_first(source_ip, sync_id);
	/*add headroom to current length for proper filling of the first buffer*/
	cmem_alvs.conn_sync_state.current_len += ALVS_STATE_SYNC_HEADROOM;
exit:
	return;
}


#endif /* ALVS_STATE_SYNC_MASTER_H_ */
