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
#include "syslog.h"
#ifndef NDEBUG
#define LOG_LEVEL LOG_DEBUG
#else
#define LOG_LEVEL LOG_INFO
#endif

/*****************************************************************************/
/*! \fn void write_log(char *s)
 * \brief write the message to syslog utility
 * \param[in] priority - indicator to the severity level of the message
 * \param[in] s - pointer to the message which is enetred to syslog logging file
 * \return none.
 */
#ifndef NDEBUG
#define write_log(priority, s) \
	syslog(LOG_MAKEPRI(LOG_USER, priority), \
			"%s [FILE: %s:%d FUNC: %s]",\
			s, __FILE__, __LINE__, __func__)
#else
#define write_log(priority, s) \
	syslog(LOG_MAKEPRI(LOG_USER, priority), "%s", s)
#endif


/*****************************************************************************/
/*! \fn void open_log(char *s)
 * \brief Open the connection to syslog utility.
 * \param[in] s -  an arbitrary identification string which future syslog
 *					invocations will prefix to each message
 * \return none.
 */
void open_log(char *s);

/*****************************************************************************/
/*! \fn close_log()
 * \brief close the connection to syslog utility
 * \param[in] s  none
 * \return none.
 */
void close_log(void);

#endif /* LOG_H_ */
