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
*  File:                alvs_sh_sched.h
*  Desc:                handle source hash scheduling functionality
*/

#ifndef ALVS_SH_SCHED_H_
#define ALVS_SH_SCHED_H_

#include <linux/ip_vs.h>
#include <ezdp.h>

#include "alvs_utils.h"
#include "alvs_server.h"

/******************************************************************************
 * \brief       use source hash algorithm and sched info DB to pick destination server
 *              source hash algo use sip & source port of incoming packet to calculate
 *              a hash value (8 bits) which is later used as key (along with service ID)
 *              to the server sched DB.
 *              the output of the sched algo is a server index. we perform another lookup
 *              to the server info DB using the server index as a key and the output result
 *              of the server index is used when creating a new connection entry.
 *
 * \return	return true if scheduling was successful, false otherwise.
 */
static __always_inline
bool alvs_sh_get_server_info(uint8_t service_index, uint32_t sip, uint16_t sport, uint32_t is_fallback)
{
	uint32_t rc;

	uint32_t hash_value = ezdp_hash(sip,
					sport << (sizeof(sport) * 8),
					LOG2(ALVS_SIZE_OF_SCHED_BUCKET),
					sizeof(sip) + sizeof(sport),
					0,
					EZDP_HASH_BASE_MATRIX_HASH_BASE_MATRIX_0,
					EZDP_HASH_PERMUTATION_0);

	/*perform lookup in scheduling info DB*/
	rc = alvs_server_sched_lookup(service_index * ALVS_SIZE_OF_SCHED_BUCKET + hash_value);

	if (likely(rc == 0)) {
		if (is_fallback) {
			/* TODO add fallback implementation */
		} else {
			/*get server info*/
			rc = alvs_server_info_lookup(cmem_alvs.sched_info_result.server_index);

			if (likely(rc == 0)) {
				if (alvs_server_is_unavailable()) {
					printf("server is unavailable - drop frame!, server index = %d\n", cmem_alvs.sched_info_result.server_index);
					/*drop frame*/
					alvs_update_discard_statistics(ALVS_ERROR_SERVER_IS_UNAVAILABLE);
					alvs_discard_frame();
					return false;
				}
				/*update stats - update active connections, connections scheduled*/
			} else {
				printf("server info lookup fail - drop frame!, server index = %d\n", cmem_alvs.sched_info_result.server_index);
				/*drop frame*/
				alvs_update_discard_statistics(ALVS_ERROR_SERVER_INFO_LKUP_FAIL);
				alvs_discard_frame();
				return false;
			}
		}
	} else {
		printf("fail in scheduling info lookup - drop frame!\n");
		/*drop frame*/
		alvs_update_discard_statistics(ALVS_ERROR_FAIL_SH_SCHEDULING);
		alvs_discard_frame();
		return false;
	}

	return true;
}


/******************************************************************************
 * \brief       try to pick destination server for connection using source hash algorithm.
 *
 * \return      return true in case scheduling was successful, false otherwise
 */
static __always_inline
bool alvs_sh_schedule_connection(uint8_t service_index, uint32_t sip, uint16_t sport)
{
	if (unlikely(alvs_sh_get_server_info(service_index,
					     sip,
					     cmem_alvs.service_info_result.service_flags & IP_VS_SVC_F_SCHED_SH_PORT ? sport : 0,
					     cmem_alvs.service_info_result.service_flags & IP_VS_SVC_F_SCHED_SH_FALLBACK)) == false) {
		printf("fail to retrieve server info (source hash)\n");
		return false;
	}
	return true;
}

#endif /*ALVS_SH_SCHED_H_*/
