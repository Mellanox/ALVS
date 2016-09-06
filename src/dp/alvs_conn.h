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
*  File:                alvs_conn.h
*  Desc:                handle connection functionality
*/

#ifndef ALVS_CONN_H_
#define ALVS_CONN_H_

#include "defs.h"
#include "alvs_server.h"
#include "alvs_utils.h"
#include "alvs_state_sync_master.h"
#include "nw_routing.h"

/******************************************************************************
 * \brief         perform lookup on connection info DB
 *
 * \return        0=lkp success, otherwise fail
 */
static __always_inline
uint32_t alvs_conn_info_lookup(uint32_t conn_index)
{
	/*get index from entry and perform lookup in conn info DB*/
	return ezdp_lookup_table_entry(&shared_cmem_alvs.conn_info_struct_desc,
				       conn_index, &cmem_alvs.conn_info_result,
				       sizeof(struct alvs_conn_info_result), 0);
}

/******************************************************************************
 * \brief       create a new entry in connection info and connection classification
 *              DBs. the connection lock should be taken before running this function.
 *              new index from connection index pool is being allocated. this index is used
 *              as the key to the connection info DB and as the result to the connection
 *              info DB.
 *              this function is called only after all scheduling process is done so all
 *              info for entry is taken from the service and server data.
 *              the connection classification key is built from the service classification key
 *              plus other fields taken from the frame itself.

 *
 * \return      return alvs_service_output_result:
 *                      ALVS_SERVICE_DATA_PATH_IGNORE - frame was sent to host or dropped
 *                      ALVS_SERVICE_DATA_PATH_RETRY - retry to transmit frame via regular connection data path
 *                      ALVS_SERVICE_DATA_PATH_SUCCESS - a new connection entry was created.
 *
 */
static __always_inline
enum alvs_service_output_result alvs_conn_create_new_entry(bool bound, uint32_t server, uint16_t port,
							   enum alvs_tcp_conn_state conn_state,
							   uint32_t flags, bool reset)
{
	uint32_t conn_index;

	/*allocate new index*/
	conn_index = ezdp_alloc_index(ALVS_CONN_INDEX_POOL_ID);
	if (conn_index == EZDP_NULL_INDEX) {
		alvs_write_log(LOG_CRIT, "error alloc index from pool server_index = %d, free indexes = %d", server, ezdp_read_free_indexes(ALVS_CONN_INDEX_POOL_ID));
		alvs_server_overload_on_delete_conn(server);

		/*drop frame*/
		alvs_discard_and_stats(ALVS_ERROR_CONN_INDEX_ALLOC_FAIL);
		return ALVS_SERVICE_DATA_PATH_IGNORE;
	}
	alvs_write_log(LOG_DEBUG, "Index %d allocated for connection", conn_index);

	cmem_alvs.conn_info_result.aging_bit = 1;
	cmem_alvs.conn_info_result.bound = bound;
	cmem_alvs.conn_info_result.reset_bit = reset;
	cmem_alvs.conn_info_result.delete_bit = reset;
	cmem_alvs.conn_info_result.conn_flags = flags;
	cmem_alvs.conn_info_result.server_index = server;
	cmem_alvs.conn_info_result.server_port = port;
	cmem_alvs.conn_info_result.conn_state = conn_state;
	cmem_alvs.conn_info_result.age_iteration = 0;
	ezdp_mem_copy(&cmem_alvs.conn_info_result.conn_class_key, &cmem_alvs.conn_class_key, sizeof(struct alvs_conn_classification_key));

	if (conn_state == IP_VS_TCP_S_ESTABLISHED) {
		cmem_alvs.conn_info_result.conn_flags &= ~IP_VS_CONN_F_INACTIVE;
		if (bound) {
			alvs_update_connection_statistics(1, 1, 0);
		}
	} else {
		cmem_alvs.conn_info_result.conn_flags |= IP_VS_CONN_F_INACTIVE;
		if (bound) {
			alvs_update_connection_statistics(1, 0, 1);
		}
	}

	/*first create connection info entry*/
	(void)ezdp_add_table_entry(&shared_cmem_alvs.conn_info_struct_desc,
			     conn_index,
			     &cmem_alvs.conn_info_result,
			     sizeof(struct alvs_conn_info_result),
			     EZDP_UNCONDITIONAL,
			     cmem_wa.alvs_wa.conn_info_table_wa,
			     sizeof(cmem_wa.alvs_wa.conn_info_table_wa));

	cmem_alvs.conn_result.conn_index = conn_index;

	(void)ezdp_add_hash_entry(&shared_cmem_alvs.conn_class_struct_desc,
				 &cmem_alvs.conn_class_key,
				 sizeof(struct alvs_conn_classification_key),
				 &cmem_alvs.conn_result,
				 sizeof(struct alvs_conn_classification_result),
				 EZDP_UNCONDITIONAL,
				 cmem_wa.alvs_wa.conn_hash_wa,
				 sizeof(cmem_wa.alvs_wa.conn_hash_wa));

	if (bound) {
		alvs_write_log(LOG_DEBUG, "Bounded connection created (conn_idx = %d server_idx = %d) successfully", conn_index, server);
	} else {
		alvs_write_log(LOG_DEBUG, "Unbounded connection created (conn_idx = %d server_addr = 0x%08x:%d) successfully", conn_index, server, port);
	}

	return ALVS_SERVICE_DATA_PATH_SUCCESS;
}

