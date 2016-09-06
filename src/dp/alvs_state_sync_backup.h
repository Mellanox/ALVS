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
*  File:                alvs_state_sync_backup.h
*  Desc:                state sync of alvs - backup functionality
*/

#ifndef ALVS_STATE_SYNC_BACKUP_H_
#define ALVS_STATE_SYNC_BACKUP_H_

#include "alvs_defs.h"
#include "alvs_server.h"
#include "application_search_defs.h"


/******************************************************************************
 * \brief       process one state sync connection message.
 *		perform version, type and protocol checkers, and update
 *		connection info and connection classification DBs accordingly.
 *
 * \return      0 in case of success
 *		-1 Decoding error and frame will be discarded
 *		+1 Decoding error and state sync connection message will be skipped
 *
 */
static __always_inline
int32_t alvs_state_sync_process_conn(struct alvs_state_sync_conn *conn)
{
	uint32_t rc;
	uint32_t flags;
	uint32_t found_result_size;
	uint32_t conn_index;
	uint32_t lookup_res;
	struct alvs_conn_classification_result *conn_class_res_ptr;
	struct alvs_server_classification_result *server_class_res_ptr;
	enum alvs_service_output_result create_entry_res;
	in_addr_t server_addr;
	uint16_t server_port;
	ezdp_hashed_key_t hash_value;
	int32_t final_res;

	/* Sanity check, version should be always 0 */
	if (conn->version != 0) {
		alvs_write_log(LOG_DEBUG, "ERROR - Dropping buffer, Unknown version %d", conn->version);
		return -1;
	}

	if (conn->type != 0) {
		alvs_write_log(LOG_DEBUG, "ERROR - Type is not 0 (Only IPv4 is currently supported)");
		return -1;
	}

	if (conn->protocol != IPPROTO_TCP && conn->protocol != IPPROTO_UDP) {
		alvs_write_log(LOG_DEBUG, "ERROR - Unsupported protocol (%d)", conn->protocol);
		return 1;
	}

	/* Get flags and Mask off un-needed flags */
	flags = conn->flags & IP_VS_CONN_F_BACKUP_MASK;
	flags |= IP_VS_CONN_F_SYNC;

	/* TODO - validate received state */

	/* Lookup connection in hash */
	cmem_alvs.conn_class_key.virtual_ip = conn->virtual_addr;
	cmem_alvs.conn_class_key.virtual_port = conn->virtual_port;
	cmem_alvs.conn_class_key.client_ip = conn->client_addr;
	cmem_alvs.conn_class_key.client_port = conn->client_port;
	cmem_alvs.conn_class_key.protocol = conn->protocol;

	alvs_write_log(LOG_DEBUG, "Connection (0x%x:%d --> 0x%x:%d, protocol=%d)...",
		       cmem_alvs.conn_class_key.client_ip,
		       cmem_alvs.conn_class_key.client_port,
		       cmem_alvs.conn_class_key.virtual_ip,
		       cmem_alvs.conn_class_key.virtual_port,
		       cmem_alvs.conn_class_key.protocol);

	alvs_lock_connection(&hash_value);
	rc = ezdp_lookup_hash_entry(&shared_cmem_alvs.conn_class_struct_desc,
				    (void *)&cmem_alvs.conn_class_key,
				    sizeof(struct alvs_conn_classification_key),
				    (void **)&conn_class_res_ptr,
				    &found_result_size, 0,
				    cmem_wa.alvs_wa.conn_hash_wa,
				    sizeof(cmem_wa.alvs_wa.conn_hash_wa));

	if (rc == 0) {
		/* connection exists */
		conn_index = conn_class_res_ptr->conn_index;
		alvs_write_log(LOG_DEBUG, "conn_idx  = %d exists", conn_class_res_ptr->conn_index);

		/* perform lookup in conn info DB */
		lookup_res = alvs_conn_info_lookup(conn_index);
		if (lookup_res != 0) {
			alvs_write_log(LOG_DEBUG, "conn_idx = %d info lookup alvs_conn_update_state FAIL, ignoring message", conn_index);
			alvs_unlock_connection(hash_value);
			return 1;
		}

		if (cmem_alvs.conn_info_result.bound == true) {
			alvs_write_log(LOG_DEBUG, "Connection is bound");
			lookup_res = alvs_server_info_lookup(cmem_alvs.conn_info_result.server_index);
			if (lookup_res != 0) {
				/*no server info - weird error scenario*/
				alvs_write_log(LOG_DEBUG, "server_info_Result  lookup conn_idx  = %d, server_idx = %d FAILED, ignoring message", conn_index, cmem_alvs.conn_info_result.server_index);
				alvs_unlock_connection(hash_value);
				return 1;
			}
			server_addr = cmem_alvs.server_info_result.server_ip;
			server_port = cmem_alvs.server_info_result.server_port;
		} else {
			alvs_write_log(LOG_DEBUG, "Connection is not bound");
			server_addr = cmem_alvs.conn_info_result.server_addr;
			server_port = cmem_alvs.conn_info_result.server_port;
		}

		if (conn->server_port != server_port ||
			conn->server_addr != server_addr) {
			alvs_write_log(LOG_DEBUG, "Server changed");
			if (!(conn->flags & IP_VS_CONN_F_INACTIVE)) {
				/* If inactive flag is not set expire the current connection and treat the incoming message as a new connection */
				alvs_write_log(LOG_DEBUG, "INACTIVE flag not set, deleting connection = %d (0x%x:%d --> 0x%x:%d, protocol=%d)...",
					       conn_index,
					       cmem_alvs.conn_info_result.conn_class_key.client_ip,
					       cmem_alvs.conn_info_result.conn_class_key.client_port,
					       cmem_alvs.conn_info_result.conn_class_key.virtual_ip,
					       cmem_alvs.conn_info_result.conn_class_key.virtual_port,
					       cmem_alvs.conn_info_result.conn_class_key.protocol);

				/* using alvs_conn_delete_without_lock instead of alvs_conn_delete function since we have already locked connection */
				alvs_conn_delete_without_lock(conn_index);
				rc = 1;  /* need to fallback to new connection */
			} else {
				/* If inactive flag is set we should ignore the message */
				alvs_write_log(LOG_DEBUG, "INACTIVE flag set, ignoring message");
				alvs_unlock_connection(hash_value);
				return 0;
			}
		}
	}

	if (rc == 0) {
		if (((cmem_alvs.conn_info_result.conn_flags ^ flags) & IP_VS_CONN_F_INACTIVE) &&
			(cmem_alvs.conn_info_result.bound == true)) {
			/* Update server statistics */
			if (flags & IP_VS_CONN_F_INACTIVE) {
				alvs_write_log(LOG_DEBUG, "INACTIVE flag is now set. Updating server statistics.");
				alvs_update_connection_statistics(0, -1, 1);
			} else {
				alvs_write_log(LOG_DEBUG, "INACTIVE flag is now clear. Updating server statistics.");
				alvs_update_connection_statistics(0, 1, -1);
			}
		}

		if (cmem_alvs.conn_info_result.bound == false) {
			alvs_write_log(LOG_DEBUG, "Try to bind server");
			alvs_server_try_bind(conn->server_addr, conn->virtual_addr, conn->server_port, conn->virtual_port, conn->protocol);
		}

		flags &= IP_VS_CONN_F_BACKUP_UPD_MASK;
		flags |= cmem_alvs.conn_info_result.conn_flags & ~IP_VS_CONN_F_BACKUP_UPD_MASK;
		cmem_alvs.conn_info_result.conn_flags = flags;

		cmem_alvs.conn_info_result.conn_state = (enum alvs_tcp_conn_state)conn->state;

		/*mark connection as active, aging will handle it*/
		cmem_alvs.conn_info_result.aging_bit = 1;
		cmem_alvs.conn_info_result.age_iteration = 0;

		ezdp_modify_table_entry(&shared_cmem_alvs.conn_info_struct_desc,
					conn_index,
					&cmem_alvs.conn_info_result,
					sizeof(struct alvs_conn_info_result),
					0,
					cmem_wa.alvs_wa.conn_info_table_wa,
					sizeof(cmem_wa.alvs_wa.conn_info_table_wa));

		final_res = 0;
	} else {
		cmem_alvs.server_class_key.virtual_ip = conn->virtual_addr;
		cmem_alvs.server_class_key.virtual_port = conn->virtual_port;
		cmem_alvs.server_class_key.server_ip = conn->server_addr;
		cmem_alvs.server_class_key.server_port = conn->server_port;
		cmem_alvs.server_class_key.protocol = conn->protocol;

		alvs_write_log(LOG_DEBUG, "Server (0x%x:%d) in service (0x%x:%d, protocol=%d)...",
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

		/*create new connection*/
		/* TODO - update timeout */
		if (rc == 0) {
			lookup_res = alvs_server_info_lookup(server_class_res_ptr->server_index);
			if (lookup_res != 0) {
				/*no server info - weird error scenario*/
				alvs_write_log(LOG_DEBUG, "server_info_Result lookup for server_idx = %d FAILED ", server_class_res_ptr->server_index);
				alvs_unlock_connection(hash_value);
				return lookup_res;
			}
			create_entry_res = alvs_conn_create_new_entry(true, server_class_res_ptr->server_index, 0, (enum alvs_tcp_conn_state)conn->state, flags, false);
		} else {
			alvs_write_log(LOG_DEBUG, "Server not found, creating unbound connection");
			create_entry_res = alvs_conn_create_new_entry(false, conn->server_addr, conn->server_port, (enum alvs_tcp_conn_state)conn->state, flags, false);
		}

		if (create_entry_res == ALVS_SERVICE_DATA_PATH_IGNORE) {
			final_res = -1;
		} else {
			alvs_write_log(LOG_DEBUG, "Connection created");
			final_res = 0;
		}
	}

	alvs_unlock_connection(hash_value);
	return final_res;
}

/******************************************************************************
 * \brief       run state sync backup on received frame.
 *		perform version & sync id check and iterate over all connections and
 *		for each one run alvs_state_sync_process_conn function.
 *
 * \return      void
 *
 */
static __always_inline
void alvs_state_sync_backup(ezframe_t __cmem * frame, uint8_t *buffer, uint32_t buflen)
{
	uint32_t size;
	int32_t rc;
	uint32_t ind;
	struct alvs_state_sync_header *hdr;
	struct alvs_state_sync_conn *conn;
	uint8_t tail[sizeof(struct alvs_state_sync_conn)];  /* assuming connection size os bigger than header size */
	uint8_t tail_len = 0;
	uint8_t conn_count;
	uint32_t b_syncid;

	rc = ezdp_lookup_table_entry(&shared_cmem_nw.app_info_struct_desc,
					ALVS_APPLICATION_INFO_INDEX, &cmem_wa.alvs_wa.alvs_app_info_result,
					sizeof(struct alvs_app_info_result), 0);
	if (unlikely(rc != 0)) {
		alvs_write_log(LOG_DEBUG, "ERROR - failed in lookup for ALVS application info.");
		alvs_discard_and_stats(ALVS_ERROR_STATE_SYNC_LOOKUP_FAIL);
		return;
	}

	if (!cmem_wa.alvs_wa.alvs_app_info_result.backup_bit) {
		alvs_write_log(LOG_DEBUG, "Backup state sync daemon is not configured.");
		alvs_discard_and_stats(ALVS_ERROR_STATE_SYNC_BACKUP_DOWN);
		return;
	}

	b_syncid = cmem_wa.alvs_wa.alvs_app_info_result.b_sync_id;
	while (buflen + tail_len < sizeof(struct alvs_state_sync_header)) {
		if (ezframe_next_buf(frame, 0) != 0) {
			alvs_write_log(LOG_DEBUG, "ERROR - message header too short");
			alvs_discard_and_stats(ALVS_ERROR_STATE_SYNC_BAD_HEADER_SIZE);
			return;
		}

		/* Store tail */
		ezdp_mem_copy(tail + tail_len, buffer, buflen);
		tail_len += buflen;

		/* Load next buffer */
		buffer = ezframe_load_buf(frame, frame_data, &buflen, 0);
	}

	size = sizeof(struct alvs_state_sync_header) - tail_len;
	if (tail_len > 0) {
		ezdp_mem_copy(tail + tail_len, buffer, size);
		hdr = (struct alvs_state_sync_header *)tail;
	} else {
		hdr = (struct alvs_state_sync_header *)buffer;
	}
	buflen -= size;
	buffer += size;

	/* SyncID sanity check */
	if (b_syncid != 0 && hdr->syncid != b_syncid) {
		alvs_write_log(LOG_DEBUG, "Ignoring message with syncid %d.", hdr->syncid);
		alvs_discard_and_stats(ALVS_ERROR_STATE_SYNC_BACKUP_NOT_MY_SYNCID);
		return;
	}

	/* Check version */
	if ((hdr->version == ALVS_STATE_SYNC_PROTO_VER) && (hdr->reserved == 0)
	    && (hdr->spare == 0)) {

		/* Handle version 1 message */
		conn_count = hdr->conn_count;
		alvs_write_log(LOG_DEBUG, "Message contains %d connections", conn_count);
		for (ind = 0; ind < conn_count; ind++) {
			alvs_write_log(LOG_DEBUG, "Processing connection %d", ind);
			tail_len = 0;

			/* Assuming no optional parameters */
			while (buflen + tail_len < sizeof(struct alvs_state_sync_conn)) {
				alvs_write_log(LOG_DEBUG, "Need to load next buffer");
				if (ezframe_next_buf(frame, 0) != 0) {
					alvs_write_log(LOG_DEBUG, "ERROR - Dropping buffer, too small");
					alvs_discard_and_stats(ALVS_ERROR_STATE_SYNC_BAD_BUFFER);
					return;
				}

				/* Store tail */
				ezdp_mem_copy(tail + tail_len, buffer, buflen);
				tail_len += buflen;

				/* Load next buffer */
				buffer = ezframe_load_buf(frame, frame_data, &buflen, 0);
			}

			size = sizeof(struct alvs_state_sync_conn) - tail_len;
			if (tail_len > 0) {
				ezdp_mem_copy(tail + tail_len, buffer, size);
				conn = (struct alvs_state_sync_conn *)tail;
			} else {
				conn = (struct alvs_state_sync_conn *)buffer;
			}
			buflen -= size;
			buffer += size;

			/* Process a single sync_conn */
			rc = alvs_state_sync_process_conn(conn);
			if (rc < 0) {
				alvs_write_log(LOG_DEBUG, "Decoding connection ERROR (%d) - Dropping buffer", rc);
				alvs_discard_and_stats(ALVS_ERROR_STATE_SYNC_DECODE_CONN);
				return;
			} else if (rc > 0) {
				alvs_write_log(LOG_DEBUG, "Decoding connection ERROR (%d) - skipping connection", rc);
			}

			/* TODO - Make sure we have 32 bit alignment? */
		}

		/* Discard frame without updating statistics */
		alvs_discard_frame();

	} else {
		alvs_write_log(LOG_DEBUG, "ERROR - Currently dropping sync messages of version 0");
		alvs_discard_and_stats(ALVS_ERROR_STATE_SYNC_BAD_MESSAGE_VERSION);
	}
}


#endif /* ALVS_STATE_SYNC_BACKUP_H_ */
