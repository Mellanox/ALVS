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
*  File:                nw_recieve.h
*  Desc:                network infrastructure file containing validation and parsing of incoming traffic.
*
*/

#ifndef NW_RECIEVE_H_
#define NW_RECIEVE_H_

#include "anl_log.h"
#include "nw_routing.h"
#ifdef CONFIG_ALVS
#include "alvs_packet_processing.h"
#endif
#ifdef CONFIG_TC
#include "tc_packet_processing.h"
#endif
/******************************************************************************
 * \brief         Parse and validate frame
 * \return        void
 */

#if 0
void build_packet_meta_data(uint8_t	*frame_base)
{
	struct iphdr *ip_ptr = NULL;

	ezdp_mem_set(&cmem_wa.nw_wa.ezdp_decode_result, 0, sizeof(struct ezdp_decode_result));
	/* decode mac to ensure it is valid */
	ezdp_decode_mac(frame_base, MAX_DECODE_SIZE,
			&cmem_wa.nw_wa.ezdp_decode_result.mac_decode_result);

	/*in case of any error send frame to host*/
	if (unlikely(cmem_wa.nw_wa.ezdp_decode_result.mac_decode_result.error_codes.decode_error)) {
		anl_write_log(LOG_DEBUG, "Decode MAC failed!");
		nw_interface_inc_counter(NW_IF_STATS_MAC_ERROR);
		nw_direct_route(frame, frame_base, cmem_nw.ingress_if_result.direct_output_if, cmem_nw.ingress_if_result.is_direct_output_lag);
		return;
	}

	/*copy decode results to metadata*/
	ezdp_mem_copy(&packet_meta_data.mac_control, &cmem_wa.nw_wa.ezdp_decode_result.mac_decode_result.control, sizeof(struct ezdp_decode_mac_control));
	ezdp_mem_copy(&packet_meta_data.last_tag_protocol_type, &cmem_wa.nw_wa.ezdp_decode_result.mac_decode_result.last_tag_protocol_type, sizeof(struct ezdp_decode_mac_protocol_type));
	packet_meta_data.number_of_tags = cmem_wa.nw_wa.ezdp_decode_result.mac_decode_result.number_of_tags;
	packet_meta_data.ip_offset = cmem_wa.nw_wa.ezdp_decode_result.mac_decode_result.layer2_size;

	if (cmem_wa.nw_wa.ezdp_decode_result.mac_decode_result.last_tag_protocol_type.ipv4) {

		ip_ptr = (struct iphdr *)(frame_base + packet_meta_data.ip_offset);

		/*skip L2 and validate IP is ok*/
		ezdp_decode_ipv4((uint8_t *)ip_ptr, sizeof(struct iphdr),
				 frame->job_desc.frame_desc.frame_length,
				 &cmem_wa.nw_wa.ezdp_decode_result.ipv4_decode_result);

		/*in case of any error send frame to host*/
		if (unlikely(cmem_wa.nw_wa.ezdp_decode_result.ipv4_decode_result.error_codes.decode_error)) {
			anl_write_log(LOG_DEBUG, "IPv4 decode failed");
			nw_interface_inc_counter(NW_IF_STATS_IPV4_ERROR);
			nw_direct_route(frame, frame_base, cmem_nw.ingress_if_result.direct_output_if, cmem_nw.ingress_if_result.is_direct_output_lag);
			return;
		}

		/* copy decode result to metadata */
		ezdp_mem_copy(&packet_meta_data.ip_next_protocol,
			      &cmem_wa.nw_wa.ezdp_decode_result.ipv4_decode_result.next_protocol,
			      sizeof(struct ezdp_decode_ip_next_protocol));
	} else {
		packet_meta_data.ip_next_protocol.raw_data = 0;
	}

}
#endif
static __always_inline
void nw_recieve_and_parse_frame(ezframe_t __cmem * frame,
				uint8_t __cmem * frame_data,
				int32_t logical_id)
{
	uint8_t	*frame_base;
	struct iphdr *ip_ptr = NULL;

	if (unlikely(nw_if_ingress_lookup(logical_id) != 0)) {
		anl_write_log(LOG_DEBUG, "fail ingress interface lookup - logical id =%d!", logical_id);
		nw_discard_frame();
		return;
	}
	/* check nw interface admin state */

	if (unlikely(cmem_nw.ingress_if_result.admin_state == false)) {
		/* drop frame!! */
		anl_write_log(LOG_DEBUG, "Interface admin state is disabled, dropping packet on ingress");
		nw_interface_inc_counter(NW_IF_STATS_DISABLE_IF_INGRESS_DROPS);
		nw_discard_frame();
		return;
	}

	/* === Load Data of first frame buffer === */
	frame_base = ezframe_load_buf(frame, frame_data, 0, 0);

	/* === Check validity of received frame === */
	if (unlikely(ezframe_valid(frame) != 0)) {
		anl_write_log(LOG_DEBUG, "frame is not valid (%s)", ezdp_get_err_msg());
		nw_interface_inc_counter(NW_IF_STATS_FRAME_VALIDATION_FAIL);
		nw_direct_route(frame, frame_base, cmem_nw.ingress_if_result.direct_output_if, cmem_nw.ingress_if_result.is_direct_output_lag);
		return;
	}

	if (cmem_nw.ingress_if_result.path_type == DP_PATH_FROM_NW_PATH) {
		/* decode mac to ensure it is valid */
		ezdp_decode_mac(frame_base, MAX_DECODE_SIZE,
				&cmem_wa.nw_wa.ezdp_decode_result.mac_decode_result);
		/*in case of any error send frame to host*/
		if (unlikely(cmem_wa.nw_wa.ezdp_decode_result.mac_decode_result.error_codes.decode_error)) {
			anl_write_log(LOG_DEBUG, "Decode MAC failed!");
			nw_interface_inc_counter(NW_IF_STATS_MAC_ERROR);
			nw_direct_route(frame, frame_base, cmem_nw.ingress_if_result.direct_output_if, cmem_nw.ingress_if_result.is_direct_output_lag);
			return;
		}
		/*copy decode results to metadata*/
		ezdp_mem_copy(&packet_meta_data.mac_control, &cmem_wa.nw_wa.ezdp_decode_result.mac_decode_result.control, sizeof(struct ezdp_decode_mac_control));
		ezdp_mem_copy(&packet_meta_data.last_tag_protocol_type, &cmem_wa.nw_wa.ezdp_decode_result.mac_decode_result.last_tag_protocol_type, sizeof(struct ezdp_decode_mac_protocol_type));
		packet_meta_data.number_of_tags = cmem_wa.nw_wa.ezdp_decode_result.mac_decode_result.number_of_tags;
		packet_meta_data.ip_offset = cmem_wa.nw_wa.ezdp_decode_result.mac_decode_result.layer2_size;

		if (cmem_wa.nw_wa.ezdp_decode_result.mac_decode_result.last_tag_protocol_type.ipv4) {
			ip_ptr = (struct iphdr *)(frame_base + packet_meta_data.ip_offset);
			/*skip L2 and validate IP is ok*/

			ezdp_decode_ipv4((uint8_t *)ip_ptr, sizeof(struct iphdr),
					 frame->job_desc.frame_desc.frame_length,
					 &cmem_wa.nw_wa.ezdp_decode_result.ipv4_decode_result);

			/*in case of any error send frame to host */
			if (unlikely(cmem_wa.nw_wa.ezdp_decode_result.ipv4_decode_result.error_codes.decode_error)) {
				anl_write_log(LOG_DEBUG, "IPv4 decode failed");
				nw_interface_inc_counter(NW_IF_STATS_IPV4_ERROR);
				nw_direct_route(frame, frame_base, cmem_nw.ingress_if_result.direct_output_if, cmem_nw.ingress_if_result.is_direct_output_lag);
				return;
			}

			/* copy decode result to metadata */
			ezdp_mem_copy(&packet_meta_data.ip_next_protocol,
				      &cmem_wa.nw_wa.ezdp_decode_result.ipv4_decode_result.next_protocol,
				      sizeof(struct ezdp_decode_ip_next_protocol));

		} else {
			packet_meta_data.ip_next_protocol.raw_data = 0;
		}

#ifdef CONFIG_TC
		/*frame is OK, let's start TC processing*/
		if (cmem_nw.ingress_if_result.app_bitmap.tc_en == true) {
			if (tc_packet_processing(frame, frame_base, ip_ptr, logical_id) == TC_PROCESSING_RC_PACKET_STOLEN) {
				anl_write_log(LOG_DEBUG, "incoming packet was stolen by TC - end NW processing.");
				return;
			}

			anl_write_log(LOG_DEBUG, "incoming packet was accepted by TC - end NW processing.");
		}

#endif

		packet_meta_data.mac_control.my_mac = ezdp_mem_cmp((uint8_t *)&cmem_nw.ingress_if_result.mac_address, frame_base, ETH_ALEN) == 0 ? 1 : 0;

		anl_write_log(LOG_DEBUG, "My MAC %02x:%02x:%02x:%02x:%02x:%02x",
			      ((struct ether_addr)cmem_nw.ingress_if_result.mac_address).ether_addr_octet[0],
			      ((struct ether_addr)cmem_nw.ingress_if_result.mac_address).ether_addr_octet[1],
			      ((struct ether_addr)cmem_nw.ingress_if_result.mac_address).ether_addr_octet[2],
			      ((struct ether_addr)cmem_nw.ingress_if_result.mac_address).ether_addr_octet[3],
			      ((struct ether_addr)cmem_nw.ingress_if_result.mac_address).ether_addr_octet[4],
			      ((struct ether_addr)cmem_nw.ingress_if_result.mac_address).ether_addr_octet[5]);
		/*check if my_mac is set*/
		if (unlikely(!(packet_meta_data.mac_control.my_mac | packet_meta_data.mac_control.ipv4_multicast))) {
			anl_write_log(LOG_DEBUG, "Not my MAC or not multicast");
			nw_interface_inc_counter(NW_IF_STATS_NOT_MY_MAC);
			nw_direct_route(frame, frame_base, cmem_nw.ingress_if_result.direct_output_if, cmem_nw.ingress_if_result.is_direct_output_lag);
			return;
		}

		if (!packet_meta_data.last_tag_protocol_type.ipv4) {
			anl_write_log(LOG_DEBUG, "Not IPv4!");
			nw_interface_inc_counter(NW_IF_STATS_NOT_IPV4);
			nw_direct_route(frame, frame_base, cmem_nw.ingress_if_result.direct_output_if, cmem_nw.ingress_if_result.is_direct_output_lag);
			return;
		}

		/*frame is OK */
#ifdef CONFIG_ALVS
		/*let's start alvs IF_STATS processing*/
		if (cmem_nw.ingress_if_result.app_bitmap.alvs_en == true) {
			alvs_packet_processing(frame, frame_base);
			return;
		}
#endif


		anl_write_log(LOG_DEBUG, "Perform standard routing");

		if (cmem_nw.ingress_if_result.app_bitmap.routing_en == true) {
			anl_write_log(LOG_DEBUG, "Perform standard routing - routing enabled.");
			int32_t interface;
			bool is_local_ip = false;
			/* Check if destination IP is local IP. Need to check all NW IF local IPs */
			for (interface = NW_BASE_LOGICAL_ID; interface < NW_BASE_LOGICAL_ID + NW_IF_NUM; interface++) {
				if (unlikely(nw_if_ingress_address_lookup(interface) != 0)) {
					anl_write_log(LOG_DEBUG, "Fail ingress interface address lookup - logical id =%d!", interface);
					nw_discard_frame();
					return;
				}
				if (ip_ptr->daddr == cmem_nw.ingress_if_addresses_result.local_ip) {
					is_local_ip = true;
					anl_write_log(LOG_DEBUG, "IP packet %3d.%3d.%3d.%3d equals to my IP",
						      *(uint8_t *)&(ip_ptr->daddr),
						      *((uint8_t *)&(ip_ptr->daddr)+1),
						      *((uint8_t *)&(ip_ptr->daddr)+2),
						      *((uint8_t *)&(ip_ptr->daddr)+3));
					break;
				}
			}
			/* If destination is not local IP, perform routing function */
			if (is_local_ip == false) {
				if (nw_do_route(frame, frame_base, ip_ptr->daddr, ezframe_get_buf_len(frame)) == true) {
					return;
				}
			}
		}
		/* default behavior or in case of nw_do_route fails, we need to send packet to host */
		nw_direct_route(frame, frame_base, cmem_nw.ingress_if_result.direct_output_if, cmem_nw.ingress_if_result.is_direct_output_lag);
	} else if (cmem_nw.ingress_if_result.path_type == DP_PATH_FROM_HOST_PATH) {

		/*check src mac of incoming packet against all nw interfaces*/
		uint8_t out_logical_id;
		uint8_t *src_mac = frame_base + sizeof(struct ether_addr);

		anl_write_log(LOG_DEBUG, "match to outgoing src mac %02x:%02x:%02x:%02x:%02x:%02x",
			      ((struct ether_addr *)src_mac)->ether_addr_octet[0],
			      ((struct ether_addr *)src_mac)->ether_addr_octet[1],
			      ((struct ether_addr *)src_mac)->ether_addr_octet[2],
			      ((struct ether_addr *)src_mac)->ether_addr_octet[3],
			      ((struct ether_addr *)src_mac)->ether_addr_octet[4],
			      ((struct ether_addr *)src_mac)->ether_addr_octet[5]);

		for (out_logical_id = NW_BASE_LOGICAL_ID; out_logical_id < NW_IF_NUM; out_logical_id++) {
			if (unlikely(nw_if_ingress_lookup(out_logical_id) != 0)) {
				anl_write_log(LOG_DEBUG, "fail ingress interface lookup - logical id =%d!", out_logical_id);
				nw_discard_frame();
				return;
			}

			if (!ezdp_mem_cmp((uint8_t *)(&cmem_nw.ingress_if_result.mac_address), src_mac, sizeof(struct ether_addr))) {
				anl_write_log(LOG_DEBUG, "found match - send to IF %d", out_logical_id);
				nw_direct_route(frame, frame_base, out_logical_id, false);
				return;
			}
		}

		anl_write_log(LOG_DEBUG, "no match to outgoing src mac - send to default IF 0");

		/*currently send frame to network without any change or any other operations*/
		nw_direct_route(frame, frame_base, 0, false);
	} else if (cmem_nw.ingress_if_result.path_type == DP_PATH_FROM_REMOTE_PATH) {
		/*currently send frame to network without any change or any other operations*/
		nw_direct_route(frame, frame_base, cmem_nw.ingress_if_result.direct_output_if, cmem_nw.ingress_if_result.is_direct_output_lag);

	} else {
		anl_write_log(LOG_DEBUG, "Error! no match in interface lookup!");
		/*currently send frame to host without any change or any other operations*/
		nw_interface_inc_counter(NW_IF_STATS_NO_VALID_ROUTE);
		nw_direct_route(frame, frame_base, cmem_nw.ingress_if_result.direct_output_if, cmem_nw.ingress_if_result.is_direct_output_lag);
	}
}

#endif /* NW_RECIEVE_H_ */
