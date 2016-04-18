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

#ifndef ALVS_DP_DEFS_H_
#define ALVS_DP_DEFS_H_

#define MAX_DECODE_SIZE		28

enum alvs_to_host_cause_id
{
	ALVS_EZFRAME_VALIDATION_FAIL			= 0,
	ALVS_PACKET_MAC_ERROR					= 1,
	ALVS_PACKET_IPV4_ERROR 					= 2,
	ALVS_PACKET_NOT_MY_MAC 					= 3,
	ALVS_PACKET_NOT_IPV4 					= 4,
	ALVS_PACKET_NOT_UDP_AND_TCP 			= 5,
	ALVS_PACKET_FAIL_CLASSIFICATION			= 6,
	ALVS_PACKET_NO_VALID_ROUTE				= 7,
	ALVS_PACKET_FAIL_ARP					= 8,
	ALVS_PACKET_FAIL_INTERFACE_LOOKUP		= 9,
	ALVS_CAUSE_ID_LAST
};


#endif /* ALVS_DP_DEFS_H_ */
