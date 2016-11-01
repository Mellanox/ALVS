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
*  File:                log.c
*  Desc:                performs logging functionality for CP via SYSlog.
*
*/

#include "log.h"

void open_log(char *s)
{
	setlogmask(LOG_UPTO(LOG_LEVEL));

	int facility = LOG_DAEMON;
	int option = LOG_CONS | LOG_PID | LOG_NDELAY;

	openlog(s, option, facility);

	EZlog_SetFileName("/var/log/anl_ezcp_log");
	EZlog_OpenLogFile();
	EZlog_SetMaximalLogSize(LOG_MAX_SIZE);

	EZlog_SetLog(EZlog_OUTPUT_FILE, EZlog_COMP,
		     EZlog_SUB_COMP, EZlog_LEVEL);
}

void close_log(void)
{
	EZlog_SetLog(EZlog_OUTPUT_FILE, EZlog_COMP_ALL,
		     EZlog_SUB_COMP_LOG_ALL, EZlog_LEVEL_FATAL);
	EZlog_CloseLogFile();

	closelog();
}


