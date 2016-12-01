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
*  File:                nw_routing.h
*  Desc:                network infrastructure file containing routing functionality
*/

#ifndef NW_ROUTING_H_
#define NW_ROUTING_H_

#include "nw_utils.h"

/******************************************************************************
 * \brief         send frame directly to the interface given by direct_if
 * \return        void
 */
static __always_inline
void nw_direct_route(ezframe_t __cmem * frame, uint8_t __cmem * frame_base, uint8_t out_if, bool is_lag)
{
	if (unlikely(nw_calc_egress_if(frame_base, out_if, is_lag) == false)) {
		nw_interface_inc_counter(NW_IF_STATS_FAIL_INTERFACE_LOOKUP);
		nw_discard_frame();
		return;
	}
	ezframe_send_to_if(frame, cmem_nw.egress_if_result.output_channel, 0);
}

/******************************************************************************
 * \brief       perform FIB lookup and get dest_ip for transmission
 *
 * \param[in]   dest_ip	         - destination IP
 * \param[out]  route_entry      - reference to a route entry result
 *
 * \return      bool             - true  - successful FIB processing
 *                                 false - fail in FIB processing, lookup fail/Unknown FIB result type
 */
static __always_inline
bool nw_fib_processing(in_addr_t dest_ip, struct route_entry_result *route_entry)
{
	in_addr_t res_dest_ip;
	struct ezdp_lookup_int_tcam_retval tcam_retval;

	/* read iTCAM */
	cmem_nw.fib_key.dest_ip = dest_ip;
	tcam_retval.raw_data = ezdp_lookup_int_tcam(NW_FIB_TCAM_SIDE,
						   NW_FIB_TCAM_PROFILE,
						   &cmem_nw.fib_key,
						   sizeof(struct nw_fib_key),
						   &cmem_wa.nw_wa.int_tcam_result);

	/* check matching */
	if (unlikely(tcam_retval.assoc_data.match == 0)) {
		anl_write_log(LOG_ERR, "FIB lookup failed. key dest_ip = 0x%08x", dest_ip);
		nw_interface_inc_counter(NW_IF_STATS_FAIL_FIB_LOOKUP);
		return false;
	}

	route_entry->result_type = cmem_wa.nw_wa.fib_result.result_type;
	route_entry->is_lag = cmem_wa.nw_wa.fib_result.is_lag;
	route_entry->output_index = cmem_wa.nw_wa.fib_result.out_index;
	res_dest_ip = cmem_wa.nw_wa.fib_result.dest_ip;

	/* get dest_ip */
	if (likely(route_entry->result_type == NW_FIB_NEIGHBOR)) {
		/* Destination IP is neighbor. use it for ARP */
		anl_write_log(LOG_DEBUG, "NW_FIB_NEIGHBOR: using origin dest IP 0x%08x", dest_ip);
		route_entry->dest_ip = dest_ip;
		return true;
	} else if (route_entry->result_type == NW_FIB_GW) {
		/* Destination IP is GW. use result IP */
		anl_write_log(LOG_DEBUG, "NW_FIB_GW: using result IP 0x%08x", res_dest_ip);
		route_entry->dest_ip = res_dest_ip;
		return true;
	} else if (route_entry->result_type == NW_FIB_DROP) {
		anl_write_log(LOG_DEBUG, "NW_FIB_DROP: Drop frame.");
		nw_interface_inc_counter(NW_IF_STATS_REJECT_BY_FIB);
		return true;
	} else if (route_entry->result_type == NW_FIB_UNSUPPORTED) {
		anl_write_log(LOG_DEBUG, "NW_FIB_UNSUPPORTED: application will handle the frame.");
		return true;
	}

	/* Unknown result type.*/
	anl_write_log(LOG_ERR, "Unknown FIB result type.");
	nw_interface_inc_counter(NW_IF_STATS_UNKNOWN_FIB_RESULT);
	return false;
}

/******************************************************************************
 * \brief         perform arp lookup and modify l2 header before transmission
 *
 * \return        NW_ARP_OK            - ARP processing finished successfully
 *                NW_ARP_CRITICAL_ERR  - ARP processing had critical error
 *                NW_ARP_LOOKUP_FAIL   - ARP processing had ARP lookup fail
 */
