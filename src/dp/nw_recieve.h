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

#include "alvs_packet_processing.h"

/******************************************************************************
 * \brief         Parse and validate frame
 * \return        void
 */
static __always_inline
void nw_recieve_and_parse_frame(ezframe_t __cmem * frame,
				uint8_t __cmem * frame_data,
				int32_t port_id)
{
	uint8_t	*frame_base;

	if (unlikely(nw_interface_lookup(port_id) != 0)) {
		alvs_write_log(LOG_DEBUG, "fail interface lookup - port id =%d!", port_id);
		nw_interface_inc_counter(NW_IF_STATS_FAIL_INTERFACE_LOOKUP);
		nw_host_do_route(frame);
		return;
	}

	/* === Check validity of received frame === */
	if (unlikely(ezframe_valid(frame) != 0)) {
		alvs_write_log(LOG_DEBUG, "frame is not valid");
		nw_interface_inc_counter(NW_IF_STATS_FRAME_VALIDATION_FAIL);
		nw_host_do_route(frame);
		return;
	}

	if (cmem_nw.interface_result.path_type == DP_PATH_FROM_NW_PATH) {

		/* === Load Data of first frame buffer === */
		frame_base = ezframe_load_buf(frame, frame_data,
					      NULL, 0);

		/* decode mac to ensure it is valid */
		ezdp_decode_mac(frame_base, MAX_DECODE_SIZE,
				&cmem_nw.mac_decode_result);

		/*in case of any error send frame to host*/
		if (unlikely(cmem_nw.mac_decode_result.error_codes.decode_error)) {
			alvs_write_log(LOG_DEBUG, "Decode MAC failed!");
			nw_interface_inc_counter(NW_IF_STATS_MAC_ERROR);
			nw_host_do_route(frame);
			return;
		}

		/*check if my_mac is set*/
		if (unlikely(!(cmem_nw.mac_decode_result.control.my_mac | cmem_nw.mac_decode_result.control.ipv4_multicast))) {
			alvs_write_log(LOG_DEBUG, "Not my MAC or not multicast");
			nw_interface_inc_counter(NW_IF_STATS_NOT_MY_MAC);
			nw_host_do_route(frame);
			return;
		}


		if (!cmem_nw.mac_decode_result.last_tag_protocol_type.ipv4) {
			alvs_write_log(LOG_DEBUG, "Not IPv4!");
			nw_interface_inc_counter(NW_IF_STATS_NOT_IPV4);
			nw_host_do_route(frame);
			return;
		}

		struct iphdr *ip_ptr = (struct iphdr *)(frame_base + cmem_nw.mac_decode_result.layer2_size);

		/*skip L2 and validate IP is ok*/
		ezdp_decode_ipv4((uint8_t *)ip_ptr, sizeof(struct iphdr),
				 frame->job_desc.frame_desc.frame_length,
				 &cmem_nw.ipv4_decode_result);

		/*in case of any error send frame to host*/
		if (unlikely(cmem_nw.ipv4_decode_result.error_codes.decode_error)) {
			alvs_write_log(LOG_DEBUG, "IPv4 decode failed");
			nw_interface_inc_counter(NW_IF_STATS_IPV4_ERROR);
			nw_host_do_route(frame);
			return;
		}

		/*frame is OK, let's start alvs IF_STATS processing*/
		alvs_packet_processing(frame, frame_base, ip_ptr, cmem_nw.mac_decode_result.control.my_mac);

	} else if (cmem_nw.interface_result.path_type == DP_PATH_FROM_HOST_PATH) {
		/*currently send frame to network without any change or any other operations*/

		/* === Load Data of first frame buffer === */
		frame_base = ezframe_load_buf(frame, frame_data,
					      NULL, 0);

		nw_send_frame_to_network(frame,
					 frame_base,
					 DEFAULT_NW_BASE_LOGICAL_ID);
	} else {
		alvs_write_log(LOG_DEBUG, "Error! no match in interface lookup!");
		/*currently send frame to host without any change or any other operations*/
		nw_interface_inc_counter(NW_IF_STATS_NO_VALID_ROUTE);
		nw_host_do_route(frame);
	}
}

#endif /* NW_RECIEVE_H_ */
