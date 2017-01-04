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
*
*  Project:             NPS400 ALVS application
*  File:                anl_log.c
*  Created on:          May 18, 2016
*  Desc:                performs logging functionality for ANL DP
*
*/

#include "anl_log.h"
#include "nw_routing.h"

#define SYSLOG_SERVER_IP          "169.254.42.41"
#define SYSLOG_CLIENT_ETH_ADDR    {0x00, 0x02, 0xc9, 0x42, 0x42, 0x43}

bool anl_open_log(void)
{
	struct syslog_info syslog_info;
	struct in_addr dest_ip, src_ip;
	char *syslog_client_ip = "169.254.42.42";

	syslog_info.send_cb = anl_send;
	syslog_info.dest_port = SYSLOG_UDP_SERVER_PORT;

	syslog_info.applic_name_size =
		snprintf(syslog_info.applic_name,
			 SYSLOG_APPLIC_NAME_STRING_SIZE,
			 " anl_dp ");

	/*get IP from string*/
	inet_aton(SYSLOG_SERVER_IP, &dest_ip);
	inet_aton(syslog_client_ip, &src_ip); /* store IP in antelope */

	syslog_info.dest_ip = dest_ip.s_addr;
	syslog_info.src_ip = src_ip.s_addr;

	/*call open_log of MIDDLE_WARE*/
	return open_log(&syslog_info);
}

int anl_send(ezframe_t  __cmem * frame)
{
	int rc;
	uint8_t *frame_base;
	uint32_t orig_length;
	struct ether_header *eth_p;
	uint8_t src_eth_addr[6] = SYSLOG_CLIENT_ETH_ADDR;

	/*check that wa provided by middle ware not less than
	 * required for the work with the first buffer
	 */
	assert(sizeof(syslog_work_area) >= SYSLOG_BUF_DATA_SIZE);

	rc = ezframe_first_buf(frame, 0);
	if (rc) {
		return rc;
	}

	assert(ezframe_get_buf_headroom(frame) >= sizeof(struct ether_header));

	/*load the buffer and get the start of the data*/
	frame_base = ezframe_load_buf(frame,
				      syslog_work_area.frame_info.frame_data + SYSLOG_FIRST_BUFFER_SIZE,
				      &orig_length,
				      EZFRAME_LOAD_DATA_WITHOUT_OFFSET);


	/*move the start of the data to ethernet_header where
	 * should update the Ethernet header
	 */
	eth_p = (struct ether_header *)(frame_base - sizeof(struct ether_header));

	/*fill ethernet source MAC*/
	ezdp_mem_copy((uint8_t *)eth_p->ether_shost, src_eth_addr,
		      sizeof(struct ether_addr));
	/*fill ethernet TYPE*/
	eth_p->ether_type = ETHERTYPE_IP;

	nw_local_host_route(frame, frame_base, orig_length, HOST_LOGICAL_ID);

	return 0;
}
