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
*  File:                alvs_server.h
*  Desc:                handle server functionality
*
*/

#ifndef ALVS_SERVER_H_
#define ALVS_SERVER_H_

#include <linux/ip_vs.h>


/******************************************************************************
 * \brief       lookup in scheduling info table according to a given index
 *
 * \return      return 0 in case of success, otherwise no match.
 */
static __always_inline
uint32_t alvs_server_sched_lookup(uint16_t scheduling_index)
{
	return ezdp_lookup_table_entry(&shared_cmem_alvs.sched_info_struct_desc,
				scheduling_index, &cmem_alvs.sched_info_result,
				sizeof(struct alvs_sched_info_result), 0);

}

/******************************************************************************
 * \brief       lookup in server info table according to a given index
 *
 * \return      return 0 in case of success, otherwise no match.
 */
static __always_inline
uint32_t alvs_server_info_lookup(uint32_t server_index)
{
	return ezdp_lookup_table_entry(&shared_cmem_alvs.server_info_struct_desc,
				server_index, &cmem_alvs.server_info_result,
				sizeof(struct alvs_server_info_result), 0);

}


/******************************************************************************
 * \brief       alvs_server_overload_on_create_conn - update overloaded flag according to
 *              the current connections in the process
 *              of creation of the new  connection. when process of available server is executed.
 *
 * \return	none
 */

