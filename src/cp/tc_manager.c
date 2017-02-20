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
*  Project:             NPS400 ALVS application
*  File:                nw_db_manager.c
*  Desc:                Performs the main process of network DB management
*
****************************************************************************/

/* linux includes */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <byteswap.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <netlink/msg.h>
#include <linux/pkt_cls.h>
#include <linux/tc_act/tc_gact.h>
#include <linux/tc_act/tc_mirred.h>
#include <linux/tc_act/tc_pedit.h>
#include <linux/if_ether.h>

/* Project includes */
#include "log.h"
#include "tc_manager.h"
#include "tc_api.h"

#include "tc_common_utils.h"

bool *tc_db_manager_cancel_application_flag;
int tc_get_all_filters_req(int linux_interface);
int tc_dump_all_filters(void);

struct iproute2_rtnl_handle {
	int			fd;
	struct sockaddr_nl	local;
	struct sockaddr_nl	peer;
	uint32_t		seq;
	uint32_t		dump;
	int			proto;
	int			flags;
};

struct iproute2_rtnl_handle tc_rtnl_handle;


/* Policy used for command attributes */
static struct nla_policy tca_policy[] = {
	[TCA_KIND] = {.type = NLA_STRING, .maxlen = MAX_TC_FILTER_KIND_SIZE},
	[TCA_OPTIONS] = {.type = NLA_NESTED},
	[TCA_STATS] = {.type = NLA_UNSPEC, .maxlen =
			sizeof(struct tc_stats)},
	[TCA_STATS2] = {.type = NLA_NESTED},
};


static struct nla_policy act_policy[TCA_ACT_MAX + 1] = {
	[TCA_ACT_KIND] = {.type = NLA_STRING},
	[TCA_ACT_OPTIONS] = {.type = NLA_NESTED},
	[TCA_ACT_STATS] = {.type = NLA_NESTED},
	[TCA_ACT_INDEX] = {.type = NLA_U32},
};


/* Policy used for command attributes */
static struct nla_policy tc_filter_policy[TCA_FLOWER_MAX + 1] = {
	[TCA_FLOWER_UNSPEC]             = { .type = NLA_UNSPEC },
	[TCA_FLOWER_CLASSID]            = { .type = NLA_U32 },
	[TCA_FLOWER_INDEV]              = { .type = NLA_STRING,
						.maxlen = IFNAMSIZ },
	[TCA_FLOWER_ACT]             = { .type = NLA_NESTED },
	[TCA_FLOWER_KEY_ETH_DST]        = { .maxlen = ETH_ALEN },
	[TCA_FLOWER_KEY_ETH_DST_MASK]   = { .maxlen = ETH_ALEN },
	[TCA_FLOWER_KEY_ETH_SRC]        = { .maxlen = ETH_ALEN },
	[TCA_FLOWER_KEY_ETH_SRC_MASK]   = { .maxlen = ETH_ALEN },
	[TCA_FLOWER_KEY_ETH_TYPE]       = { .type = NLA_U16 },
	[TCA_FLOWER_KEY_IP_PROTO]       = { .type = NLA_U8 },
	[TCA_FLOWER_KEY_IPV4_SRC]       = { .type = NLA_U32 },
	[TCA_FLOWER_KEY_IPV4_SRC_MASK]  = { .type = NLA_U32 },
	[TCA_FLOWER_KEY_IPV4_DST]       = { .type = NLA_U32 },
	[TCA_FLOWER_KEY_IPV4_DST_MASK]  = { .type = NLA_U32 },
	[TCA_FLOWER_KEY_IPV6_SRC]       = { .maxlen = sizeof(struct in6_addr) },
	[TCA_FLOWER_KEY_IPV6_SRC_MASK]  = { .maxlen = sizeof(struct in6_addr) },
	[TCA_FLOWER_KEY_IPV6_DST]       = { .maxlen = sizeof(struct in6_addr) },
	[TCA_FLOWER_KEY_IPV6_DST_MASK]  = { .maxlen = sizeof(struct in6_addr) },
	[TCA_FLOWER_KEY_TCP_SRC]        = { .type = NLA_U16 },
	[TCA_FLOWER_KEY_TCP_DST]        = { .type = NLA_U16 },
	[TCA_FLOWER_KEY_UDP_SRC]        = { .type = NLA_U16 },
	[TCA_FLOWER_KEY_UDP_DST]        = { .type = NLA_U16 },
};





static inline uint32_t netlinkl_mgrp(uint32_t group)
{
	if (group > 31) {
		write_log(LOG_CRIT, "Use setsockopt for this group %d\n", group);
		exit(-1);
	}
	return group ? (1 << (group - 1)) : 0;
}

/******************************************************************************
 * \brief         TC NL initialization.
 *                Allocates, configures and connects to socket.
 *
 * \return        void
 */
void tc_nl_init(void)
{
	uint32_t groups = netlinkl_mgrp(RTNLGRP_TC);
	int sndbuf = 32768;	/* Open raw socket */
	int rcvbuf = 1024 * 1024;
	socklen_t addr_len;
	struct timeval tv;

	tc_rtnl_handle.fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
	if (tc_rtnl_handle.fd < 0) {
		write_log(LOG_CRIT, "TC Socket Allocation FAILED");
		tc_db_manager_exit_with_error();
	}

	if (setsockopt(tc_rtnl_handle.fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf)) < 0) {
		write_log(LOG_CRIT, "TC setsockopt SO_SNDBUF FAILED");
		tc_db_manager_exit_with_error();
	}
	if (setsockopt(tc_rtnl_handle.fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) < 0) {
		write_log(LOG_CRIT, "TC setsockopt SO_RCVBUF FAILED");
		tc_db_manager_exit_with_error();
	}

	memset(&tc_rtnl_handle.local, 0, sizeof(tc_rtnl_handle.local));
	tc_rtnl_handle.local.nl_family = AF_NETLINK;
	tc_rtnl_handle.local.nl_groups = groups;

	if (bind(tc_rtnl_handle.fd, (struct sockaddr *)&tc_rtnl_handle.local, sizeof(tc_rtnl_handle.local)) < 0) {
		write_log(LOG_CRIT, "TC bind FAILED");
		tc_db_manager_exit_with_error();
	}
	addr_len = sizeof(tc_rtnl_handle.local);

	if (getsockname(tc_rtnl_handle.fd, (struct sockaddr *)&tc_rtnl_handle.local, &addr_len) < 0) {
		write_log(LOG_CRIT, "TC getsockname FAILED");
		tc_db_manager_exit_with_error();
	}

	if (addr_len != sizeof(tc_rtnl_handle.local)) {
		write_log(LOG_CRIT, "TC Wrong address length %d", addr_len);
		tc_db_manager_exit_with_error();
	}

	if (tc_rtnl_handle.local.nl_family != AF_NETLINK) {
		write_log(LOG_CRIT, "TC Wrong nl_family");
		tc_db_manager_exit_with_error();
	}


	/* Set timeout on the raw socket */
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	if (setsockopt(tc_rtnl_handle.fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
		write_log(LOG_CRIT, "Error setting socket timeout options");
		tc_db_manager_exit_with_error();
	}
	tc_rtnl_handle.seq = time(NULL);
}

/******************************************************************************
 * \brief       Print TC filter identification
 *
 * \return     none
 */
