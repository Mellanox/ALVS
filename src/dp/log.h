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
*	file - log.h
*	description - contains definitions for log.c
*/

#ifndef LOG_H_
#define LOG_H_

#include <stdint.h>
#include <netinet/in.h>
#include <net/ethernet.h>
#include "syslog.h"
#include <ezdp.h>
#include <ezframe.h>

#define MIN(x, y) (((x) <= (y)) ? (x) : (y))

#define SYSLOG_UDP_SERVER_PORT          514
#define SYSLOG_TEMPLATE_HOST_NAME_SIZE  20
#define SYSLOG_PRI_FACILITY_STRING_SIZE 4 /*TODO - for now its hardcoded. should be changed*/
#define SYSLOG_APPLIC_NAME_STRING_SIZE  10
#define SYSLOG_BUF_DATA_SIZE            64
#define SYSLOG_FIRST_BUFFER_SIZE        32
#define SYSLOG_MAX_NUM_OF_BUF           3
#define SYSLOG_BUF_HEADROOM             64
#define SYSLOG_CPU_STRING_SIZE          6
#define LOG_MSG_SIZE_FOR_TB             100

int syslog_str_size;
char syslog_str[EZFRAME_BUF_DATA_SIZE];

#ifndef NDEBUG
#define write_log_macro(priority, wa, wa_size, str, ...) { \
		syslog_str_size = snprintf(syslog_str, EZFRAME_BUF_DATA_SIZE, str "[FILE: %s:%d FUNC: %s]", ##__VA_ARGS__, __FILE__, __LINE__, __func__); \
		write_log(priority, syslog_str, syslog_str_size, wa, wa_size); \
}
#else
#define LOGMASK  LOG_UPTO(LOG_INFO)
#define write_log_macro(priority, wa, wa_size, str, ...) { \
	if (LOG_MASK(priority) & LOGMASK) { \
		syslog_str_size = snprintf(syslog_str, EZFRAME_BUF_DATA_SIZE, str, ##__VA_ARGS__); \
		write_log(priority, syslog_str, syslog_str_size, wa, wa_size); \
	} \
}
#endif


struct syslog_frame_info {
	ezframe_t   frame;
	char        frame_data[SYSLOG_BUF_DATA_SIZE];
};

struct syslog_wa_info {
	union {
		struct syslog_frame_info frame_info;
		uint64_t tb_color_flag_counter;
		struct ezdp_tb_ctr_result tb_ctr_result;
	};
	ezdp_spinlock_t mutex;
};

typedef int (*send_cb)(ezframe_t *);

struct syslog_info {
	char            applic_name[SYSLOG_APPLIC_NAME_STRING_SIZE];
	int             applic_name_size;
	send_cb         send_cb;
	uint16_t        dest_port;
	in_addr_t       dest_ip;
	in_addr_t       src_ip;
	int             remained_length_for_user_string;
};
/*****************************************************************************/
/*! \fn void write_log(int priority, char *str,int length,
 *                     char * __cmem syslog_wa, int syslog_wa_size);
 *
 * \brief send syslog frame to SYSLOG server.
 * known restrictions : num_of_buffers in frame <= 3
 *                      the first buffer consists only header
 *                      (20(IPV4) + 8(UDP) + 14 Ethernet
 * \param[in]    priority - priority of the user message
 *               str - user string
 *               length - length of the user string
 *               syslog_wa - WA in CMEM
 *               syslog_wa_size - WA size in CMEM
 * \return none.
 */
void write_log(int priority, char *str, int length,
	       void __cmem * syslog_wa,
	       unsigned int syslog_wa_size);
/*****************************************************************************/
/*! \fn void open_log(char *applic_name, int applic_name_lengh,
 *                       in_addr_t dest_ip, in_addr_t src_ip,
 *                       uint16_t dest_udp,
 *                       send_cb send_cb)
 * \brief Open the connection to syslog utility in DP.
 * \param[in]   user_syslog_info - the data strucuture from the type syslog_info
 *                                 which contains application parameters
 *                                 for syslog
 * \return TRUE - on SUCCESS, FALSE - otherwise.
 */
bool open_log(struct syslog_info *user_syslog_info);

#endif /* LOG_H_ */
