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
*  File:                tc_packet_processing.c
*  Desc:                tc packet processing entry point
*/

#include <net/ethernet.h>
#include "tc_packet_processing.h"
struct ezdp_rtc  real_time_clock __cmem_var;

uint32_t tc_bitmask_array[32] __imem_1_cluster_var = {
	0xffffffff,
	0x80000000, 0xc0000000, 0xe0000000, 0xf0000000,
	0xf8000000, 0xfc000000, 0xfe000000, 0xff000000,
	0xff800000, 0xffc00000, 0xffe00000, 0xfff00000,
	0xfff80000, 0xfffc0000, 0xfffe0000, 0xffff0000,
	0xffff8000, 0xffffc000, 0xffffe000, 0xfffff000,
	0xfffff800, 0xfffffc00, 0xfffffe00, 0xffffff00,
	0xffffff80, 0xffffffc0, 0xffffffe0, 0xfffffff0,
	0xfffffff8, 0xfffffffc, 0xfffffffe
};

enum tc_processing_rc tc_classification_perform_rule_wrapper(ezframe_t __cmem * frame,
							     uint8_t *frame_base,
							     struct iphdr  *ip_hdr,
							     uint32_t logical_id,
							     struct match_info *match_info,
							     bool is_flow_cached)
{
	return tc_classification_perform_rule(frame, frame_base, ip_hdr, logical_id, match_info, is_flow_cached);
}

enum tc_processing_rc tc_handle_slow_classification_process_wrapper(ezframe_t __cmem * frame, uint8_t *frame_base,
								    struct match_info *match_info,
								    struct iphdr *ip_hdr, uint32_t logical_id,
								    bool is_continue_after_cache)
{
	return tc_handle_slow_classification_process(frame, frame_base, match_info, ip_hdr, logical_id, is_continue_after_cache);
}


/******************************************************************************
 * \brief       tc packet processing function
 *              build classification key and start classification process
 *
 * \return        return if packet was stolen by TC process or need to continue to NW process (accept)
 */

/*this "fake" function added for defining match_info and via recursion to use only pointer of this variable.
 * Call from inside code for RECLASSIFY to tc_packet_process with pointer to match_info.
 * This is WA for recursion.
 */
enum tc_processing_rc tc_packet_processing(ezframe_t __cmem * frame, uint8_t *frame_base, struct iphdr *ip_ptr, uint32_t logical_id)
{
	struct match_info match_info;
	/*TODO add a check if IP and TCP/UDP is in packet*/

	return tc_packet_process(frame, frame_base, &match_info, ip_ptr, logical_id);

}

enum tc_processing_rc tc_packet_process(ezframe_t __cmem * frame, uint8_t *frame_base, struct match_info *match_info, struct iphdr *ip_ptr, uint32_t logical_id)
{
#if 0
		tc_handle_flow_cache_process(frame, frame_base);
	#endif
		return tc_handle_slow_classification_process(frame, frame_base, match_info, ip_ptr, logical_id, false);
}