static void tc_print_action_type(enum tc_action_type type, int i)
{
	write_log(LOG_INFO, "action[%d] = %s ", i,
	(type == TC_ACTION_TYPE_GACT_DROP) ? "drop" :
	(type == TC_ACTION_TYPE_GACT_OK) ? "OK" :
	(type == TC_ACTION_TYPE_GACT_RECLASSIFY) ? "reclassify" :
	(type == TC_ACTION_TYPE_GACT_PIPE) ? "pipe" :
	(type == TC_ACTION_TYPE_GACT_CONTINUE) ? "continue" :
	(type == TC_ACTION_TYPE_MIRRED_EGR_REDIRECT) ? "Egress redirect" :
	(type == TC_ACTION_TYPE_MIRRED_EGR_MIRROR) ? "Egress mirror" :
	(type == TC_ACTION_TYPE_PEDIT_FAMILY) ? "PEDIT" : "unknown");
}
static void tc_print_pedit_action(struct tc_action *action)
{
	int i = 0;

	tc_print_action_type(action->action_data.pedit.control_action_type, 0);
	write_log(LOG_INFO, "num_of_keys = %d", action->action_data.pedit.num_of_keys);
	for (i = 0; i < action->action_data.pedit.num_of_keys; i++) {
		write_log(LOG_INFO, "pedit_key_data[%d].at = 0x%x", i, bswap_32(action->action_data.pedit.key_data[i].pedit_key_data.at));
		write_log(LOG_INFO, "pedit_key_data[%d].mask = 0x%x", i, bswap_32(action->action_data.pedit.key_data[i].pedit_key_data.mask));
		write_log(LOG_INFO, "pedit_key_data[%d].off = 0x%x", i, bswap_32(action->action_data.pedit.key_data[i].pedit_key_data.off));
		write_log(LOG_INFO, "pedit_key_data[%d].offmask = 0x%x", i, bswap_32(action->action_data.pedit.key_data[i].pedit_key_data.offmask));
		write_log(LOG_INFO, "pedit_key_data[%d].val = 0x%x", i, bswap_32(action->action_data.pedit.key_data[i].pedit_key_data.val));
	}
}
static void tc_print_action_general_info(struct tc_action_general *action_general)
{
	write_log(LOG_INFO, "index = 0x%x, refcnt = 0x%x, bindcnt = 0x%x, capab = 0x%x",
		  action_general->index, action_general->refcnt, action_general->bindcnt, action_general->capab);
}
static void tc_print_action(struct tc_action *action, int i)
{
	tc_print_action_general_info(&action->general);
	tc_print_action_type(action->general.type, i);
	switch (action->general.type) {
	case (TC_ACTION_TYPE_PEDIT_FAMILY):
		write_log(LOG_INFO, "PEDIT_PARAMS START");
		tc_print_pedit_action(action);
	break;
	default:
	break;
	}
}

static void tc_print_actions(struct tc_actions *actions)
{
	int i = 0;
	struct tc_action *action;

	write_log(LOG_INFO, "num_of_actions = %d", actions->num_of_actions);
	for (i = 0; i < actions->num_of_actions; i++) {
		action = &actions->action[i];
		tc_print_action(action, i);
	}
}

static void tc_print_flower_filter(struct tc_filter *tc_filter_handle)
{
	struct tc_flower_rule_policy *flower_rule_policy = &tc_filter_handle->flower_rule_policy;

	write_log(LOG_INFO, "prio 0x%x, Proto 0x%x, hndln 0x%x, ifindex 0x%x, flags 0x%x, type %s",
		  tc_filter_handle->priority,
		  bswap_16(tc_filter_handle->protocol),
		  tc_filter_handle->handle,
		  tc_filter_handle->ifindex,
		  tc_filter_handle->flags,
		  (tc_filter_handle->type == TC_FILTER_FLOWER) ? "flower" : "NOT SUPPORTED");

	if (tc_filter_handle->flower_rule_policy.mask_bitmap.is_eth_dst_set == 1) {
		write_log(LOG_INFO, "key_eth_dst %02x:%02x:%02x:%02x:%02x:%02x",
			  flower_rule_policy->key_eth_dst[0],
			  flower_rule_policy->key_eth_dst[1],
			  flower_rule_policy->key_eth_dst[2],
			  flower_rule_policy->key_eth_dst[3],
			  flower_rule_policy->key_eth_dst[4],
			  flower_rule_policy->key_eth_dst[5]);
		write_log(LOG_INFO, "mask_eth_dst %02x:%02x:%02x:%02x:%02x:%02x",
			  flower_rule_policy->mask_eth_dst[0],
			  flower_rule_policy->mask_eth_dst[1],
			  flower_rule_policy->mask_eth_dst[2],
			  flower_rule_policy->mask_eth_dst[3],
			  flower_rule_policy->mask_eth_dst[4],
			  flower_rule_policy->mask_eth_dst[5]);
	}

	if (tc_filter_handle->flower_rule_policy.mask_bitmap.is_eth_src_set == 1) {
		write_log(LOG_INFO, "key_eth_src %02x:%02x:%02x:%02x:%02x:%02x",
			  flower_rule_policy->key_eth_src[0],
			  flower_rule_policy->key_eth_src[1],
			  flower_rule_policy->key_eth_src[2],
			  flower_rule_policy->key_eth_src[3],
			  flower_rule_policy->key_eth_src[4],
			  flower_rule_policy->key_eth_src[5]);
		write_log(LOG_INFO, "mask_eth_src %02x:%02x:%02x:%02x:%02x:%02x",
			  flower_rule_policy->mask_eth_src[0],
			  flower_rule_policy->mask_eth_src[1],
			  flower_rule_policy->mask_eth_src[2],
			  flower_rule_policy->mask_eth_src[3],
			  flower_rule_policy->mask_eth_src[4],
			  flower_rule_policy->mask_eth_src[5]);
	}

	if (flower_rule_policy->mask_bitmap.is_eth_type_set == 1) {
		write_log(LOG_INFO, "key_eth_type  0x%x", bswap_16(flower_rule_policy->key_eth_type));
		write_log(LOG_INFO, "key_eth_mask  0x%x", bswap_16(flower_rule_policy->mask_eth_type));
	}

	if (flower_rule_policy->mask_bitmap.is_ip_proto_set == 1) {
		write_log(LOG_INFO, "key_ip_proto  0x%x", flower_rule_policy->key_ip_proto);
		write_log(LOG_INFO, "mask_ip_proto  0x%x", flower_rule_policy->mask_ip_proto);
	}

	if (flower_rule_policy->mask_bitmap.is_ipv4_src_set == 1) {
		write_log(LOG_INFO, "key_ipv4_src  0x%x", bswap_32(flower_rule_policy->key_ipv4_src));
		write_log(LOG_INFO, "mask_ipv4_src  0x%x", bswap_32(flower_rule_policy->mask_ipv4_src));
	}

	if (flower_rule_policy->mask_bitmap.is_ipv4_dst_set == 1) {
		write_log(LOG_INFO, "key_ipv4_src  0x%x", bswap_32(flower_rule_policy->key_ipv4_dst));
		write_log(LOG_INFO, "mask_ipv4_src  0x%x", bswap_32(flower_rule_policy->mask_ipv4_dst));
	}

	if (flower_rule_policy->mask_bitmap.is_l4_src_set == 1) {
		write_log(LOG_INFO, "key_ipv4_src  0x%x",
			  bswap_16(flower_rule_policy->key_l4_src));
		write_log(LOG_INFO, "mask_ipv4_src  0x%x",
			  bswap_16(flower_rule_policy->mask_l4_src));

	}

	if (flower_rule_policy->mask_bitmap.is_l4_dst_set == 1) {
		write_log(LOG_INFO, "key_ipv4_dst  0x%x",
			  bswap_16(flower_rule_policy->key_l4_dst));
		write_log(LOG_INFO, "mask_ipv4_dst  0x%x",
			  bswap_16(flower_rule_policy->mask_l4_dst));

	}

	tc_print_actions(&tc_filter_handle->actions);

}


/******************************************************************************
 * \brief       Help function for filling policy (key, mask)
 *
 * \return      0 = when policy defined, -1 - if not defined
 */

static int flower_set_key_val(struct nlattr **tb,
			      void *val, int val_type,
			      void *mask, int mask_type, int len)
{

	if (!tb[val_type]) {
		return -1;
	}

	memcpy(val, nla_data(tb[val_type]), len);

	if (mask_type == TCA_FLOWER_UNSPEC || !tb[mask_type]) {
		memset(mask, 0xff, len);
	} else {
		memcpy(mask, nla_data(tb[mask_type]), len);
	}

	return 0;
}

