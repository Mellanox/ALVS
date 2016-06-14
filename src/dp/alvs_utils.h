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
#include "alvs_defs.h"

/******************************************************************************
 * \brief         update all statistics for: connection, server, service
 * \return        void
 */
static __always_inline
void alvs_update_all_conn_statistics(void)
{
	/*handle service stats:
	 *                    in pkts/in bytes
	 *                    out pkts/out bytes
	 *                    conn sched/refcnt (slow path)
	 *
	 * in fast path only update the in pkts/in bytes counter
	 */
#if 0
	ezdp_dual_add_posted_ctr(cmem_alvs.server_info_result.service_stats_base,
			1,
			frame.job_desc.frame_desc.frame_length);
#endif

	/*handle server stats:
	 *                   in pkts/in bytes
	 *                   out pkts/out bytes
	 *                   conn sched/refcnt (slow path)
	 *                   inactive conn/active conn (aging path/slow path)
	 *
	 * in fast path only update the in pkts/in bytes counter
	 */
#if 0
	ezdp_dual_add_posted_ctr(cmem_alvs.server_info_result.server_stats_base,
			1,
			frame.job_desc.frame_desc.frame_length);
#endif

	/* connection - count packets and bytes per connection */
#if 0
	ezdp_dual_add_posted_ctr(cmem_alvs.conn_info_result.conn_stats_base.raw_data,
					1,
					frame.job_desc.frame_desc.frame_length);
#endif
}

/******************************************************************************
 * \brief         update interface stat counter - special couters for alvs
 * \return        void
 */
static __always_inline
void alvs_update_incoming_port_stat_counter(uint32_t counter_id)
{
#if 0
	printf("update counter ID = %d\n", counter_id);
	ezdp_add_posted_ctr(cmem_nw.interface_result.nw_stats_base + counter_id, 1);
#endif
}


/******************************************************************************
 * \brief         update connection counters in service & server - use in when
 *                creating new connection or deletion of an existing one.
 * \return        void
 */
static __always_inline
void alvs_update_connection_statistics(int16_t sched_conn, int16_t active_conn, int16_t inactive_conn)
{
	/*handle service stats:
	 *                    in pkts/in bytes
	 *                    out pkts/out bytes
	 *                    conn sched/refcnt (slow path)
	 *
	 * in fast path only update the in pkts/in bytes counter
	 */
#if 0
	ezdp_dual_add_posted_ctr(cmem_alvs.server_info_result.service_stats_base + ALVS_SERVICE_STATS_CONN_SCHED_OFFSET,
				       sched_conn,
				       0);
#endif

	/*handle server stats:
	 *                   in pkts/in bytes
	 *                   out pkts/out bytes
	 *                   conn sched/refcnt (slow path)
	 *                   inactive conn/active conn (aging path/slow path)
	 *
	 * in fast path only update the in pkts/in bytes counter
	 */
#if 0
	ezdp_dual_add_posted_ctr(cmem_alvs.server_info_result.server_stats_base + ALVS_SERVER_STATS_INACTIVE_ACTIVE_CONN_OFFSET,
				       inactive_conn,
				       active_conn);
#endif

#if 0
	ezdp_dual_add_posted_ctr(cmem_alvs.server_info_result.server_stats_base + ALVS_SERVER_STATS_CONN_SCHED_REFCNT_OFFSET,
				       sched_conn,
				       0);
#endif

}

/******************************************************************************
 * \brief         update alvs error counters
 *                creating new connection or deletion of an existing one.
 * \return        void
 */
static __always_inline
void alvs_update_discard_statistics(enum alvs_error_id error_id)
{
	printf("update error counter ID = %d\n",  error_id);
#if 0
	ezdp_add_posted_ctr(cmem_alvs.server_info_result.server_stats_base + error_id, 1);
#endif
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