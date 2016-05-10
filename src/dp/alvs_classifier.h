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
*  File:                alvs_classifier.h
*  Desc:                classification functionality for ALVS
*/

#ifndef ALVS_CLASSIFIER_H_
#define ALVS_CLASSIFIER_H_

#include "nw_routing.h"

/******************************************************************************
 * \brief         do service classification - 3 tuple - DIP, dest port and IP protocol
 *
 * \return        void
 */
void alvs_service_classification(uint8_t *frame_base, struct iphdr *ip_hdr);
void alvs_service_classification(uint8_t *frame_base, struct iphdr *ip_hdr)
{
	 uint32_t rc;
	 uint32_t found_result_size;
	 struct  alvs_service_result *service_res_ptr;
	 struct tcphdr *tcp_hdr = (struct tcphdr *)((uint8_t *)ip_hdr + sizeof(struct iphdr));

	 cmem.service_key.service_address = ip_hdr->daddr;
	 cmem.service_key.service_protocol = ip_hdr->protocol;
	 cmem.service_key.service_port = tcp_hdr->dest;
	 printf("start service classification!\n");



	 rc = ezdp_lookup_hash_entry(&shared_cmem.services_struct_desc,
				     (void *)&cmem.service_key,
				     sizeof(struct alvs_service_key),
				     (void **)&service_res_ptr,
				     &found_result_size, 0,
				     cmem.service_hash_wa,
				     sizeof(cmem.service_hash_wa));

	if (rc == 0) {
		printf("pass service classification!\n");
		nw_do_route(frame_base, service_res_ptr->real_server_ip);
	} else {
		printf("fail service classification!\n");
		nw_interface_update_statistic_counter(cmem.frame.job_desc.frame_desc.logical_id, ALVS_PACKET_FAIL_CLASSIFICATION);
		nw_send_frame_to_host();
		return;
	}
}

#endif /* ALVS_CLASSIFIER_H_ */
