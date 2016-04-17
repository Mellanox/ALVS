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
*/

#ifndef DEFS_H_
#define DEFS_H_

/* system includes */
#include <stdint.h>
#include <stdbool.h>

/* Linux includes */
#include <net/ethernet.h>
#include <netinet/in.h>

/* dp includes */
#include <ezdp_search_defs.h>


/**********************************************************************************************************************************
 * Miscellaneous definitions
 **********************************************************************************************************************************/

extern 	bool     			cancel_frame_signal;

#define	__fast_path_code	__imem_1_cluster_func
#define	__slow_path_code	__imem_16_cluster_func

enum alvs_scheduler_type
{
	ALVS_ROUND_ROBIN_SCHEDULER						    = 0,
	ALVS_WEIGHTED_ROUND_ROBIN_SCHEDULER					= 1,
	ALVS_LEAST_CONNECTION_SCHEDULER						= 2,
	ALVS_WEIGHTED_LEAST_CONNECTION_SCHEDULER			= 3,
	ALVS_DESTINATION_HASH_SCHEDULER						= 4,
	ALVS_SOURCE_HASH_SCHEDULER							= 5,
	ALVS_LB_LEAST_CONNECTION_SCHEDULER					= 6,
	ALVS_LB_LEAST_CONNECTION_SCHEDULER_WITH_REPLICATION	= 7,
	ALVS_SHORTEST_EXPECTED_DELAY_SCHEDULER				= 8,
	ALVS_NEVER_QUEUE_SCHEDULER							= 9,
	ALVS_SCHEDULER_LAST									= 10
};

enum dp_path_type
{
	DP_PATH_SEND_TO_NW_NA  							    = 0,  //send to nw interface with no application logic - short circuit
	DP_PATH_SEND_TO_HOST_NA								= 1,  //send to host interface with no application logic - short circuit
	DP_PATH_SEND_TO_NW_APP								= 2,  //send to nw interface with application logic
	DP_PATH_SEND_TO_HOST_APP 							= 3,  //send to host interface with application logic
};

#define 	DP_PATH_NOT_VALID			4

#define 	ALVS_NETWORK_PORT_LOGICAL_ID	0  //need to define in configuration all network interfaces logical ID to 0 - temp solution
#define 	ALVS_HOST_OUTPUT_CHANNEL_ID		0 | 1<<8  //TODO - roee please update with real channel ID of host interface

#define 	ALVS_STATISTICS_MSID			7	//TODO - roee add to configuration - set which value you want on EMEM for posted stats
#define 	DP_NUM_COUNTERS_PER_INTERFACE	256

/**********************************************************************************************************************************
 * ALVS Search struct definitions - TODO auto  generate struct defs via CTOP gen
 **********************************************************************************************************************************/

enum alvs_struct_id
{
	ALVS_STRUCT_ID_INTERFACES			= 0,
	ALVS_STRUCT_ID_LAG					= 1,
	ALVS_STRUCT_ID_CONNECTIONS 			= 2,
	ALVS_STRUCT_ID_SERVICES 			= 3,
	ALVS_STRUCT_ID_SERVERS	 			= 4,
	ALVS_STRUCT_ID_FIB		 			= 5,
	ALVS_STRUCT_ID_ARP		 			= 6,
	ALVS_STRUCT_ID_LAST					= 7
};

/*********************************
 * Interfaces DB defs
 *********************************/

#define DP_INTERFACE_RESULT_SIZE	4

/*key*/
struct dp_interface_key
{
	uint8_t	logical_id;
};

/*result*/
struct  dp_interface_result
{
	union
	{
		uint32_t	raw_data[DP_INTERFACE_RESULT_SIZE];

		struct
		{
			/*byte0*/
			unsigned                      					 /*reserved*/       	 : EZDP_LOOKUP_PARITY_BITS_SIZE;
			unsigned                        				 /*reserved*/       	 : EZDP_LOOKUP_RESERVED_BITS_SIZE;
				/**< bits reserved for HW respond */
			unsigned										oper_status				 : 1;
			enum dp_path_type								path_type				 : 2;
			unsigned										is_vlan					 : 1;
			/*byte1*/
			uint8_t											lag_id						;
			/*byte2-3*/
			uint16_t										default_vlan				;
			/*byte4-9*/
			struct ether_addr         						mac_addres			     	;
			/*byte10-15*/
			uint8_t										 	rsrv[6];
		}__packed;
	};
};

CASSERT(sizeof(struct dp_interface_result) == DP_INTERFACE_RESULT_SIZE*sizeof(uint32_t));

/*********************************
 * Services DB defs
 *********************************/

#define ALVS_SERVICE_RESULT_SIZE	8	//words
#define ALVS_SERVICE_KEY_SIZE		7   //bytes

/*key*/
struct alvs_service_key
{
	in_addr_t	service_address;
	uint16_t	service_port;
	uint8_t		service_protocol;
}__packed;

CASSERT(sizeof(struct alvs_service_key) == ALVS_SERVICE_KEY_SIZE);

/*result*/
struct  alvs_service_result
{
	union
	{
		uint32_t	raw_data[ALVS_SERVICE_RESULT_SIZE];

		struct
		{
			/*byte0*/
			unsigned                      					 /*reserved*/       	 : EZDP_LOOKUP_PARITY_BITS_SIZE;
			unsigned                        				 /*reserved*/       	 : EZDP_LOOKUP_RESERVED_BITS_SIZE;
				/**< bits reserved for HW respond */
			unsigned										 /* reserved */			 : 4;
			/*byte1*/
			unsigned										 /* reserved */			 : 8;
			/*byte2-3*/
			uint16_t										service_id					;
			/*byte4*/
			enum alvs_scheduler_type						sched_type					;
			/*byte5-7*/
			unsigned										rs_head_index			 : 24; //pointer/index to head of attached list of real-servers(rs)
			/*byte8-11*/
			struct ezdp_sum_addr         					sched_data			     	; //sched data pointer
			/*byte12-15*/
			in_addr_t										real_server_ip				; //TEMP field for POC only!
		}__packed;
	};
};

CASSERT(sizeof(struct alvs_service_result) == ALVS_SERVICE_RESULT_SIZE*sizeof(uint32_t));

/*********************************
 * arp DB defs
 *********************************/

#define ALVS_ARP_RESULT_SIZE	2	//words
#define ALVS_ARP_KEY_SIZE		4   //bytes

/*key*/
struct alvs_arp_key
{
	in_addr_t	real_server_address;
};

CASSERT(sizeof(struct alvs_arp_key) == ALVS_ARP_KEY_SIZE);

/*result*/
struct  alvs_arp_result
{
	union
	{
		uint32_t	raw_data[ALVS_ARP_RESULT_SIZE];

		struct
		{
			/*byte0*/
			unsigned                      					 /*reserved*/       	 : EZDP_LOOKUP_PARITY_BITS_SIZE;
			unsigned                        				 /*reserved*/       	 : EZDP_LOOKUP_RESERVED_BITS_SIZE;
				/**< bits reserved for HW respond */
			unsigned										 /* reserved */			 : 4;
			/*byte1*/
			uint8_t											output_logical_id			;
			/*byte2-7*/
			struct ether_addr         						dest_mac_addr		     	;
		}__packed;
	};
};

CASSERT(sizeof(struct alvs_arp_result) == ALVS_ARP_RESULT_SIZE*sizeof(uint32_t));

/**********************************************************************************************************************************
 * ALVS statistics definitions
 **********************************************************************************************************************************/




#endif /* DEFS_H_ */
