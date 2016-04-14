/***************************************************************************
*
*						  Copyright by EZchip Technologies, 2016
*
*
*  Project:	 	NPS400 ALVS application
*  File:		defs.h
*  Desc:		Common (cp & dp) defs Header file for ALVS application
*
*
****************************************************************************/

#ifndef DEFS_H_
#define DEFS_H_

/* system includes */
#include <stdint.h>
#include <stdbool.h>

/* dp includes */
#include <ezdp_search_defs.h>


/**********************************************************************************************************************************
 * Miscellaneous definitions
 **********************************************************************************************************************************/

extern 	bool     			cancel_frame_signal;

#define	__fast_path_code	__imem_1_cluster_func
#define	__slow_path_code	__imem_16_cluster_func

#define MAX_DECODE_SIZE		28

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

#define 	ALVS_NETWORK_PORT_LOGICAL_ID	0  //need to define in configuration all network interfaces logical ID to 0 - temp solution
#define 	ALVS_HOST_CHANNEL_ID			0 | 1<<8  //TODO - roee please update with real channel ID of host interface

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

#define ALVS_INTERFACE_RESULT_SIZE	4

/*key*/
struct alvs_interface_key
{
	uint8_t	logical_id;
};

/*result*/
struct  alvs_interface_result
{
	union
	{
		uint32_t	raw_data[ALVS_INTERFACE_RESULT_SIZE];

		struct
		{
			/*byte0*/
			unsigned                      					 /*reserved*/       	 : EZDP_LOOKUP_PARITY_BITS_SIZE;
			unsigned                        				 /*reserved*/       	 : EZDP_LOOKUP_RESERVED_BITS_SIZE;
				/**< bits reserved for HW respond */
			unsigned										oper_status				 : 1;
			unsigned										 /* reserved */			 : 3;
			/*byte1*/
			uint8_t											lag_id						;
			/*byte2*/
			unsigned										 /* reserved */			 : 8;
			/*byte3*/
			unsigned										 /* reserved */			 : 8;
			/*byte4-9*/
			struct ether_addr         						mac_addres			     	;
			/*byte10-15*/
			uint8_t										 	rsrv[6];
		}__packed;
	};
};

CASSERT(sizeof(struct alvs_interface_result) == ALVS_INTERFACE_RESULT_SIZE*sizeof(uint32_t));

/*********************************
 * Services DB defs
 *********************************/

#define ALVS_SERVICES_RESULT_SIZE	8	//words
#define ALVS_SERVICES_KEY_SIZE		7   //bytes

/*key*/
struct alvs_service_key
{
	in_addr_t	service_address;
	uint16_t	service_port;
	uint8_t		service_protocol;
}__packed;

CASSERT(sizeof(struct alvs_service_key) == ALVS_SERVICES_KEY_SIZE);

/*result*/
struct  alvs_services_result
{
	union
	{
		uint32_t	raw_data[ALVS_SERVICES_RESULT_SIZE];

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

CASSERT(sizeof(struct alvs_services_result) == ALVS_SERVICES_RESULT_SIZE*sizeof(uint32_t));

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
			unsigned										 /* reserved */			 : 8;
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