/******************************************************************************
 * \brief       Fill TC filter rule policy from netlink message
 *
 * \return      0 = success, otherwise fail
 */

static int fill_tc_filter_flower_policy(struct nlattr **attr, struct tc_flower_rule_policy *flower_rule_policy)
{
	int err;

	err = flower_set_key_val(attr,  flower_rule_policy->key_eth_dst,
				 TCA_FLOWER_KEY_ETH_DST,
				 flower_rule_policy->mask_eth_dst,
				 TCA_FLOWER_KEY_ETH_DST_MASK,
				 sizeof(flower_rule_policy->key_eth_dst));
	if (err == 0) {
		flower_rule_policy->mask_bitmap.is_eth_dst_set = 1;
	}

	err = flower_set_key_val(attr,  flower_rule_policy->key_eth_src,
				 TCA_FLOWER_KEY_ETH_SRC,
				 flower_rule_policy->mask_eth_src,
				 TCA_FLOWER_KEY_ETH_SRC_MASK,
				 sizeof(flower_rule_policy->key_eth_src));
	if (err == 0) {
		flower_rule_policy->mask_bitmap.is_eth_src_set = 1;
	}

	err = flower_set_key_val(attr, &flower_rule_policy->key_eth_type,
				 TCA_FLOWER_KEY_ETH_TYPE,
				 &flower_rule_policy->mask_eth_type,
				 TCA_FLOWER_UNSPEC,
				 sizeof(flower_rule_policy->key_eth_type));
	if (err == 0 && (bswap_16(flower_rule_policy->key_eth_type) != ETH_P_ALL)) {
		write_log(LOG_DEBUG, "is_eth_type_set = 1");
		flower_rule_policy->mask_bitmap.is_eth_type_set = 1;
	} else if (bswap_16(flower_rule_policy->key_eth_type) == ETH_P_ALL) {
		flower_rule_policy->mask_eth_type = 0;
	}

	err = flower_set_key_val(attr, &flower_rule_policy->key_ip_proto,
				 TCA_FLOWER_KEY_IP_PROTO,
				 &flower_rule_policy->mask_ip_proto,
				 TCA_FLOWER_UNSPEC,
				 sizeof(flower_rule_policy->key_ip_proto));
	if (err == 0)
		flower_rule_policy->mask_bitmap.is_ip_proto_set = 1;

	if (attr[TCA_FLOWER_KEY_IPV4_SRC] || attr[TCA_FLOWER_KEY_IPV4_DST]) {
		err = flower_set_key_val(attr,
					 &flower_rule_policy->key_ipv4_src,
					 TCA_FLOWER_KEY_IPV4_SRC,
					 &flower_rule_policy->mask_ipv4_src,
					 TCA_FLOWER_KEY_IPV4_SRC_MASK,
					 sizeof(flower_rule_policy->key_ipv4_src));
		if (err == 0) {
			flower_rule_policy->mask_bitmap.is_ipv4_src_set = 1;
		}

		err = flower_set_key_val(attr,
					 &flower_rule_policy->key_ipv4_dst,
					 TCA_FLOWER_KEY_IPV4_DST,
					 &flower_rule_policy->mask_ipv4_dst,
					 TCA_FLOWER_KEY_IPV4_DST_MASK,
					 sizeof(flower_rule_policy->key_ipv4_dst));
		if (err == 0) {
			flower_rule_policy->mask_bitmap.is_ipv4_dst_set = 1;
		}

	} else if (attr[TCA_FLOWER_KEY_IPV6_SRC] || attr[TCA_FLOWER_KEY_IPV6_DST]) {
		write_log(LOG_INFO, "IPV6 is not supported");
		return -1;
	}

	if (flower_rule_policy->key_ip_proto == IPPROTO_TCP) {
		err = flower_set_key_val(attr,
					 &flower_rule_policy->key_l4_src,
					 TCA_FLOWER_KEY_TCP_SRC,
					 &flower_rule_policy->mask_l4_src,
					 TCA_FLOWER_UNSPEC,
					 sizeof(flower_rule_policy->key_l4_src));
		if (err == 0) {
			flower_rule_policy->mask_bitmap.is_l4_src_set = 1;
		}


		err = flower_set_key_val(attr, &flower_rule_policy->key_l4_dst,
					 TCA_FLOWER_KEY_TCP_DST,
					 &flower_rule_policy->mask_l4_dst,
					 TCA_FLOWER_UNSPEC,
					 sizeof(flower_rule_policy->key_l4_dst));

		if (err == 0) {
			flower_rule_policy->mask_bitmap.is_l4_dst_set = 1;
		}
	} else if (flower_rule_policy->key_ip_proto == IPPROTO_UDP) {
		err = flower_set_key_val(attr,
					 &flower_rule_policy->key_l4_src,
					 TCA_FLOWER_KEY_UDP_SRC,
					 &flower_rule_policy->mask_l4_src,
					 TCA_FLOWER_UNSPEC,
					 sizeof(flower_rule_policy->key_l4_src));
		if (err == 0) {
			flower_rule_policy->mask_bitmap.is_l4_src_set = 1;
		}

		err = flower_set_key_val(attr,
					 &flower_rule_policy->key_l4_dst,
					 TCA_FLOWER_KEY_UDP_DST,
					 &flower_rule_policy->mask_l4_dst,
					 TCA_FLOWER_UNSPEC,
					 sizeof(flower_rule_policy->key_l4_dst));
		if (err == 0) {
			flower_rule_policy->mask_bitmap.is_l4_dst_set = 1;
		}

	}
	return 0;
}

static int fill_general_action(struct nlattr *act_options, struct tc_action *action)
{
	int err;
	static struct nla_policy gact_policy[TCA_GACT_MAX + 1] = {
	[TCA_GACT_PARMS] = {.type = NLA_UNSPEC, .minlen =
			sizeof(struct tc_gact), },
	[TCA_GACT_PROB] = {.type = NLA_UNSPEC, .minlen =
			sizeof(struct tc_gact_p) },
	[TCA_GACT_TM] = {.type = NLA_UNSPEC, .minlen =
			sizeof(struct tcf_t) },
	};
	struct nlattr *gact_attrs[TCA_GACT_MAX + 1];

	err = nla_parse_nested(gact_attrs, TCA_GACT_MAX, act_options, gact_policy);
	if (err != 0) {
		write_log(LOG_ERR, "nla_parse_nested for gact - problem parsing services");
		return -1;
	}

	if (gact_attrs[TCA_GACT_PARMS]) {
		const struct tc_gact *p_gact =
			nla_data(gact_attrs[TCA_GACT_PARMS]);

		err = convert_linux_gact_action_to_action_type(p_gact->action, &action->general.type);
		if (err) {
			write_log(LOG_ERR, "Unknown gact actions %d", p_gact->action);
			return -1;
		}
		action->general.index = p_gact->index;
		action->general.capab = p_gact->capab;
		action->general.bindcnt = p_gact->bindcnt;
		action->general.refcnt = p_gact->refcnt;
	} else {
		write_log(LOG_ERR, "GACT Action Missing params");
		return -1;
	}
	return 0;
}

