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

const char *nw_if_posted_stats_offsets_names[] = {
	"FRAME_VALIDATION_FAIL",		/* 0 */
	"MAC_ERROR",				/* 1 */
	"IPV4_ERROR",				/* 2 */
	"NOT_MY_MAC",				/* 3 */
	"NOT_IPV4",				/* 4 */
	"NOT_TCP",				/* 5 */
	"NO_VALID_ROUTE",			/* 6 */
	"FAIL_ARP_LOOKUP",			/* 7 */
	"FAIL_INTERFACE_LOOKUP",		/* 8 */
	"FAIL_FIB_LOOKUP",			/* 9 */
	"REJECT_BY_FIB",			/* 10 */
	"UNKNOWN_FIB_RESULT",			/* 11 */
	"FAIL_STORE_BUF",			/* 12 */
	"FAIL_GET_MAC_ADDR",			/* 13 */
	"FAIL_SET_ETH_HEADER",			/* 14 */
	"FAIL_LAG_GROUP_LOOKUP",		/* 15 */
	"DISABLE_LAG_GROUP_DROPS",		/* 16 */
	"DISABLE_IF_EGRESS_DROPS",		/* 17 */
	"DISABLE_IF_INGRESS_DROPS",		/* 18 */
	"",					/* 19 */
};

const char *remote_if_posted_stats_offsets_names[] = {
	"FRAME_VALIDATION_FAIL",		/* 0 */
	"MAC_ERROR",				/* 1 */
	"IPV4_ERROR",				/* 2 */
	"NOT_MY_MAC",				/* 3 */
	"NOT_IPV4",				/* 4 */
	"NOT_TCP",				/* 5 */
	"NO_VALID_ROUTE",			/* 6 */
	"FAIL_ARP_LOOKUP",			/* 7 */
	"FAIL_INTERFACE_LOOKUP",		/* 8 */
	"FAIL_FIB_LOOKUP",			/* 9 */
	"REJECT_BY_FIB",			/* 10 */
	"UNKNOWN_FIB_RESULT",			/* 11 */
	"FAIL_STORE_BUF",			/* 12 */
	"",					/* 13 */
	"",					/* 14 */
	"",					/* 15 */
	"",					/* 16 */
	"",					/* 17 */
	"",					/* 18 */
	"",					/* 19 */
	"",					/* 20 */
};

const char *alvs_error_stats_offsets_names[] = {
	"UNSUPPORTED_ROUTING_ALGO",		/* 0 */
	"CANT_EXPIRE_CONNECTION",		/* 1 */
	"CANT_UPDATE_CONNECTION_STATE",		/* 2 */
	"CONN_INFO_LKUP_FAIL",			/* 3 */
	"CONN_CLASS_ALLOC_FAIL",		/* 4 */
	"CONN_INFO_ALLOC_FAIL",			/* 5 */
	"CONN_INDEX_ALLOC_FAIL",		/* 6 */
	"SERVICE_CLASS_LKUP_FAIL",		/* 7 */
	"SCHEDULING_FAIL",			/* 8 */
	"SERVER_INFO_LKUP_FAIL",		/* 9 */
	"SERVER_IS_UNAVAILABLE",		/* 10 */
	"SERVER_INDEX_LKUP_FAIL",		/* 11 */
	"CONN_CLASS_LKUP_FAIL",			/* 12 */
	"SERVICE_INFO_LOOKUP",			/* 13 */
	"UNSUPPORTED_SCHED_ALGO",		/* 14 */
	"CANT_MARK_DELETE",			/* 15 */
	"DEST_SERVER_IS_NOT_AVAIL",		/* 16 */
	"SEND_FRAME_FAIL",			/* 17 */
	"CONN_MARK_TO_DELETE",			/* 18 */
	"SERVICE_CLASS_LOOKUP",			/* 19 */
	"UNSUPPORTED_PROTOCOL",			/* 20 */
	"NO_ACTIVE_SERVERS",			/* 21 */
	"CREATE_CONN_MEM_ERROR",		/* 22 */
	"STATE_SYNC",				/* 23 */
	"STATE_SYNC_LOOKUP_FAIL",		/* 24 */
	"STATE_SYNC_BACKUP_DOWN",		/* 25 */
	"STATE_SYNC_BAD_HEADER_SIZE",		/* 26 */
	"STATE_SYNC_BACKUP_NOT_MY_SYNCID",	/* 27 */
	"STATE_SYNC_BAD_BUFFER",		/* 28 */
	"STATE_SYNC_DECODE_CONN",		/* 29 */
	"STATE_SYNC_BAD_MESSAGE_VERSION",	/* 30 */
	"",					/* 31 */
	"",					/* 32 */
	"",					/* 33 */
	"",					/* 34 */
	"",					/* 35 */
	"",					/* 36 */
	"",					/* 37 */
	"",					/* 38 */
	"",					/* 39 */
	"",					/* 40 */
};

