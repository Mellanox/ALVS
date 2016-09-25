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
*  File:                alvs_sched.h
*  Desc:                general scheduling functionality
*/

#ifndef ALVS_SCHED_H_
#define ALVS_SCHED_H_

#include <ezdp.h>

#include "alvs_utils.h"
#include "alvs_defs.h"
#include "alvs_server.h"

/*TODO remove*/
static __always_inline
uint32_t alvs_sched_service_info_lookup(uint8_t service_index)
{
	return ezdp_lookup_table_entry(&shared_cmem_alvs.service_info_struct_desc,
				service_index, &cmem_alvs.service_info_result,
				sizeof(struct alvs_service_info_result), 0);
}

/******************************************************************************
 * \brief       handle no active servers for service.
 *              drop the frame and update the statistics.
 */
static __always_inline
void alvs_sched_handle_no_active_servers(void)
{
	alvs_write_log(LOG_WARNING, "no servers for scheduling algorithm");
	/*drop frame*/
	alvs_discard_and_stats(ALVS_ERROR_NO_ACTIVE_SERVERS);
}

/******************************************************************************
 * \brief       check if there are active servers for current service.
 *
 * \return	return false, drop frame and update statistics if there are no
 *              active servers. return true otherwise.
 */
static __always_inline
bool alvs_sched_check_active_servers(void)
{
	if (unlikely(cmem_alvs.service_info_result.sched_entries_count == 0)) {
		alvs_sched_handle_no_active_servers();
		return false;
	}
	return true;
}

/******************************************************************************
 * \brief       use sip and source port to calculate
 *              a hash value (8 bits).
 *
 * \return	return the calculated hash value.
 */
static __always_inline
uint32_t alvs_sched_sh_get_scheduling_index(uint32_t sip, uint16_t sport)
{
	uint32_t sip_hash;
	uint32_t port_hash;
	uint32_t final_hash;

	sip_hash = ezdp_hash(sip,
			     0,
			     32,
			     sizeof(sip),
			     0,
			     EZDP_HASH_BASE_MATRIX_HASH_BASE_MATRIX_0,
			     EZDP_HASH_PERMUTATION_0);

	port_hash = ezdp_hash(sport,
			      0,
			      32,
			      sizeof(sport),
			      2,
			      EZDP_HASH_BASE_MATRIX_HASH_BASE_MATRIX_0,
			      EZDP_HASH_PERMUTATION_1);

	final_hash = ezdp_hash(sip_hash,
			       port_hash,
			       LOG2(ALVS_SIZE_OF_SCHED_BUCKET),
			       sizeof(sip_hash) + sizeof(port_hash),
			       0,
			       EZDP_HASH_BASE_MATRIX_HASH_BASE_MATRIX_0,
			       EZDP_HASH_PERMUTATION_3);

	return final_hash;
}

/******************************************************************************
 * \brief       use the given scheduling index to get a server info. the server
 *              index is retrieved by a scheduling index lookup and we perform
 *              another lookup to the server info DB using the server index as
 *              a key and the output result of the server index is used for
 *              creating a new connection entry.

 * \return      return alvs_sched_server_result:
 *                      ALVS_SCHED_SERVER_SUCCESS - successfully retrieved the server info.
 *                      ALVS_SCHED_SERVER_FAILED - failed to get the server info. frame discarded.
 *                      ALVS_SCHED_SERVER_EMPTY - there is no server for the given scheduling index.
 *                      ALVS_SCHED_SERVER_UNAVAILABLE - the selected server is unavailable.
*/
static __always_inline
enum alvs_sched_server_result alvs_sched_get_server_info(uint8_t service_index, uint16_t scheduling_index)
{
	uint32_t rc;

	/*perform a lookup in scheduling info DB*/
	alvs_write_log(LOG_DEBUG, "service_idx = %d, sched_idx = %d ", service_index, scheduling_index);
	rc = alvs_server_sched_lookup(scheduling_index);