static int fill_mirred_action(struct nlattr *act_options, struct tc_action *action)
{
	int err;
	static struct nla_policy mirred_policy[TCA_MIRRED_MAX +
					      1] = {
	[TCA_MIRRED_PARMS] = {.type = NLA_UNSPEC, .minlen =
		sizeof(struct tc_mirred) },
	[TCA_MIRRED_TM] = {.type = NLA_UNSPEC, .minlen =
		sizeof(struct tcf_t) },
	};
	struct nlattr *mirred_attrs[TCA_MIRRED_MAX + 1];

	err = nla_parse_nested(mirred_attrs, TCA_MIRRED_MAX, act_options, mirred_policy);
	if (err != 0) {
		write_log(LOG_ERR, "Action nla_parse_nested for mirred - problem parsing services");
		return -1;
	}

	if (mirred_attrs[TCA_MIRRED_PARMS]) {
		const struct tc_mirred *p_mirred =
			nla_data(mirred_attrs[TCA_MIRRED_PARMS]);

		if (p_mirred->action == TC_ACT_STOLEN
			&& p_mirred->eaction == TCA_EGRESS_REDIR && p_mirred->ifindex) {
		/* set action & data */
			action->general.type = TC_ACTION_TYPE_MIRRED_EGR_REDIRECT;
			action->action_data.mirred.ifindex = p_mirred->ifindex;
		} else {
			write_log(LOG_ERR, "Action unknown/unsupported. mirred action %d, eaction %d, ifindex %d",
				  p_mirred->action, p_mirred->eaction, p_mirred->ifindex);
			return -1;
		}
		 action->general.index = p_mirred->index;
		 action->general.capab = p_mirred->capab;
		 action->general.bindcnt = p_mirred->bindcnt;
		 action->general.refcnt = p_mirred->refcnt;
	} else {
		write_log(LOG_ERR, "MIRRED Action missing params!");
		return -1;
	}
	return 0;
}

static int fill_pedit_action(struct nlattr *act_options, struct tc_action *action)
{
	int err;
	struct nlattr *pedit_attrs[TCA_PEDIT_MAX + 1];
	static struct nla_policy pedit_policy[TCA_PEDIT_MAX + 1] = {
	[TCA_PEDIT_PARMS] = { .minlen = sizeof(struct tc_pedit) },
	};

	err = nla_parse_nested(pedit_attrs, TCA_PEDIT_MAX, act_options, pedit_policy);
	if (err != 0) {
		write_log(LOG_ERR, "Action nla_parse_nested for pedit - problem parsing services");
		return -1;
	}
	if (pedit_attrs[TCA_PEDIT_PARMS]) {
		const struct tc_pedit *p_edit =
			nla_data(pedit_attrs[TCA_PEDIT_PARMS]);
		if (p_edit->nkeys == 0) {
			write_log(LOG_ERR, "action of type pedit num->of_keys 0");
			return -1;
		}
		int ksize = p_edit->nkeys * sizeof(struct tc_pedit_key);

		if (nla_len(pedit_attrs[TCA_PEDIT_PARMS]) <
				(int)(sizeof(*p_edit) + ksize)) {
			write_log(LOG_ERR, "action of type pedit wrong values from netlink");
			return -1;
		}
		write_log(LOG_INFO, "PEDIT COME");

		action->general.type = TC_ACTION_TYPE_PEDIT_FAMILY;
		action->general.index = p_edit->index;
		action->general.capab = p_edit->capab;
		action->general.bindcnt = p_edit->bindcnt;
		action->general.refcnt = p_edit->refcnt;

		action->action_data.pedit.num_of_keys = p_edit->nkeys;
		err = convert_linux_gact_action_to_action_type(p_edit->action, &action->action_data.pedit.control_action_type);
		if (err) {
			write_log(LOG_ERR, "Action unkown gact actions: %d",  p_edit->action);
			return -1;
		}
		int k;

		for (k = 0; k < p_edit->nkeys; k++) {
			action->action_data.pedit.key_data[k].pedit_key_data.at =
					 p_edit->keys[k].at;
			action->action_data.pedit.key_data[k].pedit_key_data.mask =
					 p_edit->keys[k].mask;
			action->action_data.pedit.key_data[k].pedit_key_data.off =
					 p_edit->keys[k].off;
			action->action_data.pedit.key_data[k].pedit_key_data.offmask =
					 p_edit->keys[k].offmask;
			action->action_data.pedit.key_data[k].pedit_key_data.shift =
					 p_edit->keys[k].shift;
			action->action_data.pedit.key_data[k].pedit_key_data.val =
					 p_edit->keys[k].val;

		}

	}  else {
		write_log(LOG_ERR, "PEDIT  Action missing  params!");
		return -1;
	}

	return 0;
}


/******************************************************************************
 * \brief       Parse the TC message part in netlink message
 *
 * \return      0 = success, otherwise fail
 */
