/***************************************************************************
*
*						  Copyright by EZchip Technologies, 2016
*
*
*  Project:	 	NPS400 ALVS application
*  File:		nps_parse.h
*  Desc:		generic packet parse and validation
*
****************************************************************************/

#ifndef NPS_PARSE_H_
#define NPS_PARSE_H_

#include "alvs_classifier.h"
#include "alvs_interface.h"

/******************************************************************************
 * \brief	  Parse and validate frame
 * \return	  void
 */
static __always_inline
void parse_and_validate_frame(int32_t logical_id)
{
	uint8_t	*frame_base;

	if (alvs_check_ingress_interface(logical_id) == ALVS_NETWORK_PATH)
	{
	#ifndef ALVS_KETSER
		uint8_t* next_et;

		/* === Load Data of first frame buffer === */
		frame_base = ezframe_load_buf(&cmem.frame, cmem.frame_data, NULL, 0);

		/* decode mac to ensure it is valid */
		ezdp_decode_mac(frame_base,
						MAX_DECODE_SIZE,
						&cmem.mac_decode_result);

		/*in case of any error send frame to host*/
		if (unlikely(cmem.mac_decode_result.error_codes.decode_error))
		{
			send_frame_to_host(ALVS_PACKET_MAC_ERROR);
			return;
		}

		/*check if my_mac is set*/
		if (unlikely(!cmem.mac_decode_result.control.my_mac))
		{
			send_frame_to_host(ALVS_PACKET_NOT_MY_MAC);
			return;
		}

		/*skip vlan tags and check next ethertype*/
		next_et = frame_base + sizeof(struct ether_addr) + sizeof(struct ether_addr); //skip l2 addr

		//check if vlan and then skip
		if (cmem.mac_decode_result.number_of_tags)
		{
			next_et += (4 << cmem.mac_decode_result.number_of_tags);
		}

		/*check if ipv4 frame*/
		if (*((uint16_t*)next_et) != ETHERTYPE_IP)
		{
			send_frame_to_host(ALVS_PACKET_NOT_IPV4);
			return;
		}

		struct iphdr  *ip_ptr = (struct iphdr*)(next_et + sizeof(uint16_t));

		if (ip_ptr->protocol != IPPROTO_TCP && ip_ptr->protocol != IPPROTO_UDP)
		{
			send_frame_to_host(ALVS_PACKET_NOT_UDP_AND_TCP);
			return;
		}

		/*skip L2 and validate IP is ok*/
		ezdp_decode_ipv4((uint8_t*)ip_ptr,
						 sizeof(struct iphdr),
						 cmem.frame.job_desc.frame_desc.frame_length,
						 &cmem.ipv4_decode_result);

		/*in case of any error send frame to host*/
		if (unlikely(cmem.ipv4_decode_result.error_codes.decode_error))
		{
			send_frame_to_host(ALVS_PACKET_IPV4_ERROR);
			return;
		}

		/*frame is OK, let's start classification*/
		alvs_service_classification(frame_base, ip_ptr);
	#else
		send_frame_to_host(ALVS_CAUSE_ID_LAST);
		return;
	#endif
	}
	else /*HOST PATH*/
	{
		/*currently send frame to network without any change or any other operations*/
		send_frame_to_network();
	}
}

#endif /* NPS_PARSE_H_ */
