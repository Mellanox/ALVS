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
*  File:                alvs_defs.h
*  Desc:                general defs for data path ALVS
*/

#ifndef ALVS_DEFS_H_
#define ALVS_DEFS_H_

#include <ezdp.h>
#include <ezframe.h>
#include "alvs_search_defs.h"
#include "nw_defs.h"


/****************************************************************
 * General definitions
 ***************************************************************/

#define LOG2(x) (x == 2 ? 1 : x == 4 ? 2 : x == 8 ? 3 : x == 16 ? 4 : x == 32 ? 5 : \
				x == 64 ? 6 : x == 128 ? 7 : x == 256 ? 8 : x == 512 ? 9 : x == 1024 ? 10 : x == 2048 ? 11 : \
				x == 4096 ? 12 : x == 8192 ? 13 : x == 16384 ? 14 : x == 32768 ? 15 : x == 65536 ? 16 : 0)

/*prototypes*/
bool init_alvs_shared_cmem(void);
bool init_alvs_private_cmem(void);

#define MAX_DECODE_SIZE 28

/****************************************************************
 * enums for errors and other failure cases.
 * seperated to cases where packet is sent to host
 * vs cases where packet is dropped.
 ***************************************************************/
/*packet sent to host*/
enum alvs_to_host_cause_id {
	ALVS_PACKET_FAIL_SERVICE_CLASS_LOOKUP = NW_CAUSE_ID_LAST,
	ALVS_CAUSE_ID_LAST
};

/*packet drop*/
enum alvs_error_id {
	ALVS_ERROR_UNSUPPORTED_ROUTING_ALGO     = 0,
	ALVS_ERROR_CANT_EXPIRE_CONNECTION       = 1,
	ALVS_ERROR_CANT_UPDATE_CONNECTION_STATE = 2,
	ALVS_ERROR_CONN_INFO_LKUP_FAIL          = 3,
	ALVS_ERROR_CONN_CLASS_ALLOC_FAIL        = 4,
	ALVS_ERROR_CONN_INFO_ALLOC_FAIL         = 5,
	ALVS_ERROR_CONN_INDEX_ALLOC_FAIL        = 6,
	ALVS_ERROR_SERVICE_CLASS_LKUP_FAIL      = 7,
	ALVS_ERROR_FAIL_SH_SCHEDULING           = 8,
	ALVS_ERROR_SERVER_INFO_LKUP_FAIL        = 9,
	ALVS_ERROR_SERVER_IS_UNAVAILABLE        = 10,
	ALVS_ERROR_SERVER_INDEX_LKUP_FAIL       = 11,
	ALVS_ERROR_CONN_CLASS_LKUP_FAIL         = 12,
	ALVS_ERROR_SERVICE_INFO_LOOKUP          = 13,
	ALVS_ERROR_UNSUPPORTED_SCHED_ALGO       = 14,
	ALVS_ERROR_CANT_MARK_DELETE             = 15,
	ALVS_ERROR_DEST_SERVER_IS_NOT_AVAIL     = 16,
	ALVS_ERROR_ID_LAST
};

/*packet drop*/
enum alvs_service_output_result {
	ALVS_SERVICE_DATA_PATH_IGNORE           = 0, /* Frame is dropped or send to host */
	ALVS_SERVICE_DATA_PATH_RETRY            = 1,
	ALVS_SERVICE_DATA_PATH_SUCCESS          = 2,
	ALVS_SERVICE_DATA_PATH_ID_LAST
};

/****************************************************************
 * ALVS definitions
 ***************************************************************/

#define ALVS_CONN_LOCK_ELEMENTS_COUNT (256 * 1024)
#define ALVS_CONN_LOCK_ELEMENTS_MASK  (ALVS_CONN_LOCK_ELEMENTS_COUNT - 1)

/*timer defs*/
enum alvs_tcp_states_multipliers {
	ALVS_TCP_EST_MULTIPLIER         = 16,
	ALVS_TCP_CLOSE_WAIT_MULTIPLIER  = 1,
};

#define ALVS_AGING_TIMER_SCAN_ENTRIES_PER_JOB   128    /* TODO - needs to be 512 when using release 2.1 */
#define ALVS_AGING_TIMER_EVENTS_PER_ITERATION   (ALVS_CONN_MAX_ENTRIES / ALVS_AGING_TIMER_SCAN_ENTRIES_PER_JOB)
#define ALVS_AGING_TIMER_EVENT_ID_MASK          (~ALVS_AGING_TIMER_EVENTS_PER_ITERATION)

/* Number of lag members is hard coded and depended on compilation flag. */
/* in case user wants to disable LAG functionality need to set this flag. */
#	define DEFAULT_NW_BASE_LOGICAL_ID           0
#	define NUM_OF_LAG_MEMBERS                   4

/*************************************************************
 * SEARCH defs
 ************************************************************/


/***************** global CMEM data *************************/
/***********************************************************************//**
 * \struct      alvs_cmem
 * \brief       include all variables located on private CMEM
 **************************************************************************/
struct alvs_cmem {
	struct alvs_conn_classification_key             conn_class_key;
	/**< conn class key */
	struct alvs_service_classification_key          service_class_key;
	/**< service class key */
	struct alvs_conn_info_result                    conn_info_result;
	/**< connection info result */
	struct alvs_server_info_result                  server_info_result;
	/**< server info result */
	struct alvs_service_info_result                 service_info_result;
	/**< server info result */
	struct alvs_sched_info_result                   sched_info_result;
	/**< scheduling info result */
	ezdp_spinlock_t                                 conn_spinlock;
	/**< connection spinlock */
	struct alvs_conn_classification_result          conn_result;
	/**< connection result */
} __packed;

/***********************************************************************//**
 * \struct      alvs_workarea
 * \brief       DP workarea for hash and direct tables used by ALVS.
 *              temp variables located on private CMEM
 **************************************************************************/
union alvs_workarea {
	char conn_hash_wa[EZDP_HASH_WORK_AREA_SIZE(sizeof(struct alvs_conn_classification_result), sizeof(struct alvs_conn_classification_key))];
	char service_hash_wa[EZDP_HASH_WORK_AREA_SIZE(sizeof(struct alvs_service_classification_result), sizeof(struct alvs_service_classification_key))];
	char table_work_area[EZDP_TABLE_PRM_WORK_AREA_SIZE];
};

/***********************************************************************//**
 * \struct      alvs_shared_cmem
 * \brief       all variables located on shared CMEM
 **************************************************************************/

struct alvs_shared_cmem {
	ezdp_table_struct_desc_t    conn_info_struct_desc;
	ezdp_table_struct_desc_t    server_info_struct_desc;
	ezdp_hash_struct_desc_t     conn_class_struct_desc;
	ezdp_hash_struct_desc_t     service_class_struct_desc;
	ezdp_table_struct_desc_t    service_info_struct_desc;
	ezdp_table_struct_desc_t    sched_info_struct_desc;
} __packed;

/*************************************************************
 * CMEM GLOBALS
 *************************************************************/
extern struct alvs_cmem         cmem_alvs;
extern struct alvs_shared_cmem  shared_cmem_alvs;
extern union cmem_workarea      cmem_wa;
extern ezframe_t                frame;
extern uint8_t                  frame_data[EZFRAME_BUF_DATA_SIZE];

#endif /* ALVS_DEFS_H_ */