static int tc_parse_common_part(struct nlmsghdr *msghdr, struct nlattr **attr, struct tc_filter *tc_filter_handle)
{
	int err;
	struct tcmsg *t = NLMSG_DATA(msghdr);
	char kind[MAX_TC_FILTER_KIND_SIZE];


	tc_filter_handle->flags = msghdr->nlmsg_flags;

	if (t == NULL) {
		write_log(LOG_CRIT, "traffic control message is NULL");
		return -1;
	}

	tc_filter_handle->handle = t->tcm_handle;
	tc_filter_handle->ifindex = t->tcm_ifindex;

	if (t->tcm_info) {
		tc_filter_handle->protocol = TC_H_MIN(t->tcm_info);
		tc_filter_handle->priority = TC_H_MAJ(t->tcm_info)>>16;
	} else {
		write_log(LOG_CRIT, "tcm_info in tcmsg header is not defined");
		return -1;
	}

	err = nlmsg_parse(msghdr, NLMSG_ALIGN(sizeof(struct tcmsg)), attr, TCA_MAX, tca_policy);
	if (err != 0) {
		write_log(LOG_INFO, "nlmsg_parse failed err %d", err);
		return -1;
	}

	if (attr[TCA_KIND] != NULL) {
		strncpy(kind, nla_get_string(attr[TCA_KIND]), MAX_TC_FILTER_KIND_SIZE);
		if (memcmp(kind, "flower", sizeof("flower")) == 0) {
			tc_filter_handle->type = TC_FILTER_FLOWER;
		}
	} else {
		write_log(LOG_CRIT, "FILTER type is not defined in nlmsg");
		return -1;
	}

	return 0;

}
static int fill_tc_filter_action(struct nlattr *attr, struct tc_actions *tc_actions, uint16_t msg_type)
{
	int err;
	struct tc_action *action;
	int i = 0;
	int k = 0;
	static struct nla_policy tc_actions_orders_policy[MAX_NUM_OF_ACTIONS_IN_FILTER + 1] = { };
	struct nlattr *tc_actions_orders_attr[sizeof(tc_actions_orders_policy)];

	if (attr == NULL) {
		write_log(LOG_INFO, "received no action attributes");
		return -1;
	}


	if (msg_type == RTM_DELACTION) {
		err = nla_parse_nested(tc_actions_orders_attr, MAX_NUM_OF_ACTIONS_IN_FILTER, attr, NULL);
		if (err != 0) {
			write_log(LOG_ERR, "nla_parse_nested - problem parsing services");
			return -1;
		}
		i = 1;
	} else if (msg_type == RTM_NEWACTION) {
		for (i = 0; i < (MAX_NUM_OF_ACTIONS_IN_FILTER + 1); i++) {
			tc_actions_orders_policy[i].type = NLA_NESTED;
		}
		err = nla_parse_nested(tc_actions_orders_attr, MAX_NUM_OF_ACTIONS_IN_FILTER, attr, tc_actions_orders_policy);
		if (err != 0) {
			write_log(LOG_ERR, "nla_parse_nested - problem parsing services");
			return -1;
		}
		i = 0;
	} else {
			write_log(LOG_ERR, "wrong msg_type for this action");
			return -1;
		}

	for (; i < MAX_NUM_OF_ACTIONS_IN_FILTER; i++) {
		if (tc_actions_orders_attr[i]) {
			action = &tc_actions->action[k];
			write_log(LOG_INFO, "action pointer after attr %p, %d", action, i);

			struct nlattr *action_attrs[TCA_ACT_MAX + 1];

			if (msg_type == RTM_DELACTION) {
				err = nla_parse_nested(action_attrs, TCA_ACT_MAX, tc_actions_orders_attr[i], NULL);
				if (err != 0) {
					write_log(LOG_ERR, "nla_parse_nested - problem parsing services");
					return -1;
				}

			} else {
				err = nla_parse_nested(action_attrs, TCA_ACT_MAX, tc_actions_orders_attr[i], act_policy);
				if (err != 0) {
					write_log(LOG_ERR, "nla_parse_nested - problem parsing services");
					return -1;
				}
			}

#if 0
			if ((action_attrs[TCA_ACT_INDEX] == NULL) ||
				nla_len(action_attrs[TCA_ACT_INDEX]) <
				(int)sizeof(action->general.index)) {
				if (action_attrs[TCA_ACT_INDEX] == NULL) {
					write_log(LOG_ERR, "action_attrs[TCA_ACT_INDEX] = NULL");
				} else {
					write_log(LOG_ERR, "nla_len = %d", nla_len(action_attrs[TCA_ACT_INDEX]));
				}
				/* return -1; */
			} else {
				action->general.index = nla_get_u32(action_attrs[TCA_ACT_INDEX]);
				write_log(LOG_INFO, "general.index = %d", action->general.index);
			}
#endif
			if (action_attrs[TCA_ACT_KIND]) {
				const char *act_kind  = nla_get_string(action_attrs[TCA_ACT_KIND]);
				struct nlattr *act_options = action_attrs[TCA_ACT_OPTIONS];

				if (act_options == NULL) {
					write_log(LOG_ERR, "no action options are specified");
					return -1;
				}

				if (!strcmp(act_kind, "gact")) {
					err = fill_general_action(act_options, action);
					if (err) {
						write_log(LOG_ERR, "Action %d GACT FAILED", i);
						return -1;
					}
					write_log(LOG_INFO, "gact %d", i);
					action->general.type |= TC_ACTION_TYPE_GACT_FAMILY;
				} else if (!strcmp(act_kind, "pedit")) {
					err = fill_pedit_action(act_options, action);
					if (err) {
						write_log(LOG_ERR, "Action %d PEDIT FAILED", i);
						return -1;
					}
					write_log(LOG_INFO, "pedit %d", i);
					action->general.type |= TC_ACTION_TYPE_PEDIT_FAMILY;

				} else if (!strcmp(act_kind, "mirred")) {
					err = fill_mirred_action(act_options, action);
					if (err) {
						write_log(LOG_ERR, "Action %d IRRED FAILED", i);
						return -1;
					}
					write_log(LOG_INFO, "mirred %d", i);
					action->general.type |= TC_ACTION_TYPE_MIRRED_FAMILY;
				} else {
					write_log(LOG_ERR, "Action %d UNSUPPORTED operation", i);
					return -1;
				}
				action->general.family_type = action->general.type & TC_ACTION_FAMILY_MASK;
			} else {
				write_log(LOG_ERR, "Action %d TCA_ACT_KIND MISSING", i);
				return -1;
			}
			k++;
			tc_actions->num_of_actions++;

		}
	}
	if (msg_type == RTM_NEWACTION) {
		if (tc_actions->num_of_actions == 0) {
			write_log(LOG_ERR, "No actions for TC filter");
			return -1;
		}
	}
	return 0;
}
static int fill_tc_filter_flower_options(struct nlattr *attr, struct tc_filter *tc_filter_handle)
{
	int err;
	struct nlattr *tc_filter_attrs[TCA_FLOWER_MAX+1];

	if (attr) {
		err = nla_parse_nested(tc_filter_attrs, TCA_FLOWER_MAX, attr, tc_filter_policy);
		if (err != 0) {
			write_log(LOG_CRIT, "nla_parse_nested - problem parsing services");
			return -1;
		}
	} else {
		write_log(LOG_ERR, "attr[TCA_OPTIONS] NULL");
		return -1;
	}

	err = fill_tc_filter_flower_policy(tc_filter_attrs, &tc_filter_handle->flower_rule_policy);
	if (err != 0) {
		write_log(LOG_ERR, "fill_tc_filter_policy FAILED");
		return err;
	}

	err = fill_tc_filter_action(tc_filter_attrs[TCA_FLOWER_ACT], &tc_filter_handle->actions, RTM_NEWACTION);
	if (err != 0) {
		write_log(LOG_ERR, "fill_tc_filter_action FAILED");
		return err;
	}

	return 0;
}



static int tc_parse_action_msg(struct tc_actions *tc_actions, struct nlmsghdr *msghdr)
{
	struct nlattr *attr[TCA_ACT_MAX+1];
	int err;

	err = nlmsg_parse(msghdr, NLMSG_ALIGN(sizeof(struct tcamsg)), attr, TCA_ACT_MAX, NULL);
	if (err != 0) {
		write_log(LOG_INFO, "nlmsg_parse failed err %d", err);
		return -1;
	}

	err = fill_tc_filter_action(attr[TCA_ACT_TAB], tc_actions, msghdr->nlmsg_type);
	if (err != 0) {
		write_log(LOG_INFO, "fill_tc_filter_action FAILED");
		return -1;
	}


	return 0;

}

/******************************************************************************
 * \brief         Parsing of add TC filter netlink message and filling the relevant
 *                data structure to continue with this data structure
 *                to implementation of OFFLOADING via chip
 *
 * \return        void
 */
static int tc_parse_add_filter_msg(struct tc_filter *tc_filter_handle, struct nlmsghdr *n)
{
	struct nlattr *attr[TCA_MAX+1];
	int err;

	err = tc_parse_common_part(n, attr, tc_filter_handle);
	if (err) {
		write_log(LOG_CRIT, "tc_parse_common_part FAILED");
		return err;
	}

	switch (tc_filter_handle->type) {
	case (TC_FILTER_FLOWER):
		err = fill_tc_filter_flower_options(attr[TCA_OPTIONS], tc_filter_handle);
		if (err) {
			write_log(LOG_CRIT, "fill_tc_filter_flower_options FAILED");
			return err;
		}
	break;
	default:
		write_log(LOG_CRIT, "UNSUPPORTED type");
		return -1;
	}

	return 0;
}

/******************************************************************************
 * \brief         Parsing of delete TC filter netlink message and filling the relevant
 *		data structure to continue with this data structure
 *		to implementation of OFFLOADING via chip
 * \return	void
 */
static int tc_parse_del_filter_msg(struct tc_filter *tc_filter_handle,
				   struct nlmsghdr *nlmsghdr)
{
	int err;
	struct nlattr *attr[TCA_MAX+1];

	err = tc_parse_common_part(nlmsghdr, attr, tc_filter_handle);
	if (err) {
		write_log(LOG_CRIT, "tc_parse_common_part FAILED");
		return err;
	}
	switch (tc_filter_handle->type) {
	case (TC_FILTER_FLOWER):
	break;
	default:
		write_log(LOG_CRIT, "UNSUPPORTED type");
	return -1;
	}
	return 0;
}

/******************************************************************************
 * \brief         Process packet received from TC socket.
 *                According to the type of message the process continues
 *                to the relevant specific function.
 *
 * \return        void
 */