static __always_inline
enum nw_arp_processing_result nw_arp_processing(ezframe_t __cmem * frame,
		       uint8_t __cmem * frame_base,
		       struct route_entry_result *route_entry,
		       uint32_t	frame_buff_size)
{
	uint32_t rc;
	uint32_t found_result_size;
	struct nw_arp_result *arp_res_ptr;

	cmem_nw.arp_key.ip = route_entry->dest_ip;
	cmem_nw.arp_key.is_lag = route_entry->is_lag;
	cmem_nw.arp_key.out_index = route_entry->output_index;

	rc = ezdp_lookup_hash_entry(&shared_cmem_nw.arp_struct_desc,
				    (void *)&cmem_nw.arp_key,
				    sizeof(struct nw_arp_key),
				    (void **)&arp_res_ptr, &found_result_size,
				    0, cmem_wa.nw_wa.arp_hash_wa,
				    sizeof(cmem_wa.nw_wa.arp_hash_wa));

	if (likely(rc == 0)) {
		struct ether_addr *dmac = (struct ether_addr *)frame_base;

		/*copy dst mac*/
		ezdp_mem_copy(dmac, arp_res_ptr->dest_mac_addr.ether_addr_octet, sizeof(struct ether_addr));

		/*Calc egress if*/
		if (unlikely(nw_calc_egress_if(frame_base, route_entry->output_index, route_entry->is_lag) == false)) {
			anl_write_log(LOG_DEBUG, "Interface admin state is disabled, dropping packet on ingress");
			nw_interface_inc_counter(NW_IF_STATS_FAIL_INTERFACE_LOOKUP);
			return NW_ARP_CRITICAL_ERR;
		}

		/*copy src mac*/
		ezdp_mem_copy((uint8_t *)dmac+sizeof(struct ether_addr), cmem_nw.egress_if_result.mac_address.ether_addr_octet, sizeof(struct ether_addr));

		/* Store modified segment data */
		rc = ezframe_store_buf(frame,
				       frame_base,
				       frame_buff_size,
				       0);
		if (rc != 0) {
			anl_write_log(LOG_DEBUG, "Ezframe store buf was failed");
			nw_interface_inc_counter(NW_IF_STATS_FAIL_STORE_BUF);
			return NW_ARP_CRITICAL_ERR;
		}

		ezframe_send_to_if(frame, cmem_nw.egress_if_result.output_channel, 0);
		return NW_ARP_OK;

	}
	anl_write_log(LOG_DEBUG, "dest_ip = 0x%x ARP lookup FAILED", route_entry->dest_ip);
	nw_interface_inc_counter(NW_IF_STATS_FAIL_ARP_LOOKUP);
	return NW_ARP_LOOKUP_FAIL;

}

/******************************************************************************
 * \brief         perform nw route
 * \return        bool, true:  frame was handled either by send_to_if or discard
 *                      false: the application will handle this frame.
 */
static __always_inline
bool nw_do_route(ezframe_t __cmem * frame, uint8_t *frame_base,
		in_addr_t dest_ip, uint32_t frame_buff_size)
{
	struct route_entry_result route_entry;
	enum nw_arp_processing_result arp_rc;

	if (unlikely(nw_fib_processing(dest_ip, &route_entry) == false || route_entry.result_type == NW_FIB_UNSUPPORTED)) {
		/* failed FIB processing/Unsupported route type, application will handle */
		return false;
	}

	if (unlikely(route_entry.result_type == NW_FIB_DROP)) {
		nw_discard_frame();
		return true;
	}

	arp_rc = nw_arp_processing(frame, frame_base, &route_entry, frame_buff_size);
	if (likely(arp_rc == NW_ARP_OK)) {
		return true;
	} else if (arp_rc == NW_ARP_CRITICAL_ERR) {
		nw_discard_frame();
		return true;
	} else if (arp_rc == NW_ARP_LOOKUP_FAIL) {
		return false;
	}

	/* we should not get here*/
	anl_write_log(LOG_ERR, "Unsupported nw_arp_processing_result: %d", arp_rc);
	return false;
}

/******************************************************************************
 * \brief         calculate my mac, update & send frame directly to the local host interface
 * \return        void
 */
static __always_inline
void nw_local_host_route(ezframe_t __cmem * frame, uint8_t __cmem * frame_base, uint32_t frame_buff_size, uint8_t host_logical_id)
{
	uint8_t *my_mac;
	struct ether_header *eth_p;

	eth_p = (struct ether_header *)(frame_base - sizeof(struct ether_header));

	/*fill ethernet destination MAC*/
	my_mac = nw_interface_get_mac_address(host_logical_id);
	if (my_mac == NULL) {
		nw_interface_inc_counter(NW_IF_STATS_FAIL_GET_MAC_ADDR);
		nw_discard_frame();
		return;
	}
	ezdp_mem_copy((uint8_t *)eth_p, my_mac, sizeof(struct ether_addr));

	/*update frame length*/
	frame_buff_size += sizeof(struct ether_header);
	/*store buffer with updated length*/
	if (ezframe_store_buf(frame, eth_p, frame_buff_size, 0)) {
		nw_interface_inc_counter(NW_IF_STATS_FAIL_STORE_BUF);
		nw_discard_frame();
		return;
	}

	ezframe_send_to_if(frame, cmem_nw.egress_if_result.output_channel, 0);
}


#endif /* NW_ROUTING_H_ */
