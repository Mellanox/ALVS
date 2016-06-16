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

#include "alvs_server.h"
#include "alvs_utils.h"
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
 *              DBs. the function first locks the connection (using DP spinlock) and
 *              then allocates new index from connection index pool. this index is used
 *              as the key to the connection info DB and as the result to the connection
 *              info DB.
 *              this function is called only after all scheduling process is done so all
 *              info for entry is taken from the service and server data.
 *              the connection classification key is built from the service classification key
 *              plus other fields taken from the frame itself.
 *              to prevent any race conditions or cases were 2 threads are trying to create the
 *              same connection we use a DP spinlock and busy wait of all threads which are locked.
 *              locked threads will wait until all creation process is done and then will continue to
 *              regular connection data path.
 *
 * \return      return alvs_service_output_result:
 *                      ALVS_SERVICE_DATA_PATH_IGNORE - frame was sent to host or dropped
 *                      ALVS_SERVICE_DATA_PATH_RETRY - retry to transmit frame via regular connection data path
 *                      ALVS_SERVICE_DATA_PATH_SUCCESS - a new connection entry was created.
 *
 */
static __always_inline
enum alvs_service_output_result alvs_conn_create_new_entry(uint16_t server_index, enum alvs_tcp_conn_state conn_state)
{
	uint32_t conn_index;
	uint32_t rc;
	ezdp_hashed_key_t hash_value;

	alvs_lock_connection(&hash_value);

#if 0
	printf("Creating a new connection with server index %x.\n", server_index);
#endif

	/*allocate new index*/
	conn_index = ezdp_alloc_index(ALVS_CONN_INDEX_POOL_ID);
	if (conn_index == EZDP_NULL_INDEX) {
		printf("error allocation index from pool - drop frame\n");
		/*drop frame*/
		alvs_update_discard_statistics(ALVS_ERROR_CONN_INDEX_ALLOC_FAIL);
		alvs_discard_frame();
		alvs_unlock_connection(hash_value);
		return ALVS_SERVICE_DATA_PATH_IGNORE;
	}

#if 0
	printf("allocated index = 0x%x\n", conn_index);
#endif

	cmem_alvs.conn_info_result.aging_bit = 1;
	cmem_alvs.conn_info_result.sync_bit = 1;
	cmem_alvs.conn_info_result.reset_bit = 0;
	cmem_alvs.conn_info_result.delete_bit = 0;
	cmem_alvs.conn_info_result.conn_flags = cmem_alvs.server_info_result.conn_flags;
	cmem_alvs.conn_info_result.server_index = server_index;
	cmem_alvs.conn_info_result.conn_state = conn_state;
	cmem_alvs.conn_info_result.age_iteration = 0;
	cmem_alvs.conn_info_result.conn_stats_base.mem_type = EZDP_EXTERNAL_MS;
	cmem_alvs.conn_info_result.conn_stats_base.msid	= EMEM_CONN_STATS_ON_DEMAND_MSID;
	cmem_alvs.conn_info_result.conn_stats_base.element_index = conn_index;

	ezdp_mem_copy(&cmem_alvs.conn_info_result.conn_class_key, &cmem_alvs.conn_class_key, sizeof(struct alvs_conn_classification_key));

	/*first create connection info entry*/
	ezdp_add_table_entry(&shared_cmem_alvs.conn_info_struct_desc,
			     conn_index,
			     &cmem_alvs.conn_info_result,
			     sizeof(struct alvs_conn_info_result),
			     EZDP_UNCONDITIONAL,
			     cmem_wa.alvs_wa.table_work_area,
			     sizeof(cmem_wa.alvs_wa.table_work_area));

	cmem_alvs.conn_result.conn_index = conn_index;

	rc = ezdp_add_hash_entry(&shared_cmem_alvs.conn_class_struct_desc,
				 &cmem_alvs.conn_class_key,
				 sizeof(struct alvs_conn_classification_key),
				 &cmem_alvs.conn_result,
				 sizeof(struct alvs_conn_classification_result),
				 0,
				 cmem_wa.alvs_wa.conn_hash_wa,
				 sizeof(cmem_wa.alvs_wa.conn_hash_wa));

	if (rc != 0) {
		printf("fail to allocate new connection classification hash entry\n");
		/*free allocated table entry*/
		ezdp_free_index(ALVS_CONN_INDEX_POOL_ID, conn_index);
		alvs_unlock_connection(hash_value);
		if (rc == EEXIST) {
			printf("connection entry already exist!\n");
			return ALVS_SERVICE_DATA_PATH_RETRY;
		}

		printf("fail to add new connection info entry - drop frame\n");
		/*drop frame*/
		alvs_update_discard_statistics(ALVS_ERROR_CONN_INFO_ALLOC_FAIL);
		alvs_discard_frame();
		return ALVS_SERVICE_DATA_PATH_IGNORE;
	}
	/*update scheduled connections and active connections*/
	alvs_update_connection_statistics(1, 1, 0);

