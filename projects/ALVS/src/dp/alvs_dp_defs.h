/***************************************************************************
*
*						  Copyright by EZchip Technologies, 2016
*
*
*  Project:	 	NPS400 ALVS application
*  File:		alvs_dp_defs.h
*  Desc:		alvs dp related definitions
*
****************************************************************************/

#ifndef ALVS_DP_DEFS_H_
#define ALVS_DP_DEFS_H_

#define ALVS_HOST_PATH		0
#define ALVS_NETWORK_PATH	1

enum alvs_to_host_cause_id
{
	ALVS_EZFRAME_VALIDATION_FAIL			= 0,
	ALVS_PACKET_MAC_ERROR					= 1,
	ALVS_PACKET_IPV4_ERROR 					= 2,
	ALVS_PACKET_NOT_MY_MAC 					= 3,
	ALVS_PACKET_NOT_IPV4 					= 4,
	ALVS_PACKET_NOT_UDP_AND_TCP 			= 5,
	ALVS_PACKET_FAIL_CLASSIFICATION			= 6,
	ALVS_CAUSE_ID_LAST
};


#endif /* ALVS_DP_DEFS_H_ */
