/***************************************************************************
*
*						  Copyright by EZchip Technologies, 2016
*
*
*  Project:	 	NPS400 ALVS application
*  File:		alvs_interface.h
*  Desc:		handle interface operation including LAG
*
****************************************************************************/

#ifndef ALVS_INTERFACE_H_
#define ALVS_INTERFACE_H_

/******************************************************************************
 * \brief	  check if packet arrive from network port or host port
 * \return	  void
 */
static __always_inline
uint32_t alvs_check_ingress_interface(int32_t logical_id)
{
	if (logical_id == ALVS_NETWORK_PORT_LOGICAL_ID)
	{
		return ALVS_HOST_PATH;
	}
	else
	{
		return ALVS_NETWORK_PATH;
	}
}

#endif /* ALVS_INTERFACE_H_ */
