/***************************************************************************
*
*						  Copyright by EZchip Technologies, 2012
*
*
*  Project:	 	NPS400 demo dp_dp application
*  File:		defs.h
*  Desc:		Header file for demo dp_dp application definitions
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

/***********************************************
 * Search struct definitions
 ************************************************/

enum
{
	SEARCH_STRUCT_ID_FORWARDING			= 0,
	SEARCH_STRUCT_ID_LAST				= 1

} search_struct_id;



/*! search_forwarding_key struct for demo dp_dp application  */
struct search_forwarding_key
{
	uint8_t									source_port;
		/**< Source Port Id */

	uint16_t								vlan;
		/**< C VLAN ID from the packet */

} __packed;



#define SEARCH_FORWARDING_RESULT_RESERVED4_31_SIZE								28
#define SEARCH_FORWARDING_RESULT_VLAN_SIZE										12
#define SEARCH_FORWARDING_RESULT_NEXT_DEST_SIZE									20
#define SEARCH_FORWARDING_RESULT_WORD_COUNT										2

/*! search_forwarding_result struct for demo dp_dp */
struct  search_forwarding_result
{
	union
	{
		uint32_t											 raw_data[SEARCH_FORWARDING_RESULT_WORD_COUNT];

		struct
		{
			unsigned                      					 /*reserved*/       	: EZDP_LOOKUP_PARITY_BITS_SIZE;
			unsigned                        				 /*reserved*/       	: EZDP_LOOKUP_RESERVED_BITS_SIZE;
				/**< bits reserved for HW respond */
			unsigned										 /* reserved4_31 */		:	SEARCH_FORWARDING_RESULT_RESERVED4_31_SIZE;
				/**< Reserved bits 4 to 31 */
			unsigned										 vlan					:	SEARCH_FORWARDING_RESULT_VLAN_SIZE;
				/**< C VLAN ID to add to the packet */
			unsigned         								next_destination		:	SEARCH_FORWARDING_RESULT_NEXT_DEST_SIZE;
				/**< Destination ID - TM Flow ID */

		};
	};
};


struct search_forwarding_entry
{
	struct search_forwarding_result				     result;
	EZDP_PAD_HASH_ENTRY(sizeof(struct search_forwarding_result),sizeof(struct search_forwarding_key));
};


/*****************************************************/

#define DEFAULT_VLAN_ID						0
#define DEFAULT_PRIORITY					1
#define MAX_DECODE_SIZED					28

extern bool                      cancel_frame_signal;


#define			__fast_path_code		__imem_1_cluster_func
#define			__slow_path_code		__imem_16_cluster_func

#define	  VLAN_HDR_PRIORITY_SIZE								3
#define	  VLAN_HDR_CFI_SIZE										1
#define	  VLAN_HDR_VLAN_ID_SIZE									12

/*! VLAN tag format struct definition - VLAN TCI (2 bytes) */
struct vlan_tag
{
		unsigned									  priority				:	VLAN_HDR_PRIORITY_SIZE;
			/**< A 3-bit field which refers to the IEEE 802.1p priority. It indicates the frame priority level. */
		unsigned									  cfi					:	VLAN_HDR_CFI_SIZE;
			/**<  A 1-bit field. If the value of this field is 1, the MAC address is in non-canonical format.
				  If the value is 0, the MAC address is in canonical format. */
		unsigned									 vlan_id			 	:	VLAN_HDR_VLAN_ID_SIZE;
			/**< A 12-bit field specifying the VLAN to which the frame belongs. */
} __packed ;

#endif /* DEFS_H_ */