static void process_packet(uint8_t *buffer, struct sockaddr __attribute__((__unused__)) *saddr)
{
	struct nlmsghdr *nlmsghdr = (struct nlmsghdr *)buffer;
	struct tc_filter tc_filter_handle;
	int err;

	/*commands for TC group*/
	memset(&tc_filter_handle, 0, sizeof(tc_filter_handle));

	if (nlmsghdr->nlmsg_type == RTM_NEWTFILTER ||
		nlmsghdr->nlmsg_type == RTM_DELTFILTER ||
		nlmsghdr->nlmsg_type == RTM_GETTFILTER) {

		if (nlmsghdr->nlmsg_type == RTM_DELTFILTER) {
			write_log(LOG_DEBUG, "RTM_DELTFILTER");
			err = tc_parse_del_filter_msg(&tc_filter_handle, nlmsghdr);
			tc_print_flower_filter(&tc_filter_handle);
			write_log(LOG_DEBUG, "tc_parse_del_filter_msg err = %d", err);
			if (err == 0) {
				err = tc_delete_filter(&tc_filter_handle);
				if (err == TC_API_DB_ERROR) {
					write_log(LOG_CRIT, "TC delete filter failed - exit_with_error");
					tc_db_manager_exit_with_error();
				} else if (err == TC_API_FAILURE) {
					write_log(LOG_CRIT, "TC delete filter failed");
					return;
				}
			}
			write_log(LOG_INFO, "Delete TFILTER");
			tc_print_flower_filter(&tc_filter_handle);
			write_log(LOG_INFO, "EXECUTED");
			return;
		}

		if (nlmsghdr->nlmsg_type == RTM_NEWTFILTER) {
			write_log(LOG_DEBUG, "RTM_NEWTFILTER");
			err = tc_parse_add_filter_msg(&tc_filter_handle, nlmsghdr);
			tc_print_flower_filter(&tc_filter_handle);
			write_log(LOG_DEBUG, "nlmsghdr->nlmsg_flags = 0x%x", nlmsghdr->nlmsg_flags);
			if (err == 0) {
				err = tc_add_filter(&tc_filter_handle);
				if (err == TC_API_DB_ERROR) {
					write_log(LOG_CRIT, "TC add filter failed - exit_with_error");
					tc_db_manager_exit_with_error();
				} else if (err == TC_API_FAILURE) {
					write_log(LOG_CRIT, "TC add filter failed");
					return;
				}
			}


			write_log(LOG_INFO, "Add TFILTER");
			tc_print_flower_filter(&tc_filter_handle);
			write_log(LOG_INFO, "EXECUTED");
			return;
		}
		if (nlmsghdr->nlmsg_type == RTM_GETTFILTER) {
			write_log(LOG_CRIT, "RTM_GETTFILTER");
		}
	} else if (nlmsghdr->nlmsg_type == RTM_NEWQDISC || nlmsghdr->nlmsg_type == RTM_DELQDISC) {
		if (nlmsghdr->nlmsg_type == RTM_NEWQDISC) {
			write_log(LOG_DEBUG, "nlmsg_type RTM_NEWQDISC");
			struct tcmsg *t = NLMSG_DATA(nlmsghdr);

			if (t == NULL) {
				write_log(LOG_CRIT, "traffic control message is NULL");
				return;
			}
			write_log(LOG_DEBUG, "t->tcm_handle = 0x%x", t->tcm_handle);
			write_log(LOG_DEBUG, "t->tcm_parent = 0x%x", t->tcm_parent);
			if (t->tcm_info) {
				write_log(LOG_DEBUG, "t->tcm_info = 0x%x", t->tcm_info);
				if (t->tcm_parent == TC_H_INGRESS) {
					write_log(LOG_DEBUG, "RTM_NEWQDISC ENTER");
					write_log(LOG_DEBUG, "t->tcm_ifindex = %d", t->tcm_ifindex);
					/*sending TC DUMP request to get all TC filters saved in kernel*/
					err = tc_get_all_filters_req(t->tcm_ifindex);
					if (err < 0) {
						write_log(LOG_CRIT, "Request for DUMP of all tc_filters FAILED");
						tc_db_manager_exit_with_error();
					}

					/*getting the answer from Kernel with all saved Tc filters identification*/
					err = tc_dump_all_filters();
					if (err != 0) {
						write_log(LOG_CRIT, "Dump all filters FAILED");
						tc_db_manager_exit_with_error();
					}
					return;
				}
				write_log(LOG_DEBUG, "RTM_NEWQDISC NOT INGRESS");
				return;

			} else {
				write_log(LOG_CRIT, "Nothing to do");
				return;
			}

		}
		if (nlmsghdr->nlmsg_type == RTM_DELQDISC) {

			write_log(LOG_DEBUG, "nlmsg_type RTM_DELQDISC");
			struct tcmsg *t = NLMSG_DATA(nlmsghdr);

			if (t == NULL) {
				write_log(LOG_CRIT, "traffic control message is NULL");
				return;
			}
			write_log(LOG_DEBUG, "t->tcm_handle = 0x%x", t->tcm_handle);
			write_log(LOG_DEBUG, "t->tcm_parent = 0x%x", t->tcm_parent);

			if (t->tcm_info) {
				write_log(LOG_DEBUG, "t->tcm_info = 0x%x", t->tcm_info);
				if (t->tcm_parent == TC_H_INGRESS) {
					write_log(LOG_DEBUG, "tc_delete_all_filters_on_interface ENTER");
					err = tc_delete_all_filters_on_interface(t->tcm_ifindex, TC_FILTER_FLOWER);
					if (err == TC_API_DB_ERROR) {
						write_log(LOG_CRIT, "TC delete qdisc failed - exit_with_error");
						tc_db_manager_exit_with_error();
					} else if (err == TC_API_FAILURE) {
						write_log(LOG_CRIT, "TC delete qdisc filters on this qdisc FAILED");
						return;
					}
				}
			} else {
				write_log(LOG_CRIT, "Nothing to do");
				return;
			}
			write_log(LOG_INFO, "Del QDISC");
			write_log(LOG_INFO, "EXECUTED");
			return;
		}
	} else if (nlmsghdr->nlmsg_type == RTM_GETACTION || nlmsghdr->nlmsg_type == RTM_NEWACTION ||
		nlmsghdr->nlmsg_type == RTM_DELACTION) {
		if (nlmsghdr->nlmsg_type == RTM_NEWACTION) {
			write_log(LOG_INFO, "RTM_NEWACTION");
		} else if (nlmsghdr->nlmsg_type == RTM_DELACTION) {
			write_log(LOG_INFO, "RTM_DELACTION");
		} else {
			write_log(LOG_INFO, "RTM_GETACTION");
		}
		struct tc_action action;

		memset(&action, 0, sizeof(struct tc_action));
		if ((nlmsghdr->nlmsg_type == RTM_NEWACTION) ||
			(nlmsghdr->nlmsg_type == RTM_DELACTION)) {
			err = tc_parse_action_msg(&tc_filter_handle.actions, nlmsghdr);
			tc_print_actions(&tc_filter_handle.actions);

			if (err == 0) {
				if (nlmsghdr->nlmsg_type == RTM_NEWACTION) {
					int i = 0;

					for (i = 0; i < tc_filter_handle.actions.num_of_actions; i++) {
					err = tc_add_action(&tc_filter_handle.actions.action[i]);
					if (err == TC_API_DB_ERROR) {
						write_log(LOG_CRIT, "TC add action failed - exit_with_error");
						tc_db_manager_exit_with_error();
					} else if (err == TC_API_FAILURE) {
						write_log(LOG_CRIT, "TC add action failed");
						return;
					}
					}
				} else  {
					int i = 0;

					for (i = 0; i < tc_filter_handle.actions.num_of_actions; i++) {
					err = tc_delete_action(&tc_filter_handle.actions.action[i]);
					if (err == TC_API_DB_ERROR) {
						write_log(LOG_CRIT, "TC delete action failed - exit_with_error");
						tc_db_manager_exit_with_error();
					} else if (err == TC_API_FAILURE) {
						write_log(LOG_CRIT, "TC delete action failed");
						return;
					}
					}

				}
			}


		}

	} else if (nlmsghdr->nlmsg_type != NLMSG_ERROR && nlmsghdr->nlmsg_type != NLMSG_NOOP &&
		nlmsghdr->nlmsg_type != NLMSG_DONE) {
		write_log(LOG_CRIT, "Unknown message: length %08d type %08x flags %08x",
			  nlmsghdr->nlmsg_len, nlmsghdr->nlmsg_type, nlmsghdr->nlmsg_flags);
	}

}


/******************************************************************************
 * \brief       Sending TC filter DUMP request to get all TC filters
 *
 * \return      answer from kernel sendmsg request
 */
