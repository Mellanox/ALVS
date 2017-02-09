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
*  Project:             NPS400 TC application
*  File:                tc_action_pedit.h
*  Desc:                pedit actions handling for tc
*/

#ifndef TC_ACTION_PEDIT_H_
#define TC_ACTION_PEDIT_H_

#include <net/ethernet.h>
#include <linux/icmp.h>
#include <linux/udp.h>
#include "global_defs.h"
#include "tc_conf.h"
#include "tc_defs.h"
#include "tc_utils.h"
#include "anl_log.h"

/******************************************************************************
 * \brief       lookup in the pedit action info table
 *
 * \return      return 0 in case of success, otherwise no match.
 */
static __always_inline
uint32_t tc_lookup_pedit_action_info_table(uint32_t action_info_index)
{
	return ezdp_lookup_table_entry(&shared_cmem_tc.tc_pedit_action_info_struct_desc,
				       action_info_index, &cmem_tc.action_pedit_info_res,
				       sizeof(struct tc_pedit_action_info_result), 0);

}

/******************************************************************************
 * \brief       tc_action_pedit_recalc_icmp_checksum - calculate icmp checksum
 *
 * \return      void.
 */
static __always_inline
void tc_action_pedit_recalc_icmp_checksum(struct icmphdr *icmp_hdr,
					uint32_t	old_value,
					uint32_t	new_value)
{
	icmp_hdr->checksum = ezdp_add_checksum(ezdp_sub_checksum(icmp_hdr->checksum, old_value), new_value);
}

/******************************************************************************
 * \brief       tc_action_pedit_recalc_tcp_checksum - calculate tcp checksum
 *
 * \return      void.
 */
static __always_inline
void tc_action_pedit_recalc_tcp_checksum(struct tcphdr	*tcp_hdr,
					uint32_t	old_value,
					uint32_t	new_value)
{
	tcp_hdr->check = ezdp_add_checksum(ezdp_sub_checksum(tcp_hdr->check, old_value), new_value);
}

/******************************************************************************
 * \brief       tc_action_pedit_recalc_udp_checksum - calculate udp checksum
 *
 * \return      void.
 */
static __always_inline
void tc_action_pedit_recalc_udp_checksum(struct udphdr	*udp_hdr,
					uint32_t	old_value,
					uint32_t	new_value)
{
	udp_hdr->check = ezdp_add_checksum(ezdp_sub_checksum(udp_hdr->check, old_value), new_value);
}

/******************************************************************************
 * \brief       tc_action_pedit_recalc_ip_checksum - calculate ip checksum
 *
 * \return      void.
 */
static __always_inline
void tc_action_pedit_recalc_ip_checksum(struct iphdr	*ip_hdr,
					uint32_t	old_value,
					uint32_t	new_value)
{
	ip_hdr->check = ezdp_add_checksum(ezdp_sub_checksum(ip_hdr->check, old_value), new_value);
}

/******************************************************************************
 * \brief       tc_action_pedit_handle_checksum - recalc IP checksum and TCP/UDP if needed
 *
 * \return      void.
 */
static __always_inline
void tc_action_pedit_handle_checksum(struct iphdr	*ip_hdr,
				     uint32_t		old_value,
				     uint32_t		new_value,
				     bool		check_tcp_udp)
{
	struct udphdr	*udp_hdr;
	struct tcphdr	*tcp_hdr;

	tc_action_pedit_recalc_ip_checksum(ip_hdr, old_value, new_value);

	if (check_tcp_udp) {
		if (packet_meta_data.ip_next_protocol.tcp) {
			tcp_hdr = (struct tcphdr *)((uint8_t *)ip_hdr + (ip_hdr->ihl << 2));
			tc_action_pedit_recalc_tcp_checksum(tcp_hdr, old_value, new_value);
		} else if (packet_meta_data.ip_next_protocol.udp) {
			udp_hdr = (struct udphdr *)((uint8_t *)ip_hdr + (ip_hdr->ihl << 2));
			if (udp_hdr->check != 0) {
				tc_action_pedit_recalc_udp_checksum(udp_hdr, old_value, new_value);
			}
		}
	}
}


/******************************************************************************
 *\brief       tc_action_handle_pedit - update action index stats
 *		and peform pedit action. currently supporting only layered pedit.
 *
 *\return      enum tc_action_rc - stolen - packet drop or check next action if all ok.
 */
