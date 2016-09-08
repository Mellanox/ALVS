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
*  File:                alvs_service.h
*  Desc:                handle service functionality
*/

#ifndef ALVS_SERVICE_H_
#define ALVS_SERVICE_H_

#include "alvs_sched.h"
#include "alvs_conn.h"

/******************************************************************************
 * \brief       lookup in service info table according to a given index
 *
 * \return      return 0 in case of success, otherwise no match.
 */
static __always_inline
uint32_t alvs_service_info_lookup(uint8_t service_index)
{
	return ezdp_lookup_table_entry(&shared_cmem_alvs.service_info_struct_desc,
				service_index, &cmem_alvs.service_info_result,
				sizeof(struct alvs_service_info_result), 0);
}


/******************************************************************************
 * \brief       when opening a new connection entry we first need to find the
 *              destination server of the new entry according to a given schedule
 *              algorithm configured in the service to which this connection belongs.
 *              the function first locks the connection (using DP spinlock)
 *
 *              to prevent any race conditions or cases were 2 threads are trying to create the
 *              same connection we use a DP spinlock and busy wait of all threads which are locked.
 *              locked threads will wait until all scheduling process is done and then will continue to
 *              regular connection data path.
 *
 * \return        return alvs_service_output_result:
 *                      ALVS_SERVICE_DATA_PATH_IGNORE - frame was sent to host or drop
 *                      ALVS_SERVICE_DATA_PATH_RETRY - retry to transmit frame via regular connection data path
 *                      ALVS_SERVICE_DATA_PATH_SUCCESS - a new connection entry was created.
 */
static __always_inline
enum alvs_service_output_result alvs_tcp_schedule_new_connection(uint8_t service_index,
								 struct iphdr *ip_hdr,
								 struct tcphdr *tcp_hdr)
{
	uint32_t rc;
	enum alvs_service_output_result result;
	uint32_t found_result_size;
	ezdp_hashed_key_t hash_value;
	struct  alvs_conn_classification_result *conn_class_res_ptr;

	/*check if there are active servers for service*/
	if (unlikely(alvs_sched_check_active_servers() == false)) {
		result = ALVS_SERVICE_DATA_PATH_IGNORE;
		goto out;
	}

	/*take connection lock*/
	alvs_lock_connection(&hash_value);

	/*check if connection was already created*/
	rc = ezdp_lookup_hash_entry(&shared_cmem_alvs.conn_class_struct_desc,
				    (void *)&cmem_alvs.conn_class_key,
				    sizeof(struct alvs_conn_classification_key),
				    (void **)&conn_class_res_ptr,
				    &found_result_size, 0,
				    cmem_wa.alvs_wa.conn_hash_wa,
				    sizeof(cmem_wa.alvs_wa.conn_hash_wa));
	if (rc == 0) {
		cmem_alvs.conn_result.conn_index = conn_class_res_ptr->conn_index;
		alvs_write_log(LOG_DEBUG, "new connection was already created!!!! conn_index = %d", cmem_alvs.conn_result.conn_index);
		result = ALVS_SERVICE_DATA_PATH_RETRY;
		goto unlock;
	}
	if (rc == EIO)	{
		alvs_discard_and_stats(ALVS_ERROR_CREATE_CONN_MEM_ERROR);
		alvs_write_log(LOG_CRIT, "try to create new connection --> memory error!");
		result = ALVS_SERVICE_DATA_PATH_IGNORE;
		goto unlock;
	}

	/*schedule connection*/
	if (likely(cmem_alvs.service_info_result.sched_alg == ALVS_SOURCE_HASH_SCHEDULER)) {
		if (alvs_sched_sh_schedule_connection(service_index, ip_hdr->saddr, tcp_hdr->source) == false) {
			result = ALVS_SERVICE_DATA_PATH_IGNORE;
			goto unlock;
		}
	} else if (likely(cmem_alvs.service_info_result.sched_alg == ALVS_ROUND_ROBIN_SCHEDULER)) {
		if (alvs_sched_rr_schedule_connection(service_index) == false) {
			result = ALVS_SERVICE_DATA_PATH_IGNORE;
			goto unlock;
		}
	} else if (likely(cmem_alvs.service_info_result.sched_alg == ALVS_WEIGHTED_ROUND_ROBIN_SCHEDULER)) {
		if (alvs_sched_rr_schedule_connection(service_index) == false) {
			result = ALVS_SERVICE_DATA_PATH_IGNORE;
			goto unlock;
		}
	} else {
		alvs_write_log(LOG_ERR, "unsupported scheduling algorithm");
		/*drop frame*/
		alvs_discard_and_stats(ALVS_ERROR_UNSUPPORTED_SCHED_ALGO);
		result = ALVS_SERVICE_DATA_PATH_IGNORE;
		goto unlock;
	}

	result = alvs_conn_create_new_entry(true,
					    cmem_alvs.sched_info_result.server_index,
					    0,
					    ((tcp_hdr->fin || tcp_hdr->rst) ? IP_VS_TCP_S_CLOSE_WAIT : IP_VS_TCP_S_ESTABLISHED),
					    cmem_alvs.server_info_result.conn_flags,
					    tcp_hdr->rst ? 1 : 0);

	/*mark connection for state sync*/
	if (likely(result == ALVS_SERVICE_DATA_PATH_SUCCESS)) {
		alvs_write_log(LOG_DEBUG, "New connection created and marked for state sync (conn_index = %d)", cmem_alvs.conn_result.conn_index);
		cmem_alvs.conn_sync_state.conn_sync_status = ALVS_CONN_SYNC_NEED;
	}

unlock:
	alvs_unlock_connection(hash_value);
	alvs_write_log(LOG_DEBUG, "alvs_unlock_connection");
out:
	return result;
}


/******************************************************************************
 * \brief       perform service info lookup and try to create new connection entry
 *              according to service protocol and scheduling algo.
 *
 * \return      return alvs_service_output_result:
 *                      ALVS_SERVICE_DATA_PATH_IGNORE - frame was dropped or send to host
 *                      ALVS_SERVICE_DATA_PATH_RETRY - retry to transmit frame via regular connection data path
 *                      ALVS_SERVICE_DATA_PATH_SUCCESS - a new connection entry was created.
 */
static __always_inline
enum alvs_service_output_result alvs_service_data_path(uint8_t service_index,
						       struct iphdr *ip_hdr,
						       struct tcphdr *tcp_hdr)
{
	uint32_t rc;

	/*perform lookup in service info DB*/
	rc = alvs_service_info_lookup(service_index);

	 alvs_write_log(LOG_DEBUG, "(slow path) (ip->dest = 0x%x tcp->dest = %d proto=%d) service_idx = %d",
			ip_hdr->daddr,
			tcp_hdr->dest,
			ip_hdr->protocol,
			service_index);
	if (likely(rc == 0)) {
		if (ip_hdr->protocol == IPPROTO_TCP) {
			return alvs_tcp_schedule_new_connection(service_index, ip_hdr, tcp_hdr);
		}
		/*drop frame - UDP is not supported*/
		alvs_discard_and_stats(ALVS_ERROR_UNSUPPORTED_PROTOCOL);
	} else {
		/*drop frame*/
		alvs_discard_and_stats(ALVS_ERROR_SERVICE_INFO_LOOKUP);
	}
	return ALVS_SERVICE_DATA_PATH_IGNORE;
}

#endif /*ALVS_SERVICE_H_*/