	/*schedule info lookup succeeded*/
	if (likely(rc == 0)) {
		/*perform a look in the server info DB*/
		alvs_write_log(LOG_DEBUG, "service_idx = %d, server_idx = %d", service_index, cmem_alvs.sched_info_result.server_index);
		if (unlikely(alvs_server_info_lookup(cmem_alvs.sched_info_result.server_index))) {
			/*server info lookup failed*/
			alvs_write_log(LOG_ERR, "service_idx = %d, server_idx = %d server_info_lookup FAILED", service_index, cmem_alvs.sched_info_result.server_index);
			alvs_discard_and_stats(ALVS_ERROR_SERVER_INFO_LKUP_FAIL);
			return ALVS_SCHED_SERVER_FAILED;
		}
		if (alvs_server_overload_on_create_conn(cmem_alvs.sched_info_result.server_index) & IP_VS_DEST_F_OVERLOAD) {
			alvs_write_log(LOG_DEBUG, "service_idx = %d, server_idx = %d is unavailable", service_index, cmem_alvs.sched_info_result.server_index);
			return ALVS_SCHED_SERVER_UNAVAILABLE;
		}
		return ALVS_SCHED_SERVER_SUCCESS;
	}

	/*schedule info lookup failed*/
	if (unlikely(rc == ENOENT)) {
		/*no server index for the given scheduling index*/
		alvs_write_log(LOG_DEBUG, "service_idx = %d server_sched_lookup ENOENT", service_index);
		return ALVS_SCHED_SERVER_EMPTY;
	}
	alvs_write_log(LOG_DEBUG, "service_idx = %d server_sched_lookup FAILED", service_index);
	/*drop frame*/
	alvs_discard_and_stats(ALVS_ERROR_SCHEDULING_FAIL);
	return ALVS_SCHED_SERVER_FAILED;
}

/******************************************************************************
 * \brief       try to pick destination server for connection with source hash algorithm.
 *              using source hash algorithm and sched info DB to pick destination server
 *              source hash algo. the hash is later used as key (along with service ID)
 *              to the server sched DB.
 *
 * \return      return true in case scheduling was successful, false otherwise
 */
static __always_inline
bool alvs_sched_sh_schedule_connection(uint8_t service_index, uint32_t sip, uint16_t sport)
{
	enum alvs_sched_server_result sched_server_result;
	uint32_t is_fallback = cmem_alvs.service_info_result.service_flags & IP_VS_SVC_F_SCHED_SH_FALLBACK;
	uint32_t final_hash;
	int offset, tmp_offset;

	sport = cmem_alvs.service_info_result.service_flags & IP_VS_SVC_F_SCHED_SH_PORT ? sport : 0;
	final_hash = alvs_sched_sh_get_scheduling_index(sip, sport);
	alvs_write_log(LOG_DEBUG, "sport = %d, hash_value = 0x%x, input to hash = %d", sport, final_hash, (uint32_t)sport << (sizeof(sport) * 8));

	sched_server_result = alvs_sched_get_server_info(service_index, service_index * ALVS_SIZE_OF_SCHED_BUCKET + final_hash);
	if (likely(sched_server_result == ALVS_SCHED_SERVER_SUCCESS)) {
		return true;
	}

	/* update statistics for special cases */
	if (unlikely(sched_server_result == ALVS_SCHED_SERVER_UNAVAILABLE)) {
		if (unlikely(is_fallback)) {
			for (offset = 0; offset < ALVS_SIZE_OF_SCHED_BUCKET; offset++) {
				tmp_offset = (offset + final_hash) & (ALVS_SIZE_OF_SCHED_BUCKET-1);
				sched_server_result = alvs_sched_get_server_info(service_index, service_index * ALVS_SIZE_OF_SCHED_BUCKET + tmp_offset);
				if (likely(sched_server_result == ALVS_SCHED_SERVER_SUCCESS)) {
					return true;
				}
				if (unlikely(sched_server_result != ALVS_SCHED_SERVER_UNAVAILABLE))
					break;
			}
			if (unlikely(sched_server_result == ALVS_SCHED_SERVER_UNAVAILABLE))
				alvs_discard_and_stats(ALVS_ERROR_SERVER_IS_UNAVAILABLE);
		} else {
			alvs_discard_and_stats(ALVS_ERROR_SERVER_IS_UNAVAILABLE);
		}
	}
	if (unlikely(sched_server_result == ALVS_SCHED_SERVER_EMPTY)) {
		/*for sh, such case can happen only if during scheduling all the available servers were removed*/
		alvs_sched_handle_no_active_servers();
		return false;
	}

	alvs_write_log(LOG_ERR, "service_idx = %d sh_schedule_connection FAILED", service_index);
	return false;
}