/******************************************************************************
 * \brief       update the connection entry state. currently only support 2
 *              states for connection:
 *                      ESTABLISHED
 *                      CLOSE WAIT
 *
 * \return      0 = modify success, otherwise fail
 */
static __always_inline
uint32_t alvs_conn_update_state(uint32_t conn_index, enum alvs_tcp_conn_state new_state)
{
	uint32_t rc;
	ezdp_hashed_key_t hash_value;

	/*lock connection*/
	alvs_lock_connection(&hash_value);

	/*perform another lookup to prevent race conditions*/
	rc = alvs_conn_info_lookup(conn_index);

	if (rc != 0) {
		alvs_write_log(LOG_DEBUG, "conn_idx = %d info lookup alvs_conn_update_state FAIL", conn_index);
		alvs_unlock_connection(hash_value);
		return rc;
	}

	alvs_update_connection_statistics(0, -1, 1);

	cmem_alvs.conn_info_result.delete_bit = 0;
	cmem_alvs.conn_info_result.aging_bit = 1;
	cmem_alvs.conn_info_result.conn_state = new_state;
	cmem_alvs.conn_info_result.conn_flags |= IP_VS_CONN_F_INACTIVE;

	rc = ezdp_modify_table_entry(&shared_cmem_alvs.conn_info_struct_desc,
			conn_index,
			&cmem_alvs.conn_info_result,
			sizeof(struct alvs_conn_info_result),
			EZDP_UNCONDITIONAL,
			cmem_wa.alvs_wa.conn_info_table_wa,
			sizeof(cmem_wa.alvs_wa.conn_info_table_wa));

	/*mark connection for state sync*/
	cmem_alvs.conn_sync_state.conn_sync_status = ALVS_CONN_SYNC_NEED;
	alvs_write_log(LOG_DEBUG, "Connection state updated (conn_idx = %d state = %d) successfully and marked for state sync", conn_index, new_state);

	/*unlock connection*/
	alvs_unlock_connection(hash_value);

	return rc;
}

/******************************************************************************
 * \brief       set connection delete bit on
 *              this function is called in 2 cases:
 *                      2. Receive TCP reset flag
 *                      3. destination server is not available
 *
 * \return      void
 */
static __always_inline
uint32_t alvs_conn_mark_to_delete(uint32_t conn_index, uint8_t reset)
{
	uint32_t rc;
	ezdp_hashed_key_t hash_value;

	/*lock connection*/
	alvs_lock_connection(&hash_value);

	/*perform another lookup to prevent race conditions*/
	rc = alvs_conn_info_lookup(conn_index);

	if (rc != 0) {
		alvs_write_log(LOG_DEBUG, "fail in conn_idx = %d conn_info lookup alvs_conn_mark_to_delete", conn_index);
		alvs_unlock_connection(hash_value);
		return rc;
	}

	/* need to modify connection entry - change state for close and set aging bit on. */
	cmem_alvs.conn_info_result.aging_bit = 0;
	cmem_alvs.conn_info_result.delete_bit = 1;
	cmem_alvs.conn_info_result.reset_bit = reset;
	if (reset && cmem_alvs.conn_info_result.conn_state == IP_VS_TCP_S_ESTABLISHED) {
		cmem_alvs.conn_info_result.conn_state = IP_VS_TCP_S_CLOSE_WAIT;
		alvs_update_connection_statistics(0, -1, 1);
		cmem_alvs.conn_info_result.conn_flags |= IP_VS_CONN_F_INACTIVE;
	}

	rc = ezdp_modify_table_entry(&shared_cmem_alvs.conn_info_struct_desc,
			conn_index,
			&cmem_alvs.conn_info_result,
			sizeof(struct alvs_conn_info_result),
			EZDP_UNCONDITIONAL,
			cmem_wa.alvs_wa.conn_info_table_wa,
			sizeof(cmem_wa.alvs_wa.conn_info_table_wa));

	/*unlock connection*/
	alvs_unlock_connection(hash_value);
	return rc;
}

