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
*  Project:             NPS400 application
*  File:                nw_routing.c
*  Desc:                network infrastructure file containing routing functionality
*/

#include "nw_routing.h"

/******************************************************************************
 * \brief         send frame directly to the interface given by direct_if
 * \return        void
 */
void nw_direct_route(ezframe_t __cmem * frame, uint8_t __cmem * frame_base, uint8_t out_if, bool is_lag)
{
	uint32_t rc;

	anl_write_log(LOG_DEBUG, "execute nw_direct_route");

	if (unlikely(nw_calc_egress_if(frame_base, out_if, is_lag) == false)) {
		nw_interface_inc_counter(NW_IF_STATS_FAIL_INTERFACE_LOOKUP);
		nw_discard_frame();
		return;
	}

	/* Store modified segment data */
	rc = ezframe_store_buf(frame,
			       frame_base,
			       ezframe_get_buf_len(frame),
			       0);
	if (rc != 0) {
		anl_write_log(LOG_DEBUG, "Ezframe store buf was failed");
		nw_interface_inc_counter(NW_IF_STATS_FAIL_STORE_BUF);
	}

	ezframe_send_to_if(frame, cmem_nw.egress_if_result.output_channel, 0);
}
