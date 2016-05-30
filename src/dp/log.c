/*/* Copyright (c) 2016 Mellanox Technologies, Ltd. All rights reserved.
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
*  File:                log.c
*  Created on: 		May 15, 2016
*  Desc:                performs logging functionality for DP
*
*/

#include "log.h"


char ptr_pri_facility[LOG_DEBUG+1][SYSLOG_PRI_FACILITY_STRING_SIZE] = {
	"<24>", /*LOG_MAKEPRI(LOG_DAEMON, LOG_EMERG)*/
	"<25>", /*LOG_MAKEPRI(LOG_DAEMON, LOG_ALERT)*/
	"<26>", /*LOG_MAKEPRI(LOG_DAEMON, LOG_CRIT)*/
	"<27>", /*LOG_MAKEPRI(LOG_DAEMON, LOG_ERR)*/
	"<28>", /*LOG_MAKEPRI(LOG_DAEMON, LOG_WARNING)*/
	"<29>", /*LOG_MAKEPRI(LOG_DAEMON, LOG_NOTICE)*/
	"<30>", /*LOG_MAKEPRI(LOG_DAEMON, LOG_INFO)*/
	"<31>" /*LOG_MAKEPRI(LOG_DAEMON, LOG_DEBUG)*/
};
struct syslog_info syslog_info __cmem_shared_var;

bool open_log(struct syslog_info *user_syslog_info)
{

	if(user_syslog_info->send_cb == NULL)
		return false;

	/*save in syslog_wa relevant fields for further work*/
	syslog_info.applic_name_size =
			MIN(user_syslog_info->applic_name_size,
			    SYSLOG_APPLIC_NAME_STRING_SIZE);
	ezdp_mem_copy(syslog_info.applic_name,
		      user_syslog_info->applic_name,
		      syslog_info.applic_name_size);

	syslog_info.send_cb = user_syslog_info->send_cb;

	syslog_info.dest_ip = user_syslog_info->dest_ip;
	syslog_info.src_ip = user_syslog_info->src_ip;
	syslog_info.dest_port = user_syslog_info->dest_port;

	/*calculation of the remained space for user string in the
	 * mixed buffer - where part is syslog message
	 * and the rest user string*/
	syslog_info.remained_length_for_user_string =
				SYSLOG_BUF_DATA_SIZE
				- SYSLOG_PRI_FACILITY_STRING_SIZE
				- syslog_info.applic_name_size;

	return true;
}

