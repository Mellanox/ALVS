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
*  File:                nw_interface.h
*  Desc:                network infrastructure file containing interfaces functionality
*/

#ifndef NW_INTERFACE_H_
#define NW_INTERFACE_H_

#include "nw_search_defs.h"

/******************************************************************************
 * \brief         interface lookup
 * \return        void
 */
static __always_inline
uint32_t nw_interface_lookup(int32_t logical_id)
{
	return ezdp_lookup_table_entry(&shared_cmem.interface_struct_desc,
				       logical_id, &cmem.interface_result,
				       sizeof(struct nw_if_result), 0);
}

/******************************************************************************
 * \brief         get interface dp path
 * \return        enum dp_path_type
 */
static __always_inline
enum dp_path_type nw_interface_get_dp_path(int32_t logical_id)
{
	if (nw_interface_lookup(logical_id) == 0) {
		return cmem.interface_result.path_type;
	}
	return (enum dp_path_type)DP_PATH_NOT_VALID;
}

/******************************************************************************
 * \brief         get interface mac address
 * \return        pointer to mac address
 */
static __always_inline
u_int8_t *nw_interface_get_mac_address(int32_t logical_id)
{
	if (nw_interface_lookup(logical_id) == 0) {
		return cmem.interface_result.mac_address.ether_addr_octet;
	}
	return NULL;
}

/******************************************************************************
 * \brief         update interface stat counter
 * \return        void
 */
static __always_inline
void nw_interface_update_statistic_counter(uint8_t logical_id, uint32_t counter_id)
{
	struct ezdp_sum_addr temp_stat_address;

	printf("frame send to logical ID = %d. counter ID = %d\n", logical_id, counter_id);

	temp_stat_address.raw_data = shared_cmem.nw_interface_stats_base_address.raw_data;
	temp_stat_address.element_index += ((logical_id * DP_NUM_COUNTERS_PER_INTERFACE) << 1) + counter_id;
	ezdp_dual_add_posted_ctr_async(temp_stat_address.raw_data, cmem.frame.job_desc.frame_desc.frame_length, 1);
}


#endif /* ALVS_INTERFACE_H_ */