int tc_get_all_filters_req(int linux_interface)
{
	struct nlmsghdr nlh;
	struct sockaddr_nl nladdr = { .nl_family = AF_NETLINK, .nl_groups = netlinkl_mgrp(RTNLGRP_TC) };
	struct tcmsg tc_msg;
	struct iovec iov[2] = {
		{ .iov_base = &nlh, .iov_len = sizeof(nlh) },
		{ .iov_base = &tc_msg, .iov_len = sizeof(tc_msg) }
	};
	struct msghdr msg = {
		.msg_name = &nladdr,
		.msg_namelen = sizeof(nladdr),
		.msg_iov = iov,
		.msg_iovlen = 2,
	};
	memset(&tc_msg, 0, sizeof(tc_msg));
	tc_msg.tcm_family = AF_UNSPEC;
	tc_msg.tcm_parent = TC_H_ROOT;
	tc_msg.tcm_handle = TC_H_INGRESS;
	tc_msg.tcm_ifindex = linux_interface;/*TODO - not hardcoded*/

	nlh.nlmsg_len = NLMSG_LENGTH(sizeof(tc_msg));
	nlh.nlmsg_type = RTM_GETTFILTER;
	nlh.nlmsg_flags = NLM_F_DUMP |  NLM_F_REQUEST;
	nlh.nlmsg_pid = 0;
	nlh.nlmsg_seq = tc_rtnl_handle.dump = ++tc_rtnl_handle.seq;

	return sendmsg(tc_rtnl_handle.fd, &msg, 0);

}


static int tc_fill_specific_l(struct nlmsghdr *n, int maxlen, int type, const void *data,
	      int alen)
{
	int len = RTA_LENGTH(alen);
	struct rtattr *rta;

	if (NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len) > (uint32_t)maxlen) {
		fprintf(stderr, "addattr_l ERROR: message exceeded bound of %d\n", maxlen);
		return -1;
	}
	rta = NLMSG_TAIL_PRIVATE(n);
	rta->rta_type = type;
	rta->rta_len = len;
	memcpy(RTA_DATA(rta), data, alen);
	n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len);
	return 0;

}


int tc_get_all_actions_req(void)
{
	char k[16];
	int prio = 0;
	struct nlmsghdr nlh;
	struct sockaddr_nl nladdr = { .nl_family = AF_NETLINK, .nl_groups = netlinkl_mgrp(RTNLGRP_TC) };
	struct tcamsg tca_msg;
	struct rtattr *tail, *tail2;
	int msg_length;
	struct iovec iov[2] = {
		{ .iov_base = &nlh, .iov_len = sizeof(nlh) },
		{ .iov_base = &tca_msg, .iov_len = sizeof(tca_msg) }
	};

	struct msghdr msg = {
		.msg_name = &nladdr,
		.msg_namelen = sizeof(nladdr),
		.msg_iov = iov,
		.msg_iovlen = 2,
	};
	memset(k, 0, sizeof(k));

	memset(&tca_msg, 0, sizeof(tca_msg));
	tca_msg.tca_family = AF_UNSPEC;

	nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcamsg));
	write_log(LOG_INFO, "START nlh.nlmsg_len nlmsg_len = 0x%x", nlh.nlmsg_len);
	tail = NLMSG_TAIL_PRIVATE(&nlh);
	write_log(LOG_INFO, "tail = 0x%p", tail);
	strncpy(k, "gact", sizeof("gact") - 1);
	write_log(LOG_INFO, "sizeof gact %d", (int)sizeof("gact"));


	tc_fill_specific_l(&nlh, 16384, TCA_ACT_TAB, NULL, 0);
	tail2 = NLMSG_TAIL_PRIVATE(&nlh);

	tc_fill_specific_l(&nlh, 16384, (++prio), NULL, 0);
	tc_fill_specific_l(&nlh, 16384, TCA_ACT_KIND, k, (int)(strlen(k) + 1));
	tail2->rta_len = (void *)NLMSG_TAIL_PRIVATE(&nlh) - (void *)tail2;
	tail->rta_len = (void *)NLMSG_TAIL_PRIVATE(&nlh) - (void *)tail;
	msg_length = NLMSG_ALIGN(nlh.nlmsg_len) - NLMSG_ALIGN(sizeof(struct nlmsghdr));

	nlh.nlmsg_len = NLMSG_LENGTH(msg_length);
	nlh.nlmsg_type = RTM_GETACTION;
	nlh.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;
	nlh.nlmsg_pid = 0;
	nlh.nlmsg_seq = tc_rtnl_handle.dump = ++tc_rtnl_handle.seq;
	iov[1].iov_len = msg_length;
	write_log(LOG_INFO, "nlh.nlmsg_len nlmsg_len = 0x%x", nlh.nlmsg_len);
	return sendmsg(tc_rtnl_handle.fd, &msg, 0);

}

/******************************************************************************
 * \brief       Getting TC filter saved in kernel
 *
 * \return      answer from kernel sendmsg request
 */
int tc_dump_all_filters(void)
{
	int data_size = 0;
	struct sockaddr_nl nladdr = { .nl_family = AF_NETLINK, .nl_pid = 0, .nl_groups = netlinkl_mgrp(RTNLGRP_TC)};
	struct iovec iov;
	struct msghdr msg = {
			.msg_name = &nladdr,
			.msg_namelen = sizeof(nladdr),
			.msg_iov = &iov,
			.msg_iovlen = 1,
	};
	char buffer[16384];
	struct nlmsghdr *nlmsghdr_hdr;

	write_log(LOG_DEBUG, "tc_dump_all_filters");

	iov.iov_base = buffer;
	iov.iov_len = sizeof(buffer);

	while (data_size >= 0) {
		data_size = recvmsg(tc_rtnl_handle.fd, &msg, 0);
		write_log(LOG_DEBUG, "data_size > 0");
		if (data_size > 0) {
			nlmsghdr_hdr = (struct nlmsghdr *)buffer;
			while (NLMSG_OK(nlmsghdr_hdr, data_size)) {
				write_log(LOG_DEBUG, "msg_len = %d type 0x%x flags 0x%x pid 0x%x", nlmsghdr_hdr->nlmsg_len, nlmsghdr_hdr->nlmsg_type, nlmsghdr_hdr->nlmsg_flags,
					  nlmsghdr_hdr->nlmsg_pid);

				if (nladdr.nl_pid != 0 ||
					nlmsghdr_hdr->nlmsg_pid != tc_rtnl_handle.local.nl_pid ||
					nlmsghdr_hdr->nlmsg_seq != tc_rtnl_handle.dump) {
					write_log(LOG_INFO, "GO to skip");
					nlmsghdr_hdr = NLMSG_NEXT(nlmsghdr_hdr, data_size);
					continue;
				}
				process_packet((uint8_t *)nlmsghdr_hdr, 0);
				nlmsghdr_hdr = NLMSG_NEXT(nlmsghdr_hdr, data_size);

			}
			if (data_size) {
				write_log(LOG_CRIT, "!!!Remnant of size %d\n", data_size);
			}
		}
	}
return 0;
}