void set_syslog_template(struct net_hdr  *net_hdr_info,int total_frame_length)
{
	/*fill fields of IPV4 header*/
	net_hdr_info->ipv4.version = IPVERSION;
	net_hdr_info->ipv4.ihl = sizeof(struct iphdr)/4;
	net_hdr_info->ipv4.tos = 0;
	net_hdr_info->ipv4.tot_len = sizeof(struct iphdr)
				     + sizeof(struct udphdr)
				     + total_frame_length;
	net_hdr_info->ipv4.id = 0;
	net_hdr_info->ipv4.frag_off = 0x4000; // TODO IP_DF;
	net_hdr_info->ipv4.ttl = MAXTTL;
	net_hdr_info->ipv4.protocol = IPPROTO_UDP;
	net_hdr_info->ipv4.check = 0;
	net_hdr_info->ipv4.saddr = syslog_info.src_ip;
	net_hdr_info->ipv4.daddr = syslog_info.dest_ip;

	/*fill fields of UDP header*/
	net_hdr_info->udp.source = syslog_info.dest_port;
	net_hdr_info->udp.dest = syslog_info.dest_port;
	net_hdr_info->udp.len = sizeof(struct udphdr)
				+ total_frame_length;
	net_hdr_info->udp.check = 0;

	/*update ipv4 checksum on updated header*/
	ezframe_update_ipv4_checksum(&net_hdr_info->ipv4);

}
void write_log(int priority, char *str, int length, char * __cmem syslog_wa, int syslog_wa_size)
{
	struct net_hdr  *net_hdr_info =
		(struct net_hdr  *)((struct syslog_wa_info  *)syslog_wa)->
								frame_data;
	int total_frame_length = length +
				SYSLOG_PRI_FACILITY_STRING_SIZE +
				syslog_info.applic_name_size;
	int rc;
	int num_of_bufs = 2;
	int buf_len;
	uint8_t *ptr;

	/*check that there are enough space for priority_facility*/
	assert(SYSLOG_PRI_FACILITY_STRING_SIZE<=SYSLOG_BUF_DATA_SIZE);
	/*check that wa_size provided by the user not less than
	 * required by private syslog data structure */
	assert(syslog_wa_size >= sizeof(struct syslog_wa_info));
	/*restriction - check that ip_header and udp_header
	 * included in the first buffer without deviding it*/
	assert((sizeof(struct iphdr) + sizeof(struct udphdr)) <=
	       	       	       SYSLOG_FIRST_BUFFER_SIZE);

	/*check that there are enough space for application name */
	assert((SYSLOG_PRI_FACILITY_STRING_SIZE +
			syslog_info.applic_name_size)
	       	       	    <= SYSLOG_BUF_DATA_SIZE);

	/*fill syslog ipv4/udp template*/
	set_syslog_template(net_hdr_info,total_frame_length);

	/*create new frame*/
	 rc = ezframe_new(&((struct syslog_wa_info  *)syslog_wa)->frame,
			  net_hdr_info,
		     sizeof (struct net_hdr), SYSLOG_BUF_HEADROOM, 0);
	 if (rc!= 0)
		 return;

	 /*required for append*/
	ezframe_next_buf(&((struct syslog_wa_info  *)syslog_wa)->frame, 0);

	/*copy priority_facility to the buffer*/
	ezdp_mem_copy(((struct syslog_wa_info  *)syslog_wa)->frame_data,
		      &ptr_pri_facility[priority][0],
		      SYSLOG_PRI_FACILITY_STRING_SIZE);

	/*copy priority_facility to the buffer from offset
	 * after priority_facility*/
	ezdp_mem_copy((uint8_t *)&((struct syslog_wa_info  *)syslog_wa)->
		      	      frame_data[SYSLOG_PRI_FACILITY_STRING_SIZE],
			      syslog_info.applic_name,
			      (uint32_t)syslog_info.applic_name_size);


	/*calculation of the  start of the user string after
	 * adding syslog private message*/
	ptr = (uint8_t *)&((struct syslog_wa_info  *)syslog_wa)->
				frame_data[SYSLOG_PRI_FACILITY_STRING_SIZE +
			   syslog_info.applic_name_size];

	/*calculation of buf_len to add user string*/
	buf_len = MIN (length, syslog_info.remained_length_for_user_string);
	/*copy user string to the correct offset of the buffer*/
	ezdp_mem_copy (ptr, str, buf_len);

	/*append buffer to the frame*/
	rc = ezframe_append_buf(&((struct syslog_wa_info  *)syslog_wa)
			    ->frame,
			    ((struct syslog_wa_info  *)syslog_wa)->
			    frame_data,
			    SYSLOG_BUF_DATA_SIZE -
			    syslog_info.remained_length_for_user_string,
			    0);
	if (rc != 0) {
		ezframe_free(&((struct syslog_wa_info  *)syslog_wa)->
			     	     frame, 0);
		return ;
	}

	/*update pointer of the user data*/
	str += buf_len;
	/*update length remained to copy from the user data*/
	length -= buf_len;
	/*update buffer for the next copy to the wa*/
	ptr = (uint8_t *)((struct syslog_wa_info  *)syslog_wa)->
							frame_data;
	 /*required for append*/
	ezframe_next_buf(&((struct syslog_wa_info  *)syslog_wa)->frame, 0);

	/*restrictions - the buffer contains in summary not more than 3 buffers*/
	while ((length > 0) && (num_of_bufs < SYSLOG_MAX_NUM_OF_BUF))
	 {
		/*calculation of buf_len to add user string*/
		buf_len = MIN (length, SYSLOG_BUF_DATA_SIZE);
		/*copy user string to the correct offset of the buffer*/
		ezdp_mem_copy (ptr, str, buf_len);
		/*append buffer to the frame*/
		rc = ezframe_append_buf(&((struct syslog_wa_info  *)syslog_wa)
				    ->frame,
				    ((struct syslog_wa_info  *)syslog_wa)->
				    frame_data,
				    buf_len, 0);
		if (rc != 0) {
			ezframe_free(&((struct syslog_wa_info  *)syslog_wa)->
				     	     frame, 0);
			return ;
		}

		/*update frame to point to the last buffer*/
		 ezframe_next_buf(&((struct syslog_wa_info  *)syslog_wa)
				      ->frame, 0);
		/*update pointer of the user data*/
		str += buf_len;
		/*update length remained to copy from the user data*/
		length -= buf_len;
		/*update buffer for the next copy to the wa*/
		ptr = (uint8_t *)((struct syslog_wa_info  *)syslog_wa)->
								frame_data;
		/*update number of buffers in the frame -
		 * NOTE - in summary allowed 3 buffers*/
		num_of_bufs += 1;

	 }
	/*if user sent send_cb - call it*/
	syslog_info.send_cb(&((struct syslog_wa_info  *)syslog_wa)->
				    frame);

}
