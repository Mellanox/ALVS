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

#include "nw_host.h"
#include "nw_interface.h"

/******************************************************************************
 * \brief         send frames to network ports
 * \return        void
 */
static __always_inline
void nw_send_frame_to_network_interface(uint32_t output_channel_id)
{
	ezframe_send_to_if(&cmem.frame, output_channel_id, 0);
}


/******************************************************************************
 * \brief         perform arp lookup and modify l2 header before transmission
 * \return        void
 */
static __always_inline
void nw_arp_processing(uint8_t *frame_base, in_addr_t dest_ip)
{
	 uint32_t rc;
	 uint32_t found_result_size;
	 struct nw_arp_result *arp_res_ptr;

	 printf("do arp lookup!\n");

	 cmem.arp_key.real_server_address = dest_ip;

	 rc = ezdp_lookup_hash_entry(&shared_cmem.arp_struct_desc,
				     (void *)&cmem.arp_key,
				     sizeof(struct nw_arp_key),
				     (void **)&arp_res_ptr, &found_result_size,
				     0, cmem.arp_hash_wa,
				     sizeof(cmem.arp_hash_wa));

	if (rc == 0) {
		struct ether_addr *dmac = (struct ether_addr *)frame_base;

		/*perform lookup on output logical ID (calculation of output logical ID is for lag)*/
		if (nw_interface_lookup(arp_res_ptr->base_logical_id + (cmem.mac_decode_result.da_sa_hash & LAG_HASH_MASK)) != 0 )
		{
			printf("lookup fail on output logical ID = %d!\n", arp_res_ptr->base_logical_id + (cmem.mac_decode_result.da_sa_hash & LAG_HASH_MASK));
			return;
		}

		/*copy dst mac*/
		ezdp_mem_copy(dmac, arp_res_ptr->dest_mac_addr.ether_addr_octet, sizeof(struct ether_addr));
		printf ("dst mac = %02x:%02x:%02x:%02x:%02x:%02x \n",arp_res_ptr->dest_mac_addr.ether_addr_octet[0],
								arp_res_ptr->dest_mac_addr.ether_addr_octet[1],
								arp_res_ptr->dest_mac_addr.ether_addr_octet[2],
								arp_res_ptr->dest_mac_addr.ether_addr_octet[3],
								arp_res_ptr->dest_mac_addr.ether_addr_octet[4],
								arp_res_ptr->dest_mac_addr.ether_addr_octet[5]);
		/*copy src mac*/
		ezdp_mem_copy((uint8_t*)dmac+sizeof(struct ether_addr), cmem.interface_result.mac_address.ether_addr_octet, sizeof(struct ether_addr));
		printf ("src mac = %02x:%02x:%02x:%02x:%02x:%02x \n",cmem.interface_result.mac_address.ether_addr_octet[0],
				cmem.interface_result.mac_address.ether_addr_octet[1],
				cmem.interface_result.mac_address.ether_addr_octet[2],
				cmem.interface_result.mac_address.ether_addr_octet[3],
				cmem.interface_result.mac_address.ether_addr_octet[4],
				cmem.interface_result.mac_address.ether_addr_octet[5]);

		/* Store modified segment data */
		ezframe_store_buf(&cmem.frame,
							   frame_base,
							   ezframe_get_buf_len(&cmem.frame),
							   0);

		nw_send_frame_to_network_interface(cmem.interface_result.output_channel);
	} else {
		printf("fail arp lookup!\n");
		nw_interface_update_statistic_counter(cmem.frame.job_desc.frame_desc.logical_id, ALVS_PACKET_FAIL_ARP);
		nw_send_frame_to_host();
		return;
	}
}


/******************************************************************************
 * \brief         peform nw route
 * \return        void
 */
static __always_inline
void nw_do_route(uint8_t *frame_base, in_addr_t dest_ip)
{
	nw_arp_processing(frame_base, dest_ip);
}

#endif /* NW_ROUTING_H_ */