/******************************************************************************
 * \brief       try to pick destination server for connection with round robin algorithm.
 *              using round robin algorithm and sched info DB and a service sched
 *              counter to pick a destination server.
 *
 * \return      return true in case scheduling was successful, false otherwise
 */
static __always_inline
bool alvs_sched_rr_schedule_connection(uint8_t service_index)
{
	enum alvs_sched_server_result sched_server_result;
	uint32_t sched_count;
	uint16_t retries = ALVS_SCHED_RR_RETRIES;

	/*get server info according to schedule counter*/
	sched_count = ezdp_atomic_read_and_inc32_sum_addr(cmem_alvs.service_info_result.service_sched_ctr, NULL);

	alvs_write_log(LOG_DEBUG, "service_index = %d, sched_count = %d", service_index, sched_count);

	do {
		/*check of entries count is power of 2. if so, do not use mod operations. (use mask)*/
		if (ezdp_count_bits(cmem_alvs.service_info_result.sched_entries_count, 0, 16) == 1) {
			/*workaround for >= 256 bucket size. bucket size must be power of 2*/
			sched_count = sched_count & (cmem_alvs.service_info_result.sched_entries_count-1);
		} else {
			/* this equation should decrease to minimum the deviation of connection allocation between servers
			 * when sched_count is overlapped
			 */
			sched_count = ezdp_mod((ezdp_mod(sched_count, cmem_alvs.service_info_result.sched_entries_count, 0, 0) +
				ezdp_mod(sched_count, cmem_alvs.service_info_result.sched_entries_count, 16, 0)), cmem_alvs.service_info_result.sched_entries_count, 0, 0);
		}
		sched_server_result = alvs_sched_get_server_info(service_index, service_index * ALVS_SIZE_OF_SCHED_BUCKET + sched_count);
		/* retry if the selected server is no longer in the bucket */
		if (unlikely(sched_server_result == ALVS_SCHED_SERVER_EMPTY)) {
			if (unlikely(alvs_sched_service_info_lookup(service_index))) {
				/*drop frame*/
				alvs_discard_and_stats(ALVS_ERROR_SERVICE_INFO_LOOKUP);
				goto fail_out;
			}
			if (unlikely(alvs_sched_check_active_servers() == false)) {
				goto fail_out;
			}
		} else {
			break;
		}
	} while (unlikely(--retries));
	alvs_write_log(LOG_DEBUG, "entries = %d, entry_index = %d, result = %d",
		       cmem_alvs.service_info_result.sched_entries_count, sched_count, sched_server_result);

	if (likely(sched_server_result == ALVS_SCHED_SERVER_SUCCESS)) {
		return true;
	}

	/* drop frame and update statistics for special cases */
	if (unlikely(sched_server_result == ALVS_SCHED_SERVER_UNAVAILABLE)) {
		alvs_discard_and_stats(ALVS_ERROR_SERVER_IS_UNAVAILABLE);
	} else if (unlikely(sched_server_result == ALVS_SCHED_SERVER_EMPTY)) {
		alvs_discard_and_stats(ALVS_ERROR_SCHEDULING_FAIL);
	}

	alvs_write_log(LOG_ERR, "service_idx = %d rr_schedule_connection FAILED", service_index);
fail_out:
	return false;
}

#endif /*ALVS_SCHED_H_*/
