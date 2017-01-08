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
*
*  Project:             NPS400 ALVS application
*  File:                global_defs.h
*  Desc:                global definitions
*/

#ifndef GLOBAL_DEFS_H_
#define GLOBAL_DEFS_H_

#include "nw_defs.h"
#include "alvs_defs.h"

#define ANL_LOG2(x) (x == 2 ? 1 : x == 4 ? 2 : x == 8 ? 3 : x == 16 ? 4 : x == 32 ? 5 : \
			x == 64 ? 6 : x == 128 ? 7 : x == 256 ? 8 : x == 512 ? 9 : x == 1024 ? 10 : x == 2048 ? 11 : \
			x == 4096 ? 12 : x == 8192 ? 13 : x == 16384 ? 14 : x == 32768 ? 15 : x == 65536 ? 16 : \
			x == 131072 ? 17 : x == 262144 ? 18 : 0)

/* TODO take from netinet/ip.h after fixing includes */
#define IP_DF           0x4000
struct net_hdr {
	struct iphdr ipv4;
	struct udphdr udp;
} __packed;


union cmem_workarea {
#ifdef CONFIG_ALVS
	union alvs_workarea   alvs_wa;
#endif
	union nw_workarea     nw_wa;
};

#ifdef CONFIG_ALVS
extern struct alvs_cmem         cmem_alvs;
extern struct alvs_shared_cmem  shared_cmem_alvs;
#endif

extern struct syslog_wa_info    syslog_work_area;
extern union cmem_workarea      cmem_wa;
extern ezframe_t                frame;
extern uint8_t                  frame_data[EZFRAME_BUF_DATA_SIZE];
extern struct packet_meta_data  packet_meta_data;


#endif  /*GLOBAL_DEFS_H_*/
