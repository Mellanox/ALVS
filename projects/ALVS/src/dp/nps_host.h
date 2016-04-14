/***************************************************************************
*
*						  Copyright by EZchip Technologies, 2016
*
*
*  Project:	 	NPS400 ALVS application
*  File:		nps_host.h
*  Desc:		handle frames sent to host
*
****************************************************************************/

#ifndef NPS_HOST_H_
#define NPS_HOST_H_

/******************************************************************************
 * \brief	  send frames to host
 * \return	  void
 */
static __always_inline
void send_frame_to_host(enum alvs_to_host_cause_id cause_id)
{
	printf("frame send to host. Cause ID = %d\n", cause_id);
	ezframe_send_to_if(	&cmem.frame,
						ALVS_HOST_CHANNEL_ID,
						0);
}

/******************************************************************************
 * \brief	  send frames to network ports
 * \return	  void
 */
static __always_inline
void send_frame_to_network(void)
{
	ezframe_send_to_if(	&cmem.frame,
						cmem.mac_decode_result.da_sa_hash & 0x3,
						0);
}
#endif /* NPS_HOST_H_ */
