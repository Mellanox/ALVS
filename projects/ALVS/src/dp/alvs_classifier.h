/***************************************************************************
*
*						  Copyright by EZchip Technologies, 2016
*
*
*  Project:	 	NPS400 ALVS application
*  File:		alvs_classifier.h
*  Desc:		perform service/connection classification on incoming packet
*
****************************************************************************/

#ifndef ALVS_CLASSIFIER_H_
#define ALVS_CLASSIFIER_H_

/******************************************************************************
 * \brief	  do service classification  - 3 tuple - DIP, dest port and IP protocol
 * \return	  void
 */
static __always_inline
void alvs_service_classification(uint8_t* frame_base, struct iphdr  *ip_hdr)
{
	 uint32_t						rc;
	 uint32_t						found_result_size;
	 struct  alvs_services_result   *service_res_ptr;
	 struct tcphdr tcp_hdr = (struct tcphdr*)((uint8_t*)ip_hdr + sizeof(struct iphdr));

	 cmem.service_key.service_address 	= ip_hdr->daddr;
	 cmem.service_key.service_protocol 	= ip_hdr->protocol;
	 cmem.service_key.service_port	  	= tcp_hdr->dest;

	 rc = ezdp_lookup_hash_entry(&shared_cmem.services_struct_desc,
								 (void*)&cmem.service_key,
								 sizeof(struct alvs_service_key),
								 (void **)&service_res_ptr,
								 &found_result_size,
								 0,
								 cmem.service_hash_wa,
								 sizeof(cmem.service_hash_wa));

	if (rc == 0)
	{
		arp_handler(frame_base, service_res_ptr->real_server_ip);
	}
	else
	{
		send_frame_to_host(ALVS_PACKET_FAIL_CLASSIFICATION);
		return;
	}
}

#endif /* ALVS_CLASSIFIER_H_ */