/******************************************************************************
 * \brief       remove a connection entry from connection info DB and from
 *              connection classification table. without using alvs_lock_connection
 *              therefore thread must take conn_lock before using this function.
 *
 * \return        void
 */
static __always_inline
void alvs_conn_delete_without_lock(uint32_t conn_index)
{

	if (alvs_server_info_lookup(cmem_alvs.conn_info_result.server_index) == 0) {
		if (cmem_alvs.conn_info_result.conn_state == IP_VS_TCP_S_ESTABLISHED) {
			alvs_update_connection_statistics(-1, -1, 0);
		} else {
			alvs_update_connection_statistics(-1, 0, -1);
		}
		alvs_server_overload_on_delete_conn(cmem_alvs.conn_info_result.server_index);
	}

	/*1st remove the classification entry*/
	if (ezdp_delete_hash_entry(&shared_cmem_alvs.conn_class_struct_desc,
			       &cmem_alvs.conn_info_result.conn_class_key,
			       sizeof(struct alvs_conn_classification_key),
			       0,
			       cmem_wa.alvs_wa.conn_hash_wa,
			       sizeof(cmem_wa.alvs_wa.conn_hash_wa)) != 0) {
		alvs_write_log(LOG_DEBUG, "unable to delete conn_class_key conn_idx = %d alvs_conn_delete", conn_index);
		return;
	}

	/*now remove the connection info entry*/
	if (ezdp_delete_table_entry(&shared_cmem_alvs.conn_info_struct_desc,
				conn_index,
				0,
				cmem_wa.alvs_wa.conn_info_table_wa,
				sizeof(cmem_wa.alvs_wa.conn_info_table_wa)) != 0) {
		alvs_write_log(LOG_DEBUG, "unable to delete conn_info conn_idx = %d alvs_conn_delete", conn_index);
		return;
	}

	ezdp_free_index(ALVS_CONN_INDEX_POOL_ID, conn_index);
}

/******************************************************************************
 * \brief       remove a connection entry from connection info DB and from
 *              connection classification table. this function is called from
 *              from aging mechanism only.
 *              before removing a connection the function will try to take conn_lock
 *              and then to use alvs_conn_delete_without_lock function
 *
 * \return        void
 */
static __always_inline
void alvs_conn_delete(uint32_t conn_index)
{
	ezdp_hashed_key_t hash_value;

	/*lock connection*/
	alvs_lock_connection(&hash_value);

	alvs_conn_delete_without_lock(conn_index);

	/*unlock*/
	alvs_unlock_connection(hash_value);
}

/******************************************************************************
 * \brief       set connection entry aging bit back to 1 after whenever it is set to 0 and a
 *              a new frame arrives to the NPS which belongs to this connection.
 *
 * \return      0 in case of success, otherwise failure.
 */
static __always_inline
uint32_t alvs_conn_refresh(uint32_t conn_index)
{
	uint32_t rc;
	ezdp_hashed_key_t hash_value;

	/*lock connection*/
	alvs_lock_connection(&hash_value);

	/*perform another lookup to prevent race conditions*/
	rc = alvs_conn_info_lookup(conn_index);

	if (rc != 0) {
		alvs_write_log(LOG_DEBUG, "fail in conn_idx = %d conn_info lookup alvs_conn_refresh", conn_index);
		alvs_unlock_connection(hash_value);
		return rc;
	}

	/* turn on the aging bit */
	cmem_alvs.conn_info_result.aging_bit = 1;
	cmem_alvs.conn_info_result.delete_bit = 0;

	rc =  ezdp_modify_table_entry(&shared_cmem_alvs.conn_info_struct_desc,
			conn_index,
			&cmem_alvs.conn_info_result,
			sizeof(struct alvs_conn_info_result),
			EZDP_UNCONDITIONAL,
			cmem_wa.alvs_wa.conn_info_table_wa,
			sizeof(cmem_wa.alvs_wa.conn_info_table_wa));

	/*unlock*/
	alvs_unlock_connection(hash_value);
	return rc;
}

/******************************************************************************
 * \brief       set connection entry aging bit to 0. this function is called only
 *              from aging mechanism.
 *
 * \return      0 in case of success, otherwise failure.
 */
