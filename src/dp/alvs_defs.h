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
				x == 4096 ? 12 : x == 8192 ? 13 : x == 16384 ? 14 : x == 32768 ? 15 : x == 65536 ? 16 : \
				x == 131072 ? 17 : x == 262144 ? 18 : 0)

/*prototypes*/
bool init_alvs_shared_cmem(void);
bool init_alvs_private_cmem(void);
bool init_alvs_emem(void);


#define MAX_DECODE_SIZE 28

enum alvs_service_output_result {
	ALVS_SERVICE_DATA_PATH_IGNORE           = 0, /* Frame is dropped or send to host */
	ALVS_SERVICE_DATA_PATH_RETRY            = 1,
	ALVS_SERVICE_DATA_PATH_SUCCESS          = 2,
};

/****************************************************************
 * ALVS definitions
 ***************************************************************/

#define ALVS_TIMER_INTERVAL_SEC 16

#define ALVS_SCHED_RR_RETRIES 10

#define ALVS_STATE_SYNC_PROTO_VER        1
#define ALVS_STATE_SYNC_HEADROOM         64
#define ALVS_STATE_SYNC_BUFFERS_LIMIT    5 /*value should fit 4 bits*/
#define ALVS_STATE_SYNC_DST_IP           0xe0000051/*224.0.0.81*/
#define ALVS_STATE_SYNC_DST_PORT         8848
#define ALVS_STATE_SYNC_DST_MAC          {0x01, 0x00, 0x5e, 0x00, 0x00, 0x51}

enum alvs_conn_sync_status {
	ALVS_CONN_SYNC_NEED              = 0,
	ALVS_CONN_SYNC_NO_NEED           = 1,
	ALVS_CONN_SYNC_LAST
};

struct alvs_conn_sync_state {
	enum alvs_conn_sync_status        conn_sync_status:4;
	unsigned                          amount_buffers:4;
	uint8_t                           *current_base;
	uint8_t                           current_len;
	uint8_t                           conn_count;
};

enum alvs_sched_server_result {
	ALVS_SCHED_SERVER_SUCCESS       = 0,
	ALVS_SCHED_SERVER_FAILED        = 1,
	ALVS_SCHED_SERVER_EMPTY         = 2,
	ALVS_SCHED_SERVER_UNAVAILABLE   = 3,
	ALVS_SCHED_LAST
};

#define ALVS_CONN_LOCK_ELEMENTS_MASK  (ALVS_CONN_LOCK_ELEMENTS_COUNT - 1)


#define ALVS_AGING_TIMER_SCAN_ENTRIES_PER_JOB   128
#define ALVS_AGING_TIMER_EVENTS_PER_ITERATION   (ALVS_CONN_MAX_ENTRIES / ALVS_AGING_TIMER_SCAN_ENTRIES_PER_JOB)


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
	struct alvs_server_classification_key           server_class_key;
	/**< server class key */
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
	/**< connection class result */
	struct alvs_conn_sync_state                     conn_sync_state;
	/**< connection state synchronization */
} __packed;

/***********************************************************************//**
 * \struct      alvs_workarea
 * \brief       DP workarea for hash and direct tables used by ALVS.
 *              temp variables located on private CMEM
 **************************************************************************/
union alvs_workarea {
	char conn_hash_wa[EZDP_HASH_WORK_AREA_SIZE(sizeof(struct alvs_conn_classification_result), sizeof(struct alvs_conn_classification_key))];
	char server_hash_wa[EZDP_HASH_WORK_AREA_SIZE(sizeof(struct alvs_server_classification_result), sizeof(struct alvs_server_classification_key))];
	char service_hash_wa[EZDP_HASH_WORK_AREA_SIZE(sizeof(struct alvs_service_classification_result), sizeof(struct alvs_service_classification_key))];
	char conn_info_table_wa[EZDP_TABLE_WORK_AREA_SIZE(sizeof(struct alvs_conn_info_result))];
	char table_struct_work_area[EZDP_TABLE_WORK_AREA_SIZE(sizeof(ezdp_table_struct_desc_t))];
	uint64_t counter_work_area;
	struct alvs_app_info_result alvs_app_info_result;
	/**< application info class result */
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
	ezdp_hash_struct_desc_t     server_class_struct_desc;
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


/*************************************************************
 * State sync structures
 *************************************************************/
struct alvs_state_sync_header {
	uint8_t    reserved;
	/* must be zero for version 0 backward compatibility */

	uint8_t    syncid;
	/* Sync ID */

	uint16_t   size;
	/* size of header in bytes */

	uint8_t    conn_count;
	/* number of connections in the message */

	uint8_t    version;
	/* version of message - should be set to SYNC_PROTO_VER */

	uint16_t   spare;
	/* must be zero */
};


struct alvs_state_sync_conn {
	uint8_t    type;
	/* IPv4/IPv6 */

	uint8_t    protocol;
	/* protocol of connection (TCP/UDP) */

	unsigned   version   : 4;
	/* version, set to 0 for IPv4 */

	unsigned   size      : 12;
	/* size of the connection in bytes */

	uint32_t   flags;
	/* status flags */

	uint16_t   state;
	/*state info */

	/* ports */
	uint16_t   client_port;
	uint16_t   virtual_port;
	uint16_t   server_port;

	uint32_t   fwmark;
	/* Firewall mark from skb - not supported */

	uint32_t   timeout;
	/* Timeout of the connection */

	/* addresses (IPv4 only) */
	in_addr_t  client_addr;
	in_addr_t  virtual_addr;
	in_addr_t  server_addr;
} __packed;


#endif /* ALVS_DEFS_H_ */
