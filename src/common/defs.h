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

#ifndef DEFS_H_
#define DEFS_H_

/* Data MSIDs */
#define HALF_CLUSTER_DATA_MSID        0x2
#define X1_CLUSTER_DATA_MSID          0x4
#define X2_CLUSTER_DATA_MSID          0x6
#define X4_CLUSTER_DATA_MSID          0x8
#define X16_CLUSTER_DATA_MSID         0xa
#define ALL_CLUSTER_DATA_MSID         0xc
#define EMEM_DATA_NO_ECC_MSID         0x0
#define EMEM_DATA_IN_BAND_MSID        0x1
#define EMEM_DATA_OUT_OF_BAND_MSID    0x2

/* Statistics MSIDs */
#define EMEM_STATISTICS_POSTED_MSID   0x3

/* Search MSIDs */
#define EMEM_SEARCH_MSID              0x4

enum struct_id {
	STRUCT_ID_NW_INTERFACES            = 0,
	STRUCT_ID_NW_LAG                   = 1,
	STRUCT_ID_ALVS_CONNECTIONS         = 2,
	STRUCT_ID_ALVS_SERVICES            = 3,
	STRUCT_ID_ALVS_SERVERS             = 4,
	STRUCT_ID_NW_FIB                   = 5,
	STRUCT_ID_NW_ARP                   = 6,
	NUM_OF_STRUCT_IDS
};

#endif /* DEFS_H_ */
