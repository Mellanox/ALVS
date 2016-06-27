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
*  File:                nw_utils.h
*  Desc:                common interface utils
*/

#ifndef NW_UTILS_H_
#define NW_UTILS_H_


#include "defs.h"
#include "global_defs.h"
#include "nw_search_defs.h"


/******************************************************************************
 * \brief         update interface stat counter - special couters for nw
 * \return        void
 */
static __always_inline
void nw_interface_inc_counter(uint32_t counter_id)
{
	alvs_write_log(LOG_INFO, "nw_interface_inc_counter:  incrementing counter 0x%x", cmem_nw.interface_result.nw_stats_base + counter_id);
	ezdp_add_posted_ctr(cmem_nw.interface_result.nw_stats_base + counter_id, 1);
}

/******************************************************************************
 * \brief         interface lookup
 * \return        void
 */
static __always_inline
uint32_t nw_interface_lookup(int32_t	logical_id)
{
	return ezdp_lookup_table_entry(&shared_cmem_nw.interface_struct_desc,
				       logical_id, &cmem_nw.interface_result,
				       sizeof(struct nw_if_result), 0);
}

/******************************************************************************
 * \brief         interface lookup
 * \return        void
 */
static __always_inline
uint32_t nw_interface_lookup_host(void)
{
	return ezdp_lookup_table_entry(&shared_cmem_nw.interface_struct_desc,
				       ALVS_HOST_LOGICAL_ID,
				       &cmem_nw.host_interface_result,
				       sizeof(struct nw_if_result), 0);
}

/******************************************************************************
 * \brief         interface lookup
 * \return        void
 */
static __always_inline
uint8_t *nw_interface_get_host_mac_address(void)
{
	if (unlikely(nw_interface_lookup_host() != 0)) {
		alvs_write_log(LOG_CRIT, "error - host interface lookup fail!");
		return NULL;
	}
	return cmem_nw.host_interface_result.mac_address.ether_addr_octet;
}

/******************************************************************************
 * \brief         discard frame due to unexpected error. frame can not be sent to host.
 * \return        void
 */
static __always_inline
void nw_discard_frame(void)
{
	/*drop frame*/
	ezframe_free(&frame, 0);
}


/******************************************************************************
 * \brief         perform host route
 * \return        void
 */
static __always_inline
void nw_host_do_route(ezframe_t	__cmem * frame,
		      uint8_t __cmem * frame_base __unused)
{
	if (unlikely(nw_interface_lookup_host() != 0)) {
		alvs_write_log(LOG_CRIT, "error - host interface lookup fail!");
		/*drop frame*/
		nw_discard_frame();
		return;
	}
	ezframe_send_to_if(frame, cmem_nw.host_interface_result.output_channel, 0);
}

#endif  /*NW_UTILS_H_*/
