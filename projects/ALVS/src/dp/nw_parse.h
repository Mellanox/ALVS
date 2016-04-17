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
*/

#ifndef NW_RECIEVE_H_
#define NW_RECIEVE_H_

#include "alvs_classifier.h"
#include "nw_interface.h"

/******************************************************************************
 * \brief	  Parse and validate frame
 * \return	  void
 */
static __always_inline
void nw_recieve_parse_frame(int32_t logical_id)
{
	uint8_t	*frame_base;

	enum dp_path_type path_type = nw_interface_get_dp_path(logical_id);

	if (path_type == DP_PATH_SEND_TO_HOST_APP)
	{
		uint8_t* next_et;

		/* === Load Data of first frame buffer === */
		frame_base = ezframe_load_buf(&cmem.frame, cmem.frame_data, NULL, 0);

		/* decode mac to ensure it is valid */
		ezdp_decode_mac(frame_base,
						MAX_DECODE_SIZE,
						&cmem.mac_decode_result);

		/*in case of any error send frame to host*/
		if (unlikely(cmem.mac_decode_result.error_codes.decode_error))
		{
			nw_interface_inc_statistic_counter(logical_id, ALVS_PACKET_MAC_ERROR, DP_NUM_COUNTERS_PER_INTERFACE, 1);
			nw_send_frame_to_host();
			return;
		}

		/*check if my_mac is set*/
		if (unlikely(!cmem.mac_decode_result.control.my_mac))
		{
			nw_interface_inc_statistic_counter(logical_id, ALVS_PACKET_NOT_MY_MAC, DP_NUM_COUNTERS_PER_INTERFACE, 1);
			nw_send_frame_to_host();
			return;
		}

		/*skip vlan tags and check next ethertype*/
		next_et = frame_base + sizeof(struct ether_addr) + sizeof(struct ether_addr); //skip l2 addr

		//check if vlan and then skip
		if (cmem.mac_decode_result.number_of_tags)
		{
			next_et += (4 << cmem.mac_decode_result.number_of_tags);
		}

		/*check if ipv4 frame*/
		if (*((uint16_t*)next_et) != ETHERTYPE_IP)
		{
			nw_interface_inc_statistic_counter(logical_id, ALVS_PACKET_NOT_IPV4, DP_NUM_COUNTERS_PER_INTERFACE, 1);
			nw_send_frame_to_host();
			return;
		}

		struct iphdr  *ip_ptr = (struct iphdr*)(next_et + sizeof(uint16_t));

		/*skip L2 and validate IP is ok*/
		ezdp_decode_ipv4((uint8_t*)ip_ptr,
						 sizeof(struct iphdr),
						 cmem.frame.job_desc.frame_desc.frame_length,
						 &cmem.ipv4_decode_result);

		/*in case of any error send frame to host*/
		if (unlikely(cmem.ipv4_decode_result.error_codes.decode_error))
		{
			nw_interface_inc_statistic_counter(logical_id, ALVS_PACKET_IPV4_ERROR, DP_NUM_COUNTERS_PER_INTERFACE, 1);
			nw_send_frame_to_host();
			return;
		}

		if (ip_ptr->protocol != IPPROTO_TCP && ip_ptr->protocol != IPPROTO_UDP)
		{
			nw_interface_inc_statistic_counter(logical_id, ALVS_PACKET_NOT_UDP_AND_TCP, DP_NUM_COUNTERS_PER_INTERFACE, 1);
			nw_send_frame_to_host();
			return;
		}

		//check if need to check validity of TCP/UDP


		/*frame is OK, let's start classification - TODO change from hard coded to process ID from interface */
		alvs_service_classification( frame_base, ip_ptr);
	}
	else if (path_type == DP_PATH_SEND_TO_NW_APP || path_type == DP_PATH_SEND_TO_NW_NA) /*APP NW PATH*/
	{
		/*currently send frame to network without any change or any other operations*/
		nw_send_frame_to_network_interface();
	}
	else if (path_type == DP_PATH_SEND_TO_HOST_NA) /*NA HOST PATH*/
	{
		/*currently send frame to host without any change or any other operations*/
		nw_interface_inc_statistic_counter(logical_id, ALVS_PACKET_NO_VALID_ROUTE, DP_NUM_COUNTERS_PER_INTERFACE, 1);
		nw_send_frame_to_host();
	}
	else
	{
		printf("Error! no match in interface lookup!!\n");
	}
}

#endif /* NPS_PARSE_H_ */