static __always_inline
uint32_t alvs_server_overload_on_create_conn(uint16_t server_index)
{

	uint64_t counter = 0;
	uint32_t server_flags = 0;

	/* alvs_conn_create_new_entry
	 * We need to check if overloaded bit should be set as
	 * number of connections per this server is increased.
	 */
	server_flags = ezdp_atomic_read32_sum_addr(cmem_alvs.server_info_result.server_flags_dp_base);
	ezdp_read_and_inc_single_ctr(cmem_alvs.server_info_result.server_on_demand_stats_base + ALVS_SERVER_STATS_CONNECTION_TOTAL_OFFSET,
				     1, &cmem_wa.alvs_wa.counter_work_area, 0);
	if (cmem_alvs.server_info_result.u_thresh != 0) {
		counter = cmem_wa.alvs_wa.counter_work_area + 1;

		alvs_write_log(LOG_DEBUG, "alvs_server_overload_on_create_conn");
		alvs_write_log(LOG_DEBUG, "server_index  = %d,  server_flags = 0x%x, u_thresh = %d, l_thresh = %d, sched_conns = %d",
			       server_index, cmem_alvs.server_info_result.server_flags,
			       cmem_alvs.server_info_result.u_thresh,
			       cmem_alvs.server_info_result.l_thresh,
			       (uint32_t)counter);
		if (counter == cmem_alvs.server_info_result.u_thresh) {
			/* read if OVERLOADED was set before - if yes - new connection can not created */
			/* it's required if number of connections was reduced by function of delete_connection */
			/* but OVERLOADED flag wasn't cleared */
			if (server_flags & IP_VS_DEST_F_OVERLOAD) {
				/* if OVERLOAD flag is set - connection can not be created */
				ezdp_dec_single_ctr(cmem_alvs.server_info_result.server_on_demand_stats_base + ALVS_SERVER_STATS_CONNECTION_TOTAL_OFFSET,
						    1);
				return server_flags;
			}
			/*need to set OVERLOAD for the next bind_connection*/
			ezdp_atomic_or32_sum_addr(cmem_alvs.server_info_result.server_flags_dp_base, IP_VS_DEST_F_OVERLOAD);
			alvs_write_log(LOG_DEBUG, "counter %d == u_thresh %d, server_flags = 0x%x", (uint32_t)counter, cmem_alvs.server_info_result.u_thresh, server_flags);
			/* return NOT_OVERLOADED - this bit set for next connections*/
			return !IP_VS_DEST_F_OVERLOAD;
		} else if (counter > cmem_alvs.server_info_result.u_thresh) {
			/* set OVERLOADED - as due race this bit can be still unset*/
			ezdp_atomic_or32_sum_addr(cmem_alvs.server_info_result.server_flags_dp_base, IP_VS_DEST_F_OVERLOAD);
			/* its not allowed to open new connections if the counter of current connections (including preset connection) is above thresholds*/
			ezdp_dec_single_ctr(cmem_alvs.server_info_result.server_on_demand_stats_base + ALVS_SERVER_STATS_CONNECTION_TOTAL_OFFSET,
					    1);
			alvs_write_log(LOG_DEBUG, "counter %d > u_thresh %d ", (uint32_t)counter, cmem_alvs.server_info_result.u_thresh);
			/* return OVERLOADED - in summary number connections above upper threshold */
			return IP_VS_DEST_F_OVERLOAD;
		}
		/* the case - number of connections less than upper threshold */
		/* check if number of connections is bellow l_thresholds */
		/* it can be if server is modified with l_thresh greater that it was */
		/* so in creation of new connection should check if number of connections below of l_thres */
		/* if yes - clear overloading */
		if (cmem_alvs.server_info_result.l_thresh != 0) {
			if (counter <= cmem_alvs.server_info_result.l_thresh) {
				/* if number of connections is bellow thresholds clear OVERLOAD flag */
				ezdp_atomic_and32_sum_addr(cmem_alvs.server_info_result.server_flags_dp_base,
							   ~IP_VS_DEST_F_OVERLOAD);
				alvs_write_log(LOG_DEBUG, "counter %d <= l_thresh %d, server_flags = 0x%x", (uint32_t)counter, cmem_alvs.server_info_result.u_thresh, server_flags);
				return !IP_VS_DEST_F_OVERLOAD;
			}
		}
		/* if number of connections between low threshold and high threshold return current server_flag */
		if (server_flags & IP_VS_DEST_F_OVERLOAD) {
			/* if OVERLOAD flag is set - connection can not be created */
			ezdp_dec_single_ctr(cmem_alvs.server_info_result.server_on_demand_stats_base + ALVS_SERVER_STATS_CONNECTION_TOTAL_OFFSET,
					    1);
		}
		alvs_write_log(LOG_DEBUG, " counter %d < u_thresh %d, server_flags = 0x%x", (uint32_t)counter, cmem_alvs.server_info_result.u_thresh, server_flags);
		/* return current server_flag */
		return server_flags;

	} else {
		alvs_write_log(LOG_DEBUG, "alvs_server_overload_on_create_conn");
		alvs_write_log(LOG_DEBUG, "server_index  = %d,  server_flags = 0x%x, u_thresh = %d, l_thresh = %d",
			       server_index, cmem_alvs.server_info_result.server_flags,
			       cmem_alvs.server_info_result.u_thresh,
			       cmem_alvs.server_info_result.l_thresh);
		/* if upper threshold == 0 -> low threshold == 0 -> increase number of connections and return 0 */
		alvs_write_log(LOG_DEBUG, " cmem_alvs.server_info_result.u_thresh = 0");
		return !IP_VS_DEST_F_OVERLOAD;
	}
}

/******************************************************************************
 * \brief       alvs_server_overload_on_delete_conn - update overloaded flag according to
 *              the current connections in the process
 *              of deleting connection. Can be called from delete connection process or during failure of allocation of
 *              index in the creation of the new connection.
 *
 * \return	none
 */