static __always_inline
uint32_t alvs_conn_age_out(uint32_t conn_index, uint8_t age_iteration)
{
	uint32_t rc;
	ezdp_hashed_key_t hash_value;

	/*lock connection*/
	alvs_lock_connection(&hash_value);

	/*perform another lookup to prevent race conditions*/
	rc = alvs_conn_info_lookup(conn_index);

	if (rc != 0) {
		alvs_write_log(LOG_DEBUG, "fail in conn_idx = %d conn_info lookup alvs_conn_age_out", conn_index);
		alvs_unlock_connection(hash_value);
		return rc;
	}

	/* turn off the aging bit */
	cmem_alvs.conn_info_result.aging_bit = 0;
	cmem_alvs.conn_info_result.age_iteration = age_iteration;

	rc = ezdp_modify_table_entry(&shared_cmem_alvs.conn_info_struct_desc,
			conn_index,
			&cmem_alvs.conn_info_result,
			sizeof(struct alvs_conn_info_result),
			EZDP_UNCONDITIONAL,
			cmem_wa.alvs_wa.conn_info_table_wa,
			sizeof(cmem_wa.alvs_wa.conn_info_table_wa));
	/*unlock*/
	alvs_unlock_connection(hash_value);
	return rc;
}

/******************************************************************************
 * \brief       perform routing of incoming packet that belongs to an existing
 *              connection/new connection according to the configured routing
 *              algorithm of the server. currently support only direct routing.
 *
 * \return      void
 */
static __always_inline
void alvs_conn_do_route(uint8_t *frame_base)
{
	/*transmit packet according to routing method*/
	if (likely((cmem_alvs.server_info_result.conn_flags & IP_VS_CONN_F_FWD_MASK) == IP_VS_CONN_F_DROUTE)) {
		if (cmem_alvs.conn_info_result.bound == true) {
			nw_do_route(&frame,
				    frame_base,
				    cmem_alvs.server_info_result.server_ip,
				    ezframe_get_buf_len(&frame));
		} else {
			nw_do_route(&frame,
				    frame_base,
				    cmem_alvs.conn_info_result.server_addr,
				    ezframe_get_buf_len(&frame));
		}
	} else {
		alvs_write_log(LOG_ERR, "got unsupported routing algo = %d alvs_conn_do_route", cmem_alvs.server_info_result.conn_flags & IP_VS_CONN_F_FWD_MASK);
		/*drop frame*/
		alvs_discard_and_stats(ALVS_ERROR_UNSUPPORTED_ROUTING_ALGO);
		return;
	}

	/*update statistics*/
	alvs_update_incoming_traffic_stats();

	/*send connection state sync*/
	if (unlikely(cmem_alvs.conn_sync_state.conn_sync_status == ALVS_CONN_SYNC_NEED)) {
		/*check application info if state sync is active*/
		if (unlikely(alvs_util_app_info_lookup() == 0 && cmem_wa.alvs_wa.alvs_app_info_result.master_bit)) {
			alvs_state_sync_send_single(cmem_wa.alvs_wa.alvs_app_info_result.source_ip,
						    cmem_wa.alvs_wa.alvs_app_info_result.m_sync_id);
		}
	}
}

/******************************************************************************
 * \brief       incoming packet has passed the connection classification. after
 *              conn classification the packet goes through fast path of the application:
 *              1. get connection info
 *              2. get server info
 *              3. check conn state & update
 *              4. check server state
 *              5. perform routing
 *              in case of failure in any of the stages the packet will be either dropped
 *              or sent to further processing by the host.
 *
 * \return        void
 */
