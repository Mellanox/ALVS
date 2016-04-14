/***************************************************************************
*
*						  Copyright by EZchip Technologies, 2016
*
*
*  Project:	 	NPS400 ALVS application
*  File:		nps_routing.h
*  Desc:		route frames to their destination target
*
****************************************************************************/

#ifndef NPS_ROUTING_H_
#define NPS_ROUTING_H_

/******************************************************************************
 * \brief	  perform arp lookup and modify l2 header before transmission
 * \return	  void
 */
static __always_inline
void arp_handler(uint8_t* frame_base, in_addr_t	dest_ip)
{
	 uint32_t						rc;
	 uint32_t						found_result_size;
	 struct  alvs_arp_result   		*arp_res_ptr;


	 cmem.arp_key.real_server_address 	= dest_ip;

	 rc = ezdp_lookup_hash_entry(&shared_cmem.arp_struct_desc,
								 (void*)&cmem.arp_key,
								 sizeof(struct alvs_arp_key),
								 (void **)&arp_res_ptr,
								 &found_result_size,
								 0,
								 cmem.arp_hash_wa,
								 sizeof(cmem.arp_hash_wa));

	if (rc == 0)
	{
		struct ether_addr *dmac = (struct ether_addr *)frame_base;
		ezdp_mem_copy(dmac, arp_res_ptr->dest_mac_addr, sizeof(struct ether_addr));
		send_frame_to_network();
	}
	else
	{
		send_frame_to_host(ALVS_PACKET_FAIL_CLASSIFICATION);
		return;
	}
}

#endif /* NPS_ROUTING_H_ */