static __always_inline
enum tc_action_rc tc_action_handle_pedit(ezframe_t __cmem       * frame,
					 uint8_t		*frame_base,
					 struct iphdr		*ip_hdr,
					 uint32_t		pedit_action_info_index)
{
	int		temp_offset;
	uint32_t	buf_headroom;

	anl_write_log(LOG_DEBUG, "pedit_action_info_index 0x%x", pedit_action_info_index);
	if (!ip_hdr) {
		anl_write_log(LOG_ERR, "pedit action but frame is not IP");
		tc_discard_frame();
		return TC_ACTION_RC_PACKET_STOLEN;
	}
	anl_write_log(LOG_DEBUG, "ip_hdr");
	buf_headroom = ezframe_get_buf_headroom(frame);

	do {
		/*perform lookup in  pedit action info table*/
		if (tc_lookup_pedit_action_info_table(pedit_action_info_index) != 0) {
			anl_write_log(LOG_ERR, "no match in pedit action info for index %d", pedit_action_info_index);
			tc_discard_frame();
			return TC_ACTION_RC_PACKET_STOLEN;
		}
		anl_write_log(LOG_DEBUG, "match in pedit action info for extra index %d", pedit_action_info_index);

		/*check raw mode*/
		if (cmem_tc.action_pedit_info_res.offmask) {
			anl_write_log(LOG_DEBUG, "offmask != 0");

			temp_offset = (int)(cmem_tc.action_pedit_info_res.at + packet_meta_data.ip_offset);
			if (temp_offset > (int)(EZFRAME_BUF_DATA_SIZE - 4 - buf_headroom) ||
				temp_offset < (int)buf_headroom) {
				anl_write_log(LOG_ERR, "pedit offset %d is not valid", temp_offset);
				tc_discard_frame();
				return TC_ACTION_RC_PACKET_STOLEN;

			}
			uint8_t	*packet_off = (uint8_t *)&frame_base[temp_offset];

			/*calc final offset*/
			cmem_tc.action_pedit_info_res.off += (*packet_off & cmem_tc.action_pedit_info_res.offmask) >> cmem_tc.action_pedit_info_res.shift;
		}

		if (ezdp_mod(cmem_tc.action_pedit_info_res.off, 4, 0, 0)) {
			anl_write_log(LOG_ERR, "pedit offset %d is not aligned to 32", cmem_tc.action_pedit_info_res.off);
			tc_discard_frame();
			return TC_ACTION_RC_PACKET_STOLEN;
		}

		temp_offset = (int)(cmem_tc.action_pedit_info_res.off + packet_meta_data.ip_offset);
		if (temp_offset > (int)(EZFRAME_BUF_DATA_SIZE - 4 - buf_headroom) ||
			(int)(temp_offset + buf_headroom) < (int)buf_headroom) {
			anl_write_log(LOG_ERR, "pedit offset %d is not valid", cmem_tc.action_pedit_info_res.off);
			tc_discard_frame();
			return TC_ACTION_RC_PACKET_STOLEN;

		}

		uint32_t *modify_ptr = (uint32_t *)&frame_base[temp_offset];
		uint32_t old_value = *((uint32_t *)modify_ptr);

		anl_write_log(LOG_DEBUG, "old_value 0x%x",
			      old_value)
		*modify_ptr = ((*((uint32_t *)modify_ptr) & cmem_tc.action_pedit_info_res.mask) ^ cmem_tc.action_pedit_info_res.val);

		anl_write_log(LOG_DEBUG, "modified field 0x%x",
			      *((uint32_t *)modify_ptr));


		/*check if need to update checksum - src,dst, protocol changed*/
		if (cmem_tc.action_pedit_info_res.off == 8 ||
		cmem_tc.action_pedit_info_res.off == 12 ||
		cmem_tc.action_pedit_info_res.off == 16) {
			anl_write_log(LOG_DEBUG, "Update IP pseudo header fields pedit_off = %d", cmem_tc.action_pedit_info_res.off);
			tc_action_pedit_handle_checksum(ip_hdr,
							old_value,
							*modify_ptr,
							true);
		} else if (cmem_tc.action_pedit_info_res.off < 20) {
			anl_write_log(LOG_DEBUG, "Update IP NON pseudo header fields");
			tc_action_pedit_handle_checksum(ip_hdr,
							old_value,
							*modify_ptr,
							false);
		} else if (packet_meta_data.ip_next_protocol.tcp) {
			anl_write_log(LOG_DEBUG, "Update TCP header fields pedit_off = %d", cmem_tc.action_pedit_info_res.off);
			anl_write_log(LOG_INFO, "cmem_tc.action_pedit_info_res.off 0x%x", cmem_tc.action_pedit_info_res.off);
			struct l4_hdr *l4_hdr = (struct l4_hdr *)((uint8_t *)ip_hdr + (ip_hdr->ihl << 2));
			struct tcphdr	*tcp_hdr = (struct tcphdr *)l4_hdr;

			tc_action_pedit_recalc_tcp_checksum(tcp_hdr, old_value, *((uint32_t *)l4_hdr));
		} else if (packet_meta_data.ip_next_protocol.udp) {
			anl_write_log(LOG_DEBUG, "Update UDP header fields pedit_off = %d", cmem_tc.action_pedit_info_res.off);
			struct l4_hdr	*l4_hdr = (struct l4_hdr *)((uint8_t *)ip_hdr + (ip_hdr->ihl << 2));
			struct udphdr	*udp_hdr = (struct udphdr *)l4_hdr;

			if (udp_hdr->check != 0) {
				tc_action_pedit_recalc_udp_checksum(udp_hdr, old_value, *((uint32_t *)l4_hdr));
			}
		} else if (packet_meta_data.ip_next_protocol.icmp_igmp) {
			struct icmphdr	*icmp_hdr = (struct icmphdr *)((uint8_t *)ip_hdr + (ip_hdr->ihl << 2));

			tc_action_pedit_recalc_icmp_checksum(icmp_hdr, old_value, *((uint32_t *)icmp_hdr));
		}

		pedit_action_info_index = cmem_tc.action_pedit_info_res.next_key_index;

	} while (cmem_tc.action_pedit_info_res.is_next_key_valid);

	return	TC_ACTION_RC_CHECK_NEXT_ACTION;
}

#endif  /*TC_ACTION_PEDIT_H_*/
