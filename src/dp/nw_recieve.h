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
				int32_t logical_id)
{
	uint8_t	*frame_base;
	struct iphdr *ip_ptr;

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

		/*check if my_mac is set*/
		if (unlikely(!(cmem_wa.nw_wa.ezdp_decode_result.mac_decode_result.control.my_mac | cmem_wa.nw_wa.ezdp_decode_result.mac_decode_result.control.ipv4_multicast))) {
			anl_write_log(LOG_DEBUG, "Not my MAC or not multicast");
			nw_interface_inc_counter(NW_IF_STATS_NOT_MY_MAC);
			nw_direct_route(frame, frame_base, cmem_nw.ingress_if_result.direct_output_if, cmem_nw.ingress_if_result.is_direct_output_lag);
			return;
		}

		if (!cmem_wa.nw_wa.ezdp_decode_result.mac_decode_result.last_tag_protocol_type.ipv4) {
			anl_write_log(LOG_DEBUG, "Not IPv4!");
			nw_interface_inc_counter(NW_IF_STATS_NOT_IPV4);
			nw_direct_route(frame, frame_base, cmem_nw.ingress_if_result.direct_output_if, cmem_nw.ingress_if_result.is_direct_output_lag);
			return;
		}

		/*copy decode results to metadata*/
		ezdp_mem_copy(&packet_meta_data.mac_control, &cmem_wa.nw_wa.ezdp_decode_result.mac_decode_result.control, sizeof(struct ezdp_decode_mac_control));
		ezdp_mem_copy(&packet_meta_data.last_tag_protocol_type, &cmem_wa.nw_wa.ezdp_decode_result.mac_decode_result.last_tag_protocol_type, sizeof(struct ezdp_decode_mac_protocol_type));
		packet_meta_data.number_of_tags = cmem_wa.nw_wa.ezdp_decode_result.mac_decode_result.number_of_tags;
		packet_meta_data.ip_offset = cmem_wa.nw_wa.ezdp_decode_result.mac_decode_result.layer2_size;

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

		/*frame is OK, let's start alvs IF_STATS processing*/
		alvs_packet_processing(frame, frame_base);

	} else if (cmem_nw.ingress_if_result.path_type == DP_PATH_FROM_HOST_PATH || cmem_nw.ingress_if_result.path_type == DP_PATH_FROM_REMOTE_PATH) {

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
