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
*  File:                alvs_utils.h
*  Desc:                various alvs helper functions
*/

#ifndef ALVS_UTILS_H_
#define ALVS_UTILS_H_

#include "defs.h"
#include "nw_utils.h"
#include "alvs_search_defs.h"

/******************************************************************************
 * \brief         get the amount of aging iterations for alvs connection state.
 *
 * \return        amount of aging iterations
 */
static __always_inline
int alvs_util_get_conn_iterations(enum alvs_tcp_conn_state alvs_state)
{
	switch (alvs_state) {
	case IP_VS_TCP_S_ESTABLISHED:
		return ALVS_TCP_CONN_ITER_ESTABLISHED;
	case IP_VS_TCP_S_CLOSE_WAIT:
		return ALVS_TCP_CONN_ITER_CLOSE_WAIT;
	default:
		/*should not happen*/
		return 1;
	}
}

/******************************************************************************
 * \brief       perform alvs application info lookup.
 *
 * \return      lookup result
 *
 */
static __always_inline
int alvs_util_app_info_lookup(void)
{
	return nw_app_info_lookup(ALVS_APPLICATION_INFO_INDEX, &cmem_wa.alvs_wa.alvs_app_info_result,
					sizeof(struct alvs_app_info_result));
}

/******************************************************************************
 * \brief         update in pkts/in bytes statistics for: server, service
 * \return        void
 */
static __always_inline
void alvs_update_incoming_traffic_stats(void)
{
	/*handle service stats:
	 *                    in pkts/in bytes
	 *                    out pkts/out bytes
	 *                    conn sched/refcnt (slow path)
	 */
	ezdp_dual_add_posted_ctr(cmem_alvs.server_info_result.service_stats_base + ALVS_SERVICE_STATS_IN_PKTS_BYTES_OFFSET,
				 1,
				 frame.job_desc.frame_desc.frame_length);

	/*handle server stats:
	 *                   in pkts/in bytes
	 *                   out pkts/out bytes
	 *                   conn sched/refcnt (slow path)
	 *                   inactive conn/active conn (aging path/slow path)
	 */
	ezdp_dual_add_posted_ctr(cmem_alvs.server_info_result.server_stats_base + ALVS_SERVER_STATS_IN_PKTS_BYTES_OFFSET,
				 1,
				 frame.job_desc.frame_desc.frame_length);
}

/******************************************************************************
 * \brief         update traffic counters in server & service - used when
 *                creating new connection or deletion of an existing one.
 * \return        void
 */
static __always_inline
void alvs_update_connection_statistics(int32_t sched_conn, int32_t active_conn, int32_t inactive_conn)
{
	/*handle service stats:
	 *                    in pkts/in bytes
	 *                    out pkts/out bytes
	 *                    conn sched/refcnt (slow path)
	 */
	ezdp_add_posted_ctr(cmem_alvs.server_info_result.service_stats_base + ALVS_SERVICE_STATS_CONN_SCHED_OFFSET,
			    sched_conn);
	/*handle server stats:
	 *                   in pkts/in bytes
	 *                   out pkts/out bytes
	 *                   conn sched/refcnt (slow path)
	 *                   inactive conn/active conn (aging path/slow path)
	 */
	ezdp_add_posted_ctr(cmem_alvs.server_info_result.server_stats_base + ALVS_SERVER_STATS_ACTIVE_CONN_OFFSET,
			    active_conn);

	ezdp_add_posted_ctr(cmem_alvs.server_info_result.server_stats_base + ALVS_SERVER_STATS_INACTIVE_CONN_OFFSET,
			    inactive_conn);

	ezdp_add_posted_ctr(cmem_alvs.server_info_result.server_stats_base + ALVS_SERVER_STATS_CONN_SCHED_OFFSET,
			    sched_conn);
}

/******************************************************************************
 * \brief         update alvs error counters
 * \return        void
 */
static __always_inline
void alvs_update_discard_statistics(enum alvs_error_stats_offsets error_id)
{
	ezdp_sum_addr_t addr = BUILD_SUM_ADDR(EZDP_EXTERNAL_MS,
					      EMEM_ALVS_ERROR_STATS_POSTED_MSID,
					      EMEM_ALVS_ERROR_STATS_POSTED_OFFSET + error_id);

	ezdp_add_posted_ctr(addr, 1);
}

/******************************************************************************
 * \brief         discard frame due to unexpected error. frame can not be sent to host.
 * \return        void
 */
static __always_inline
void alvs_discard_frame(void)
{
	/*drop frame*/
	ezframe_free(&frame, 0);
}

/******************************************************************************
 * \brief         discard frame and update alvs error counters.
 * \return        void
 */
static __always_inline
void alvs_discard_and_stats(enum alvs_error_stats_offsets error_id)
{
	alvs_update_discard_statistics(error_id);
	alvs_discard_frame();
}

/******************************************************************************
 * \brief         perform a lock on connection ( 5 tuple) using DP spinlock
 *
 * \return        0 - success
 */
static __always_inline
void alvs_lock_connection(ezdp_hashed_key_t *hash_value)
{
	/*calc hash key for lock*/
	*hash_value = ezdp_bulk_hash((uint8_t *)&cmem_alvs.conn_class_key, sizeof(struct alvs_conn_classification_key));
	cmem_alvs.conn_spinlock.addr.address = *hash_value & ALVS_CONN_LOCK_ELEMENTS_MASK;
	anl_write_log(LOG_DEBUG, "cmem_alvs.conn_spinlock.addr.address = 0x%x", cmem_alvs.conn_spinlock.addr.address);
	ezdp_lock_spinlock(&cmem_alvs.conn_spinlock);
}

/******************************************************************************
 * \brief      perform a try lock on connection ( 5 tuple) using DP spinlock
 *
 * \return     0 - success
 *             1 - failure
 */
static __always_inline
uint32_t alvs_try_lock_connection(ezdp_hashed_key_t *hash_value)
{
	/*calc hash key for lock*/
	*hash_value = ezdp_bulk_hash((uint8_t *)&cmem_alvs.conn_class_key, sizeof(struct alvs_conn_classification_key));
	cmem_alvs.conn_spinlock.addr.address = *hash_value & ALVS_CONN_LOCK_ELEMENTS_MASK;
	return ezdp_try_lock_spinlock(&cmem_alvs.conn_spinlock);
}

/******************************************************************************
 * \brief      unlock connection ( 5 tuple) using DP spinlock
 *
 * \return     0 - success
 *             2 - try to unlock twice
 */
static __always_inline
uint32_t alvs_unlock_connection(ezdp_hashed_key_t hash_value)
{
	cmem_alvs.conn_spinlock.addr.address = hash_value & ALVS_CONN_LOCK_ELEMENTS_MASK;
	return ezdp_unlock_spinlock(&cmem_alvs.conn_spinlock);
}

#endif  /*ALVS_UTILS_H_*/