static __always_inline
void alvs_server_overload_on_delete_conn(uint16_t server_index)
{

	uint64_t counter = 0;

	/* this function is called from delete_connection, so OVERLOAD bit can be clear */
	ezdp_read_and_dec_single_ctr(cmem_alvs.server_info_result.server_on_demand_stats_base + ALVS_SERVER_STATS_CONNECTION_TOTAL_OFFSET,
				     1, &cmem_wa.alvs_wa.counter_work_area, 0);

	if (cmem_alvs.server_info_result.u_thresh == 0) {
		alvs_write_log(LOG_DEBUG, "cmem_alvs.server_info_result.u_thresh = l_thresh = 0");
		ezdp_atomic_and32_sum_addr(cmem_alvs.server_info_result.server_flags_dp_base, ~IP_VS_DEST_F_OVERLOAD);
		return;
	}
	/* get sched_conns for the formula for updating over_loaded flag*/
	counter = cmem_wa.alvs_wa.counter_work_area - 1;

	alvs_write_log(LOG_DEBUG, "alvs_server_overload_on_delete_conn");
	alvs_write_log(LOG_DEBUG, "server_index  = %d,  server_flags = 0x%x, u_thresh = %d, l_thresh = %d, sched_conns = %d, overloaded_flags = 0x%x",
		       server_index, cmem_alvs.server_info_result.server_flags,
		       cmem_alvs.server_info_result.u_thresh,
		       cmem_alvs.server_info_result.l_thresh,
		       (uint32_t)counter,
		       ezdp_atomic_read32_sum_addr(cmem_alvs.server_info_result.server_flags_dp_base));

	if (cmem_alvs.server_info_result.l_thresh != 0) {
		/* check if low threshold is not 0 */
		alvs_write_log(LOG_DEBUG, "cmem_alvs.server_info_result.l_thresh != 0");
		if (counter < cmem_alvs.server_info_result.l_thresh) {
			/* if number of connections drops below the low threshold - OVERLOAD flag should be cleared */
			alvs_write_log(LOG_DEBUG, "counter %d < cmem_alvs.server_info_result.l_thresh %d", (uint32_t)counter, cmem_alvs.server_info_result.l_thresh);
			ezdp_atomic_and32_sum_addr(cmem_alvs.server_info_result.server_flags_dp_base, ~IP_VS_DEST_F_OVERLOAD);
		}
	} else if (cmem_alvs.server_info_result.u_thresh != 0) {
		/* check if upper threshold is not 0 */
		alvs_write_log(LOG_DEBUG, "cmem_alvs.server_info_result.u_thresh != 0");
		if (counter * 4 < cmem_alvs.server_info_result.u_thresh * 3) {
			/* if number of connections of the current drops below  three forth of its upper connection threshold. */
			/* if yes - clear OVERLOAD bit */
			alvs_write_log(LOG_DEBUG, "counter * 4 < cmem_alvs.server_info_result.u_thresh * 3");
			ezdp_atomic_and32_sum_addr(cmem_alvs.server_info_result.server_flags_dp_base, ~IP_VS_DEST_F_OVERLOAD);
		}
	}
}

/******************************************************************************
 * \brief       Try to find the server index from server 5-tuple.
 *              (virtual ip, virtual port, server ip, server port, protocol)
 *
 * \return	true if server exists (index stored in server_index),
 *              false if server not exists.
 */
static __always_inline
bool alvs_find_server_index(in_addr_t server_ip, in_addr_t virtual_ip,
			    uint16_t server_port, uint16_t virtual_port,
			    uint16_t protocol, uint32_t *server_index)
{
	uint32_t rc;
	uint32_t found_result_size;
	struct alvs_server_classification_result *server_class_res_ptr;

	cmem_alvs.server_class_key.virtual_ip = virtual_ip;
	cmem_alvs.server_class_key.virtual_port = virtual_port;
	cmem_alvs.server_class_key.server_ip = server_ip;
	cmem_alvs.server_class_key.server_port = server_port;
	cmem_alvs.server_class_key.protocol = protocol;

	alvs_write_log(LOG_DEBUG, "Trying to find server (0x%x:%d) in service (0x%x:%d, protocol=%d)...",
		       cmem_alvs.server_class_key.server_ip,
		       cmem_alvs.server_class_key.server_port,
		       cmem_alvs.server_class_key.virtual_ip,
		       cmem_alvs.server_class_key.virtual_port,
		       cmem_alvs.server_class_key.protocol);

	rc = ezdp_lookup_hash_entry(&shared_cmem_alvs.server_class_struct_desc,
				    (void *)&cmem_alvs.server_class_key,
				    sizeof(struct alvs_server_classification_key),
				    (void **)&server_class_res_ptr,
				    &found_result_size, 0,
				    cmem_wa.alvs_wa.server_hash_wa,
				    sizeof(cmem_wa.alvs_wa.server_hash_wa));

	if (rc != 0) {
		/* Server not found */
		alvs_write_log(LOG_DEBUG, "Server not found");
		return false;
	}

	*server_index = server_class_res_ptr->server_index;
	alvs_write_log(LOG_DEBUG, "Server found in index %d.", *server_index);
	return true;
}

#endif  /*ALVS_SERVER_H_*/