int tc_dump_all_actions(void)
{
	int data_size = 0;
	struct sockaddr_nl nladdr = { .nl_family = AF_NETLINK, .nl_pid = 0, .nl_groups = netlinkl_mgrp(RTNLGRP_TC) };
	struct iovec iov;
	struct msghdr msg = {
			.msg_name = &nladdr,
			.msg_namelen = sizeof(nladdr),
			.msg_iov = &iov,
			.msg_iovlen = 1,
	};
	char buffer[16384];
	struct nlmsghdr *nlmsghdr_hdr;

	write_log(LOG_DEBUG, "tc_dump_all_actions");

	iov.iov_base = buffer;
	iov.iov_len = sizeof(buffer);

	while (data_size >= 0) {
		data_size = recvmsg(tc_rtnl_handle.fd, &msg, 0);
		write_log(LOG_DEBUG, "data_size > 0 = %d", data_size);
		nlmsghdr_hdr = (struct nlmsghdr *)buffer;
		process_packet((uint8_t *)nlmsghdr_hdr, 0);

		if (data_size > 0) {
			nlmsghdr_hdr = (struct nlmsghdr *)buffer;
			while (NLMSG_OK(nlmsghdr_hdr, data_size)) {
				write_log(LOG_DEBUG, "msg_len = %d type 0x%x flags 0x%x pid 0x%x", nlmsghdr_hdr->nlmsg_len, nlmsghdr_hdr->nlmsg_type, nlmsghdr_hdr->nlmsg_flags,
					  nlmsghdr_hdr->nlmsg_pid);

				if (nladdr.nl_pid != 0 ||
					nlmsghdr_hdr->nlmsg_pid != tc_rtnl_handle.local.nl_pid ||
					nlmsghdr_hdr->nlmsg_seq != tc_rtnl_handle.dump) {
					write_log(LOG_INFO, "GO to skip");
					nlmsghdr_hdr = NLMSG_NEXT(nlmsghdr_hdr, data_size);
					continue;
				}
				process_packet((uint8_t *)nlmsghdr_hdr, 0);
				nlmsghdr_hdr = NLMSG_NEXT(nlmsghdr_hdr, data_size);

			}
			if (data_size) {
				write_log(LOG_CRIT, "!!!Remnant of size %d\n", data_size);
			}
		}
	}
return 0;
}
/******************************************************************************
 * \brief       Polling TC socket for getting messages
 * \return      void
 */
void tc_manager_poll(void)
{
	int data_size = 0;
	struct sockaddr_nl nladdr = { .nl_family = AF_NETLINK, .nl_pid = 0, .nl_groups = netlinkl_mgrp(RTNLGRP_TC) };
	struct iovec iov;
	struct msghdr msg = {
			.msg_name = &nladdr,
			.msg_namelen = sizeof(nladdr),
			.msg_iov = &iov,
			.msg_iovlen = 1,
	};
	char buffer[16384];
	struct nlmsghdr *nlmsghdr_hdr;

	iov.iov_base = buffer;
	iov.iov_len = sizeof(buffer);

	while (*tc_db_manager_cancel_application_flag == false) {
		data_size = recvmsg(tc_rtnl_handle.fd, &msg, 0);

		if (data_size > 0) {
			nlmsghdr_hdr = (struct nlmsghdr *)buffer;
			while (NLMSG_OK(nlmsghdr_hdr, data_size)) {

				process_packet((uint8_t *)nlmsghdr_hdr, 0);
				nlmsghdr_hdr = NLMSG_NEXT(nlmsghdr_hdr, data_size);

			}
			if (data_size) {
				write_log(LOG_CRIT, "!!!Remnant of size %d\n", data_size);
			}
		}
	}
}

int tc_get_ingress_qdisc_request(void)
{
	struct nlmsghdr nlh;
	struct sockaddr_nl nladdr = { .nl_family = AF_NETLINK, .nl_groups = netlinkl_mgrp(RTNLGRP_TC) };
	struct tcmsg tc_msg;
	struct iovec iov[2] = {
		{ .iov_base = &nlh, .iov_len = sizeof(nlh) },
		{ .iov_base = &tc_msg, .iov_len = sizeof(tc_msg) }
	};
	struct msghdr msg = {
		.msg_name = &nladdr,
		.msg_namelen = sizeof(nladdr),
		.msg_iov = iov,
		.msg_iovlen = 2,
	};
	memset(&tc_msg, 0, sizeof(tc_msg));
	tc_msg.tcm_family = AF_UNSPEC;
	tc_msg.tcm_parent = TC_H_ROOT;
	tc_msg.tcm_handle = TC_H_INGRESS;

	nlh.nlmsg_len = NLMSG_LENGTH(sizeof(tc_msg));
	nlh.nlmsg_type = RTM_GETQDISC;
	nlh.nlmsg_flags = NLM_F_DUMP |  NLM_F_REQUEST;
	nlh.nlmsg_pid = 0;
	nlh.nlmsg_seq = tc_rtnl_handle.dump = ++tc_rtnl_handle.seq;

	return sendmsg(tc_rtnl_handle.fd, &msg, 0);

}
/******************************************************************************
 * \brief       Initialization of all TC filters handled by the TC DB manager
 *
 * \return      void
 */
void  tc_db_manager_table_init(void)
{
	int err;

	err = tc_get_ingress_qdisc_request();
	if (err < 0) {
		write_log(LOG_CRIT, "Request for DUMP of ingress TC_QDISC FAILED");
		tc_db_manager_exit_with_error();
	}

	/*getting the answer from Kernel with all saved Tc filters identification*/
	err = tc_dump_all_filters();
	if (err != 0) {
		write_log(LOG_CRIT, "Dump all filters FAILED");
		tc_db_manager_exit_with_error();
	}
#if 0
	/*sending TC DUMP request to get all TC actions saved in kernel*/
	write_log(LOG_INFO, "tc_get_all_actions_req");
	err = tc_get_all_actions_req();
	if (err < 0) {
		write_log(LOG_CRIT, "Dump all actions FAILED = err = %d", err);
		tc_db_manager_exit_with_error();
	}

	/*getting the answer from Kernel with all saved Tc actions identification*/
	err = tc_dump_all_filters();
	if (err != 0) {
		write_log(LOG_CRIT, "Dump all filters FAILED FAILED");
		tc_db_manager_exit_with_error();
	}

	/*sending TC DUMP request to get all TC filters saved in kernel*/
	err = tc_get_all_filters_req();
	if (err < 0) {
		write_log(LOG_CRIT, "Request for DUMP of all tc_filters FAILED");
		tc_db_manager_exit_with_error();
	}

	/*getting the answer from Kernel with all saved Tc filters identification*/
	err = tc_dump_all_filters();
	if (err != 0) {
		write_log(LOG_CRIT, "Dump all filters FAILED");
		tc_db_manager_exit_with_error();
	}
#endif
}


/******************************************************************************
 * \brief	  Delete TC manager object
 *
 * \return	  void
 */
void tc_db_manager_delete(void)
{
	write_log(LOG_DEBUG, "Delete TC DB manager");
	if (tc_rtnl_handle.fd >= 0) {
		close(tc_rtnl_handle.fd);
		tc_rtnl_handle.fd = -1;
	}
	tc_destroy();
}

/******************************************************************************
 * \brief    Raises SIGTERM signal to main thread and exits the thread.
 *           Deletes the TC DB  manager.
 *
 * \return   void
 */
void tc_db_manager_exit_with_error(void)
{
	*tc_db_manager_cancel_application_flag = true;
	tc_db_manager_delete();
	kill(getpid(), SIGTERM);
	pthread_exit(NULL);
}
/******************************************************************************
 * \brief	  Initialization of TC netlink connection and TC SQL DBs.
 *            Also masks SIGTERM signal, will be received in main thread only.
 *
 * \return	  void
 */
void tc_db_manager_init(void)
{
	sigset_t sigs_to_block;

	sigemptyset(&sigs_to_block);
	sigaddset(&sigs_to_block, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &sigs_to_block, NULL);

	tc_nl_init();
	if (tc_init() != TC_API_OK) {
		write_log(LOG_CRIT, "Failed to create SQL DB.");
		tc_db_manager_exit_with_error();
	}
}

/******************************************************************************
 * \brief	  TC thread main application.
 *
 * \return	  void
 */
void tc_db_manager_main(bool *cancel_application_flag)
{
	tc_db_manager_cancel_application_flag = cancel_application_flag;
	write_log(LOG_DEBUG, "tc_db_manager_init...");
	tc_db_manager_init();
	write_log(LOG_DEBUG, "tc_db_manager_table_init...");
#ifndef EZ_SIM
	tc_db_manager_table_init();
#endif
	write_log(LOG_INFO, "tc_manager_poll...");
	tc_manager_poll();
	write_log(LOG_DEBUG, "tc_manager_exit...");
	tc_db_manager_delete();
}