	alvs_unlock_connection(hash_value);

	printf("Connection added successfully.\n");

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
		printf("fail in conn info lookup while trying to modify!\n");
		alvs_unlock_connection(hash_value);
		return rc;
	}

	cmem_alvs.conn_info_result.aging_bit = 1;
	cmem_alvs.conn_info_result.conn_state = new_state;
	cmem_alvs.conn_info_result.sync_bit = 1;

	rc = ezdp_modify_table_entry(&shared_cmem_alvs.conn_info_struct_desc,
			conn_index,
			&cmem_alvs.conn_info_result,
			sizeof(struct alvs_conn_info_result),
			EZDP_UNCONDITIONAL,
			cmem_wa.alvs_wa.table_work_area,
			sizeof(EZDP_TABLE_PRM_WORK_AREA_SIZE));

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
uint32_t alvs_conn_mark_to_delete(uint32_t conn_index, uint8_t reset_bit)
{
	uint32_t rc;
	ezdp_hashed_key_t hash_value;

	/*lock connection*/
	alvs_lock_connection(&hash_value);

	/*perform another lookup to prevent race conditions*/
	rc = alvs_conn_info_lookup(conn_index);

	if (rc != 0) {
		printf("fail in conn info lookup while trying to modify!\n");
		alvs_unlock_connection(hash_value);
		return rc;
	}

	/* need to modify connection entry - change state for close and set aging bit on. */
	cmem_alvs.conn_info_result.aging_bit = 0;
	cmem_alvs.conn_info_result.delete_bit = 1;
	cmem_alvs.conn_info_result.reset_bit = reset_bit;

	rc = ezdp_modify_table_entry(&shared_cmem_alvs.conn_info_struct_desc,
			conn_index,
			&cmem_alvs.conn_info_result,
			sizeof(struct alvs_conn_info_result),
			EZDP_UNCONDITIONAL,
			cmem_wa.alvs_wa.table_work_area,
			sizeof(EZDP_TABLE_PRM_WORK_AREA_SIZE));

	/*unlock connection*/
	alvs_unlock_connection(hash_value);
	return rc;
}

/******************************************************************************
 * \brief       remove a connection entry from connection info DB and from
 *              connection classification table. this function is called from
 *              from aging mechanism only.
 *
 * \return        void
 */
