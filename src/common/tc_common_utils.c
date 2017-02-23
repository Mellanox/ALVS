/***************************************************************************
*
* Copyright (c) 2016 Mellanox Technologies, Ltd. All rights reserved.
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
*  Project:             NPS400 ATC application
*  File:                tc_common_utils.c
*  Desc:                common functions utils for ATC
*
****************************************************************************/

#include <string.h>
#include <stdio.h>
#include <linux/pkt_cls.h>

#include "tc_common_defs.h"

extern int action_a2n(char *arg, int *result);

/******************************************************************************
 * \brief      get action string name
 *
 * \param[in]  type   - TC filter type
 * \param[in]  buf    - WA buffer. size should be greater than 32 Bytes
 * \param[in]  len    - buffer length
 *
 * \return     pointer to string - filter type
 */
char *tc_get_filter_type_str(enum tc_filter_type  type,
			     char                *buf,
			     int                  len)
{
	if (type == TC_FILTER_FLOWER) {
		return "flower";
	} else if (type == TC_FILTER_U32) {
		return  "u32";
	} else if (type == TC_FILTER_FW) {
		return "fw";
	}

	/* else */
	snprintf(buf, len, "[%d]", type);
	return buf;
}


/******************************************************************************
 * \brief      get action family string name
 *
 * \param[in]  type   - action type
 * \param[in]  buf    - WA buffer. size should be greater than 32 Bytes
 * \param[in]  len    - buffer length
 *
 * \return     pointer to string - family name
 */
char *tc_get_action_family_str(enum tc_action_type  type,
			       char                *buf,
			       int                  len)
{
	uint32_t family_type;

	family_type = (type & TC_ACTION_FAMILY_MASK);
	if (family_type == TC_ACTION_TYPE_GACT_FAMILY) {
		return "gact";
	} else if (family_type == TC_ACTION_TYPE_MIRRED_FAMILY) {
		return  "mirred";
	} else if (family_type == TC_ACTION_TYPE_PEDIT_FAMILY) {
		return "pedit";
	} else if (family_type == TC_ACTION_TYPE_OTHER_FAMILY) {
		return "other";
	}

	/* else */
	snprintf(buf, len, "[%d]", type);
	return buf;

}


/******************************************************************************
 * \brief      get action string name
 *
 * \param[in]  type   - action type
 * \param[in]  buf    - WA buffer. size should be greater than 32 Bytes
 * \param[in]  len    - buffer length
 *
 * \return     pointer to string - family name
 */
char *tc_get_action_str(enum tc_action_type  type,
			char                *buf,
			int                  len)
{
	if (type == TC_ACTION_TYPE_GACT_DROP) {
		return "drop";
	} else if (type == TC_ACTION_TYPE_GACT_OK) {
		return  "pass";
	} else if (type == TC_ACTION_TYPE_GACT_PIPE) {
		return "pipe";
	} else if (type == TC_ACTION_TYPE_GACT_CONTINUE) {
		return "continue";
	} else if (type == TC_ACTION_TYPE_MIRRED_EGR_REDIRECT) {
		return "egress redirect";
	} else if (type == TC_ACTION_TYPE_MIRRED_EGR_MIRROR) {
		return "egress mirror";
	} else if (type == TC_ACTION_TYPE_PEDIT_IP_ACTION) {
		return "ip";
	} else if (type == TC_ACTION_TYPE_PEDIT_FAMILY) {
		return "pedit";
	} else if (type == TC_ACTION_TYPE_PEDIT_TCP_ACTION) {
		return "tcp";
	} else if (type == TC_ACTION_TYPE_PEDIT_UDP_ACTION) {
		return "udp";
	} else if (type == TC_ACTION_TYPE_PEDIT_ICMP_ACTION) {
		return "icmp";
	} else if (type == TC_ACTION_TYPE_IFE) {
		return "ife";
	}

	/* else */
	snprintf(buf, len, "[%d]", type);
	return buf;

}


int convert_linux_gact_action_to_action_type(int linux_action,
					     enum tc_action_type *type)
{
	switch (linux_action) {
	case(TC_ACT_SHOT):
		*type = TC_ACTION_TYPE_GACT_DROP;
	break;
	case (TC_ACT_OK):
		*type = TC_ACTION_TYPE_GACT_OK;
	break;
	case (TC_ACT_RECLASSIFY):
		*type = TC_ACTION_TYPE_GACT_RECLASSIFY;
	break;
	case (TC_ACT_UNSPEC):
		*type = TC_ACTION_TYPE_GACT_CONTINUE;
	break;
	case (TC_ACT_PIPE):
		*type = TC_ACTION_TYPE_GACT_PIPE;
	break;
	default:
		return -1;
	}

	return 0;
}


int tc_get_gact_action_type(char *action, enum tc_action_type *type)
{
	int linux_action_type;
	int rc;

	rc = action_a2n(action, &linux_action_type);
	if (rc != 0) {
		return (-1);
	}

	return convert_linux_gact_action_to_action_type(linux_action_type, type);
}


int tc_get_action_family_type(char *action, enum tc_action_type *type)
{
	if (strcmp(action, "gact") == 0) {
		*type = TC_ACTION_TYPE_GACT_FAMILY;
	} else if (strcmp(action, "pedit") == 0) {
		*type = TC_ACTION_TYPE_PEDIT_FAMILY;
	} else if (!strcmp(action, "mirred")) {
		*type = TC_ACTION_TYPE_MIRRED_FAMILY;
	} else {
		return -1;
	}

	return 0;
}