static __always_inline
void alvs_conn_data_path(uint8_t *frame_base, struct iphdr *ip_hdr, struct tcphdr *tcp_hdr, uint32_t conn_index)
{
	uint32_t rc;

	alvs_write_log(LOG_DEBUG, "conn_idx  = %d exists (fast path)", conn_index);

	/*perform lookup in conn info DB*/
	rc = alvs_conn_info_lookup(conn_index);

	if (likely(rc == 0)) {
		if (cmem_alvs.conn_info_result.bound == false) {
			alvs_server_try_bind(cmem_alvs.conn_info_result.server_addr, cmem_alvs.conn_class_key.virtual_ip,
					     cmem_alvs.conn_info_result.server_port, cmem_alvs.conn_class_key.virtual_port,
					     cmem_alvs.conn_class_key.protocol);
		}

		/*check if someone already indicated that this connection should be deleted...*/
		if (cmem_alvs.conn_info_result.reset_bit == 1) {
			/*server in not available - close connection*/
			alvs_write_log(LOG_DEBUG, "conn_idx  = %d reset_bit = 1 already", conn_index);
			/*drop frame*/
			alvs_discard_and_stats(ALVS_ERROR_CONN_MARK_TO_DELETE);
			return;
		}

		if (cmem_alvs.conn_info_result.bound == true) {
			/*get destination server info*/
			if (alvs_server_info_lookup(cmem_alvs.conn_info_result.server_index) != 0) {
				/*no server info - weird error scenario*/
				alvs_write_log(LOG_DEBUG, "server_info_Result  lookup conn_idx  = %d, server_idx = %d FAILED ", conn_index, cmem_alvs.conn_info_result.server_index);
				/*drop frame*/
				alvs_discard_and_stats(ALVS_ERROR_SERVER_INFO_LKUP_FAIL);
				return;
			}

			alvs_write_log(LOG_DEBUG, "conn_idx  = %d, server_idx = %d FOUND", conn_index, cmem_alvs.conn_info_result.server_index);

			/*check destination server status*/
			if (!(cmem_alvs.server_info_result.server_flags & IP_VS_DEST_F_AVAILABLE)) {
				/*server in not available - close connection*/
				alvs_write_log(LOG_DEBUG, "conn_idx  = %d, server_idx = %d is unavailable ", conn_index, cmem_alvs.conn_info_result.server_index);
				if (alvs_conn_mark_to_delete(conn_index, 0) != 0) {
					/*unable to update connection - weird error scenario*/
					alvs_write_log(LOG_DEBUG, "conn_idx  = %d, server_idx = %d alvs_conn_mark_to_delete FAILED ", conn_index, cmem_alvs.conn_info_result.server_index);
					/*drop frame*/
					alvs_discard_and_stats(ALVS_ERROR_CANT_MARK_DELETE);
					return;
				}
				/*silent drop the packet and continue*/
				alvs_discard_and_stats(ALVS_ERROR_DEST_SERVER_IS_NOT_AVAIL);
				return;
			}
		}

		/*check if state changed*/
		if (tcp_hdr->rst) {
			alvs_write_log(LOG_DEBUG, "conn_idx  = %d, got RST = 1 go conn_mark_to_delete", conn_index);
			if (alvs_conn_mark_to_delete(conn_index, 1) != 0) {
				/*unable to update connection - weird error scenario*/
				alvs_write_log(LOG_DEBUG, "conn_idx  = %d, conn_mark_to_delete FAILED ", conn_index);
				/*drop frame*/
				alvs_discard_and_stats(ALVS_ERROR_CANT_MARK_DELETE);
				return;
			}
		} else {
			if (cmem_alvs.conn_info_result.conn_state == IP_VS_TCP_S_ESTABLISHED && tcp_hdr->fin) {
				alvs_write_log(LOG_DEBUG, "conn_idx  = %d,  IP_VS_TCP_S_ESTABLISHED got FIN = 1 ", conn_index);
				if (alvs_conn_update_state(conn_index, IP_VS_TCP_S_CLOSE_WAIT) != 0) {
					alvs_write_log(LOG_DEBUG, "conn_idx  = %d, update connection state to close FAIL", conn_index);
					/*drop frame*/
					alvs_discard_and_stats(ALVS_ERROR_CANT_UPDATE_CONNECTION_STATE);
					return;
				}
			} else if (cmem_alvs.conn_info_result.aging_bit == 0) { /*check if we need to update the aging bit*/
				/*set connection aging bit back to 1*/
				alvs_write_log(LOG_DEBUG, "conn_idx  = %d,  Refreshing aging bit", conn_index);
				if (alvs_conn_refresh(conn_index) != 0) {
					alvs_write_log(LOG_DEBUG, "conn_idx  = %d,  Refreshing aging bit FAILED", conn_index);
					/*drop frame*/
					alvs_discard_and_stats(ALVS_ERROR_CANT_UPDATE_CONNECTION_STATE);
					return;
				}
			}
		}

		alvs_conn_do_route(frame_base);
	} else {
		/*no classification info - weird error scenario*/
		alvs_write_log(LOG_ERR, "conn_idx  = %d,  fail lookup to connection info DB ", conn_index);
		/*drop frame*/
		alvs_discard_and_stats(ALVS_ERROR_CONN_INFO_LKUP_FAIL);
		return;
	}
}

#endif /* ALVS_CONN_H_ */