static __always_inline
void alvs_conn_delete(uint32_t conn_index)
{
	ezdp_hashed_key_t hash_value;

	/*lock connection*/
	alvs_lock_connection(&hash_value);

	/*1st remove the classification entry*/
	if (ezdp_delete_hash_entry(&shared_cmem_alvs.conn_class_struct_desc,
			       &cmem_alvs.conn_info_result.conn_class_key,
			       sizeof(struct alvs_conn_classification_key),
			       0,
			       cmem_wa.alvs_wa.conn_hash_wa,
			       sizeof(cmem_wa.alvs_wa.conn_hash_wa)) != 0) {
		alvs_unlock_connection(hash_value);
		printf("unable to delete connection class entry for conn id = %d\n", conn_index);
		return;
	}

	/*now remove the connection info entry*/
	if (ezdp_delete_table_entry(&shared_cmem_alvs.conn_info_struct_desc,
				conn_index,
				0,
				cmem_wa.alvs_wa.table_work_area,
				sizeof(cmem_wa.alvs_wa.table_work_area)) != 0) {
		alvs_unlock_connection(hash_value);
		printf("unable to delete connection info entry for conn id = %d\n", conn_index);
		return;
	}

	ezdp_free_index(ALVS_CONN_INDEX_POOL_ID, conn_index);

	alvs_update_connection_statistics(-1, -1, 1);

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
		printf("fail in conn info lookup while trying to modify!\n");
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
			cmem_wa.alvs_wa.table_work_area,
			sizeof(EZDP_TABLE_PRM_WORK_AREA_SIZE));

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
		printf("fail in conn info lookup while trying to modify!\n");
		alvs_unlock_connection(hash_value);
		return rc;
	}

	/* turn off the aging bit */
	cmem_alvs.conn_info_result.aging_bit  =  0;
	cmem_alvs.conn_info_result.age_iteration  =  age_iteration;

	rc = ezdp_modify_table_entry(&shared_cmem_alvs.conn_info_struct_desc,
			conn_index,
			&cmem_alvs.conn_info_result,
			sizeof(struct alvs_conn_info_result),
			EZDP_UNCONDITIONAL,
			cmem_wa.alvs_wa.table_work_area,
			sizeof(EZDP_TABLE_PRM_WORK_AREA_SIZE));

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
	if (likely(cmem_alvs.server_info_result.routing_alg == ALVS_DIRECT_ROUTING)) {
#if 0
		printf("Direct-route!.\n");
#endif
		nw_do_route(&frame,
			    frame_base,
			    cmem_alvs.server_info_result.server_ip,
			    ezframe_get_buf_len(&frame));
	} else {
		printf("got unsupported routing algo = %d\n", cmem_alvs.server_info_result.routing_alg);
		/*drop frame*/
		alvs_update_discard_statistics(ALVS_ERROR_UNSUPPORTED_ROUTING_ALGO);
		alvs_discard_frame();
		return;
	}

	/*update statistics*/
	alvs_update_all_conn_statistics();
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

	/*perform lookup in conn info DB*/
	rc = alvs_conn_info_lookup(conn_index);

	if (likely(rc == 0)) {
		/*get destination server info*/
#if 0
		printf("Server index is 0x%x\n", cmem_alvs.conn_info_result.server_index);
#endif
		if (alvs_server_info_lookup(cmem_alvs.conn_info_result.server_index) != 0) {
			/*no server info - weird error scenario*/
			printf("error - alvs server info lookup fail. server id = %d!\n", cmem_alvs.conn_info_result.server_index);
			/*drop frame*/
			alvs_update_discard_statistics(ALVS_ERROR_SERVER_INFO_LKUP_FAIL);
			alvs_discard_frame();
			return;
		}

		/*check if someone already indicated that this connection should be deleted...*/
		if (cmem_alvs.conn_info_result.reset_bit == 1) {
			/*server in not available - close connection*/
			printf("connection is marked to delete!\n");
			/*drop frame*/
			alvs_update_discard_statistics(ALVS_ERROR_CONN_MARK_TO_DELETE);
			alvs_discard_frame();
			return;
		}

		/*check destination server status*/
		if (!(cmem_alvs.server_info_result.server_flags & IP_VS_DEST_F_AVAILABLE)) {
			/*server in not available - close connection*/
			printf("Server is unavailable.\n");
			if (alvs_conn_mark_to_delete(conn_index, 0) != 0) {
				/*unable to update connection - weird error scenario*/
				printf("error - fail to mark delete connection ID = %d!\n", conn_index);
				/*drop frame*/
				alvs_update_discard_statistics(ALVS_ERROR_CANT_MARK_DELETE);
				alvs_discard_frame();
				return;
			}
			/*silent drop the packet and continue*/
			alvs_update_discard_statistics(ALVS_ERROR_DEST_SERVER_IS_NOT_AVAIL);
			alvs_discard_frame();
			return;
		}

		/*check if state changed*/
		if (tcp_hdr->rst) {
			if (alvs_conn_mark_to_delete(conn_index, 1) != 0) {
				/*unable to update connection - weird error scenario*/
				printf("error - fail to mark delete connection ID = %d - tcp state changed to reset!\n", conn_index);
				/*drop frame*/
				alvs_update_discard_statistics(ALVS_ERROR_CANT_MARK_DELETE);
				alvs_discard_frame();
				return;
			}
		} else {
			if (cmem_alvs.conn_info_result.conn_state == ALVS_TCP_CONNECTION_ESTABLISHED && tcp_hdr->fin) {
				if (alvs_conn_update_state(conn_index, ALVS_TCP_CONNECTION_CLOSE_WAIT) != 0) {
					printf("error - fail to update connection state to close. conn index = %d.\n", conn_index);
					/*drop frame*/
					alvs_update_discard_statistics(ALVS_ERROR_CANT_UPDATE_CONNECTION_STATE);
					alvs_discard_frame();
					return;
				}
			} else if (cmem_alvs.conn_info_result.aging_bit == 0) { /*check if we need to update the aging bit*/
				/*set connection aging bit back to 1*/
				if (alvs_conn_refresh(conn_index) != 0) {
					printf("error - fail to refresh connection ID = %d.\n", conn_index);
					/*drop frame*/
					alvs_update_discard_statistics(ALVS_ERROR_CANT_UPDATE_CONNECTION_STATE);
					alvs_discard_frame();
					return;
				}
			}
		}

		alvs_conn_do_route(frame_base);
	} else {
		/*no classification info - weird error scenario*/
		printf("error - fail lookup to connection info DB. connection ID = %d.\n", conn_index);
		/*drop frame*/
		alvs_update_discard_statistics(ALVS_ERROR_CONN_INFO_LKUP_FAIL);
		alvs_discard_frame();
		return;
	}
}

#endif /* ALVS_CONN_H_ */
