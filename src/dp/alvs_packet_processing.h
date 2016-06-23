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
*  File:                alvs_packet_processing.h
*  Desc:                packet processing of alvs
*/

#ifndef ALVS_PACKET_PROCESSING_H_
#define ALVS_PACKET_PROCESSING_H_

#include <linux/ip.h>
#include <linux/tcp.h>
#include "global_defs.h"
#include "alvs_service.h"
#include "nw_routing.h"

/*prototypes*/
void alvs_packet_processing(uint8_t *frame_base, struct iphdr *ip_hdr) __fast_path_code;

/******************************************************************************
 * \brief       alvs unknown packet processing
 *              perform service classification - 3 tuple - DIP, dest port, IP protocol
 *              in case of failure in classification frame is sent to host, otherwise need
 *              to open new connection entry based on the scheduling algorithm and service type.
 *
 * \return      void
 */
static __always_inline
void alvs_unknown_packet_processing(uint8_t *frame_base, struct iphdr *ip_hdr, struct tcphdr *tcp_hdr)
{
	 uint32_t rc;
	 uint32_t found_result_size;
	 struct  alvs_service_classification_result *service_class_res_ptr;
	 struct  alvs_conn_classification_result *conn_class_res_ptr;

	 alvs_write_log(LOG_INFO, "(slow path) (0x%x:%d --> 0x%x:%d, protocol=%d)...",
			cmem_alvs.conn_class_key.client_ip,
			cmem_alvs.conn_class_key.client_port,
			cmem_alvs.conn_class_key.virtual_ip,
			cmem_alvs.conn_class_key.virtual_port,
			cmem_alvs.conn_class_key.protocol);

	 cmem_alvs.service_class_key.service_address = ip_hdr->daddr;
	 cmem_alvs.service_class_key.service_port = tcp_hdr->dest;
	 cmem_alvs.service_class_key.service_protocol = ip_hdr->protocol;

	 rc = ezdp_lookup_hash_entry(&shared_cmem_alvs.service_class_struct_desc,
				     (void *)&cmem_alvs.service_class_key,
				     sizeof(struct alvs_service_classification_key),
				     (void **)&service_class_res_ptr,
				     &found_result_size, 0,
				     cmem_wa.alvs_wa.service_hash_wa,
				     sizeof(cmem_wa.alvs_wa.service_hash_wa));

	if (likely(rc == 0)) {
		enum alvs_service_output_result	service_data_path_res = alvs_service_data_path(service_class_res_ptr->service_index,
											       frame_base,
											       ip_hdr,
											       tcp_hdr);
		if (service_data_path_res == ALVS_SERVICE_DATA_PATH_SUCCESS) {
			/*1st thread opened a new connection - just do route*/
			alvs_conn_do_route(frame_base);
		} else if (service_data_path_res == ALVS_SERVICE_DATA_PATH_RETRY) {
			/*all other packets tried to open new connection go through regular fast path*/
			rc = ezdp_lookup_hash_entry(&shared_cmem_alvs.conn_class_struct_desc,
						    (void *)&cmem_alvs.conn_class_key,
						    sizeof(struct alvs_conn_classification_key),
						    (void **)&conn_class_res_ptr,
						    &found_result_size, 0,
						    cmem_wa.alvs_wa.conn_hash_wa,
						    sizeof(cmem_wa.alvs_wa.conn_hash_wa));
			if (rc == 0) {
				alvs_conn_data_path(frame_base, ip_hdr, tcp_hdr, conn_class_res_ptr->conn_index);
			} else {
				alvs_write_log(LOG_ERR, "failed connection classification to newly created entry");
				/*drop frame*/
				alvs_update_discard_statistics(ALVS_ERROR_CONN_CLASS_LKUP_FAIL);
				alvs_discard_frame();
			}
		} /* all other cases - drop or sent to host - packet was already processed. */
	} else {
		alvs_write_log(LOG_ERR, "fail service classification lookup");
		alvs_update_incoming_port_stat_counter(ALVS_PACKET_FAIL_SERVICE_CLASS_LOOKUP);
		nw_host_do_route(&frame, frame_base);
	}
}

#endif /* ALVS_CLASSIFIER_H_ */
