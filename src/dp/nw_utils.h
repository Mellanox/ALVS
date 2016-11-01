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

#include "anl_log.h"
#include "global_defs.h"
#include "nw_search_defs.h"

/******************************************************************************
 * \brief         perform application info lookup
 * \return        lookup result
 */
static __always_inline
uint32_t nw_app_info_lookup(uint32_t key, void __cmem * entry_ptr, uint32_t entry_ptr_size)
{
	return ezdp_lookup_table_entry(&shared_cmem_nw.app_info_struct_desc, key, entry_ptr, entry_ptr_size, 0);
}

/******************************************************************************
 * \brief         update interface stat counter - special couters for nw
 * \return        void
 */
static __always_inline
void nw_interface_inc_counter(uint32_t counter_id)
{
	ezdp_add_posted_ctr(cmem_nw.ingress_if_result.stats_base + counter_id, 1);
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
 * \brief         interface lookup
 * \return        lookup result
 */
static __always_inline
uint32_t nw_if_ingress_lookup(int8_t logical_id)
{
	return ezdp_lookup_table_entry(&shared_cmem_nw.interface_struct_desc,
				       logical_id,
				       &cmem_nw.ingress_if_result,
				       sizeof(struct nw_if_result), 0);
}

/******************************************************************************
 * \brief         interface lookup
 * \return        lookup result
 */
static __always_inline
uint32_t nw_if_egress_lookup(int8_t logical_id)
{
	return ezdp_lookup_table_entry(&shared_cmem_nw.interface_struct_desc,
				       logical_id,
				       &cmem_nw.egress_if_result,
				       sizeof(struct nw_if_result), 0);
}

/******************************************************************************
 * \brief         calculate & update egress interface
 * \return        bool - true for success, false for fail
 */
static __always_inline
bool nw_calc_egress_if(uint8_t __cmem * frame_base, uint8_t out_if, bool is_lag)
{
	uint32_t hash_value = 0;

	if (is_lag == 1) {
		/*do hash on destination mac - for lag calculation*/
		hash_value = ezdp_hash(((uint32_t *)frame_base)[0],
				       ((uint32_t *)frame_base)[1],
				       LOG2(NUM_OF_LAG_MEMBERS),
				       sizeof(struct ether_addr),
				       0,
				       EZDP_HASH_BASE_MATRIX_HASH_BASE_MATRIX_0,
				       EZDP_HASH_PERMUTATION_0);
	}

	if (unlikely(nw_if_egress_lookup(out_if + hash_value) != 0)) {
		anl_write_log(LOG_ERR, "network egress interface = %d lookup fail", out_if + hash_value);
		return false;
	}
	return true;
}

/******************************************************************************
 * \brief         interface lookup
 * \return        host mac address
 */
static __always_inline
uint8_t *nw_interface_get_mac_address(int8_t logical_id)
{
	if (unlikely(nw_if_egress_lookup(logical_id) != 0)) {
		anl_write_log(LOG_CRIT, "error - egress interface lookup fail!");
		return NULL;
	}
	return cmem_nw.egress_if_result.mac_address.ether_addr_octet;
}

#endif  /*NW_UTILS_H_*/
