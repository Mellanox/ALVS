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
*
*  Project:             NPS400 ALVS application
*  File:                application_search_defs.h
*  Desc:                application_search_defs API.
*
*/

#ifndef APPLICATION_SEARCH_DEFS_H_
#define APPLICATION_SEARCH_DEFS_H_

/*********************************
 * includes & defines
 *********************************/

#include "alvs_search_defs.h"

#define APPLICATION_INFO_MAX_ENTRIES	16
#define ALVS_APPLICATION_INFO_INDEX	0


/*********************************
 * application info DB defs
 *********************************/

/*key*/
struct application_info_key {
	uint8_t application_index;
} __packed;

CASSERT(sizeof(struct application_info_key) == 1);


/*result*/
struct application_info {
#ifdef NPS_BIG_ENDIAN
	unsigned	/*reserved*/  : EZDP_LOOKUP_PARITY_BITS_SIZE;
	unsigned	/*reserved*/  : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned	/*reserved*/ : 4;
#else
	unsigned	/*reserved*/ : 4;
	unsigned	/*reserved*/ : EZDP_LOOKUP_RESERVED_BITS_SIZE;
	unsigned	/*reserved*/ : EZDP_LOOKUP_PARITY_BITS_SIZE;
#endif
	/*byte1-15*/
	uint8_t		reserverd[15];
};

union application_info_result {
	struct alvs_app_info_result	alvs_app;
	struct application_info		app_info;
};

CASSERT(sizeof(union application_info_result) == 16);


#endif /* APPLICATION_SEARCH_DEFS_H_ */
