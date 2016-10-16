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
*  File:                nw_mc.h
*  Desc:                network infrastructure file containing multicast functionality
*/

#ifndef NW_MC_H_
#define NW_MC_H_

#include "nw_utils.h"

#define NW_MC_IP_MASK 0xE0000000

/******************************************************************************
 * \brief       perform network interface lookup and update ethernet header
 *              with source and destination mac addresses.
 *
 * \params[in]	mc_ip		- multicast IP
 *        [out]	dst_eth_addr	- pointer to MC MAC result (6 bytes)
 *
 * \return      true for success.
 *              false for failure (IP is not MC IP)
 */
static __always_inline
bool nw_mc_calc_mac(uint32_t  mc_ip,
			   uint8_t  *dst_eth_addr)
{
	assert((mc_ip & NW_MC_IP_MASK) != NW_MC_IP_MASK);

	dst_eth_addr[0] = 0x01;
	dst_eth_addr[1] = 0x00;
	dst_eth_addr[2] = 0x5e;
	dst_eth_addr[3] = (mc_ip & 0x7F0000) >> 16;
	dst_eth_addr[4] = (mc_ip & 0xFF00)   >> 8;
	dst_eth_addr[5] = (mc_ip & 0xFF);

	return true;
}

/******************************************************************************
 * \brief       perform network interface lookup and update ethernet header
 *              with source and destination mac addresses.
 *
 * \params[in]	eth_header - multicast IP
 *
 * \return      void
 *
 */
static __always_inline
void nw_mc_set_eth_hdr(struct ether_header *eth_header,
		       uint32_t             mc_ip)
{
	/* fill ethernet destination MAC */
	if (unlikely(nw_mc_calc_mac(mc_ip, (uint8_t *)eth_header->ether_dhost) == false)) {
		alvs_write_log(LOG_ERR, "nw_calc_mac_for_mc_ip failed");
		return;
	}

	/* lookup for network interface */
	if (unlikely(nw_interface_lookup(USER_BASE_LOGICAL_ID) != 0)) {
		alvs_write_log(LOG_DEBUG, "set ETH hearer fail nw interface lookup - port id =%d!",
			       USER_BASE_LOGICAL_ID);
		return;
	}

	/* fill ethernet source MAC */
	ezdp_mem_copy((uint8_t *)eth_header->ether_shost,
		      cmem_nw.interface_result.mac_address.ether_addr_octet,
		      sizeof(struct ether_addr));

	/* fill ethernet type */
	eth_header->ether_type = ETHERTYPE_IP;
}


/******************************************************************************
 * \brief         Handle frame of layer 3 and add MC layer 2 acording to MC IP
 *
 * \return        void
 */
static __always_inline
void nw_mc_handle(ezframe_t   __cmem * frame)
{

	struct net_hdr      *net_hdr;
	struct ether_header *eth_header;
	uint32_t             mc_ip;
	uint32_t             headroom;
	uint8_t             *frame_base;
	uint32_t             frame_length;
	uint32_t             rc;

	/* get first buffer */
	rc = ezframe_first_buf(frame, 0);
	if (rc != 0) {
		alvs_write_log(LOG_DEBUG, "ezframe_first_buf failed in nw_mc_handle");
		ezframe_free(frame, 0);
		return;
	}
	frame_base = ezframe_load_buf(frame, frame_data, &frame_length, 0);

	/* get mc_ip from net header */
	net_hdr    = (struct net_hdr *)frame_base;
	mc_ip      = net_hdr->ipv4.saddr;

	/* update frame base, length, eth_header */
	eth_header    = (struct ether_header *)frame_base;
	eth_header--; /* go back sizeof ether_header structure */
	frame_base    = (uint8_t *)eth_header;
	frame_length += sizeof(struct ether_header);

	/* update frame len & offset */
	assert(ezframe_get_buf_headroom(frame) >= sizeof(struct ether_header));
	headroom  = ezframe_get_buf_headroom(frame);
	headroom -= sizeof(struct ether_header);
	ezframe_set_buf_headroom(frame, headroom);

	/* set ethernet header */
	nw_mc_set_eth_hdr(eth_header, mc_ip);

	/* store bufer with ether header */
	rc = ezframe_store_buf(frame, frame_base, frame_length, 0);
	if (rc != 0) {
		alvs_write_log(LOG_DEBUG, "Ezframe store buf was failed");
		nw_interface_inc_counter(NW_IF_STATS_FAIL_STORE_BUF);
		nw_discard_frame();
		return;
	}

	/* send to network */
	nw_send_frame_to_network(frame, (uint8_t *)eth_header, USER_BASE_LOGICAL_ID);
}


#endif /* NW_MC_H_ */
