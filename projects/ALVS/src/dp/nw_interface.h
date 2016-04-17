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
*/

#ifndef NW_INTERFACE_H_
#define NW_INTERFACE_H_

/******************************************************************************
 * \brief	  interface lookup
 * \return	  void
 */
static __always_inline
uint32_t nw_interface_lookup(int32_t logical_id)
{
	return  ezdp_lookup_table_entry(&shared_cmem.interface_struct_desc,
									logical_id,
									&cmem.interface_result,
									sizeof(struct  dp_interface_result),
									0);
}

/******************************************************************************
 * \brief	  get interface dp path
 * \return	  enum dp_path_type
 */
static __always_inline
enum dp_path_type nw_interface_get_dp_path(int32_t logical_id)
{
	if (ezdp_lookup_table_entry(&shared_cmem.interface_struct_desc,
								logical_id,
								&cmem.interface_result,
								sizeof(struct  dp_interface_result),
								0))
	{
		return cmem.interface_result.path_type;
	}
	return DP_PATH_NOT_VALID;
}

/******************************************************************************
 * \brief	  increment interface stat counter by value
 * \return	  void
 */
static __always_inline
void nw_interface_inc_statistic_counter(uint8_t logical_id, uint32_t counter_id, uint16_t counters_per_interface, uint32_t value)
{
	struct ezdp_sum_addr temp_stat_address;

	printf("frame send to logical ID = %d. counter ID = %d\n", logical_id, counter_id);

	temp_stat_address.raw_data = shared_cmem.nw_interface_stats_base_address.raw_data;
	temp_stat_address.element_index += (counters_per_interface << logical_id) + counter_id;
	ezdp_add_posted_ctr_async(temp_stat_address.raw_data, value);
}

#endif /* ALVS_INTERFACE_H_ */
