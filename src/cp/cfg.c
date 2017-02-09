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
*  File:                cfg.c
*  Desc:                Implementation of application configuration API.
*
*/

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include "cfg.h"
#include "log.h"

#define CFG_BUF_MAX_LENGTH 256

struct cfg_applications {
	int routing_en;
	int qos_en;
	int firewall_en;
};

struct system_cfg {
	int agt;
	int stats;
	int remote_cntrl;
	int lag_en;
	struct cfg_applications applications;
	EZapiChannel_EthIFType port_type;
	char dp_bin_file[CFG_BUF_MAX_LENGTH];
	char run_cpus[CFG_BUF_MAX_LENGTH];
	char if0_name[IF_NAME_MAX_LENGTH];
	char if1_name[IF_NAME_MAX_LENGTH];
	char if2_name[IF_NAME_MAX_LENGTH];
	char if3_name[IF_NAME_MAX_LENGTH];
};

#if 0
enum {
	NAME_OPTION = 0,
	IP_ADDR_OPTION,
	NETMASK_OPTION
};

const char *if_opts[] = {
	[NAME_OPTION] = "name",
	[IP_ADDR_OPTION] = "ip",
	[NETMASK_OPTION] = "netmask",
	NULL
};
#endif

static struct system_cfg system_cfg;

#if 0
void set_if_configuration(char *if_cfg_name, struct if_info *if_info)
{
	char	*subopts, *value;

	subopts = optarg;
	while (*subopts != '\0') {
		char *saved = subopts;

		switch (getsubopt(&subopts, (char **)if_opts, &value)) {
		case NAME_OPTION:
		{
			if (value == NULL) {
				write_log(LOG_ERR, "name value is missing for %s", if_cfg_name);
				abort();
			}
			strcpy(if_info->name, value);
			break;
		}
		case IP_ADDR_OPTION:
		{
			if (value == NULL) {
				write_log(LOG_ERR, "ip address value is missing for %s", if_cfg_name);
				abort();
			}
			strcpy(if_info->ip_addr, value);
			break;
		}
		case NETMASK_OPTION:
		{
			if (value == NULL) {
				write_log(LOG_ERR, "netmask value is missing for %s", if_cfg_name);
				abort();
			}
			strcpy(if_info->netmask, value);
			break;
		}
		default:
		{
			/* Unknown suboption. */
			write_log(LOG_ERR, "Unknown interface suboption `%s'", saved);
			abort();
		}
		}
	}
	write_log(LOG_INFO, "Added data %s: name=%s, ip=%s, netmask = %s", if_cfg_name, if_info->name, if_info->ip_addr, if_info->netmask);
	write_log(LOG_INFO, "Added data %s: name=%s, ip=%s, netmask = %s", if_cfg_name, if_info->name, if_info->ip_addr, if_info->netmask);

}
#endif

void system_configuration(int argc, char **argv)
{
	int	option_index;
	int	rc;

	struct option long_options[] = {
		{ "agt_enabled", no_argument, (int *)(&system_cfg.agt), true },
		{ "statistics", no_argument, (int *)(&system_cfg.stats), true },
		{ "remote_control", no_argument, (int *)(&system_cfg.remote_cntrl), true },
		{ "routing_app", no_argument, (int *)(&system_cfg.applications.routing_en), true },
		{ "QoS_app", no_argument, (int *)(&system_cfg.applications.qos_en), true },
		{ "firewall_app", no_argument, (int *)(&system_cfg.applications.firewall_en), true },
		{ "lag_enable", no_argument, (int *)(&system_cfg.lag_en), true },
		{ "port_type", required_argument, 0, 'p' },
		{ "dp_bin_file", required_argument, 0, 'b'},
		{ "run_cpus", required_argument, 0, 'r'},
		{ "if0", required_argument, 0, 'w'},
		{ "if1", required_argument, 0, 'x'},
		{ "if2", required_argument, 0, 'y'},
		{ "if3", required_argument, 0, 'z'},
		{0, 0, 0, 0} };


	memset(&system_cfg, 0, sizeof(struct system_cfg));

	/* assigning defaults */
#ifdef CONFIG_TC
	strcpy(system_cfg.dp_bin_file, "/usr/lib/atc/atc_dp");
#endif
#ifdef CONFIG_ALVS
	strcpy(system_cfg.dp_bin_file, "/usr/lib/alvs/alvs_dp");
#endif
	strcpy(system_cfg.run_cpus, "not_used");
	system_cfg.port_type = EZapiChannel_EthIFType_40GE;
	system_cfg.lag_en = false;
	strcpy(system_cfg.if0_name, "eth0");
	strcpy(system_cfg.if1_name, "");
	strcpy(system_cfg.if2_name, "");
	strcpy(system_cfg.if3_name, "");

	while (true) {
		rc = getopt_long(argc, argv, "", long_options, &option_index);
		if (rc == -1) {
			break;
		}

		switch (rc) {
		case 0:
			break;

		case 'p':
			if (strcmp(optarg, "10GE") == 0) {
				system_cfg.port_type = EZapiChannel_EthIFType_10GE;
			} else if (strcmp(optarg, "40GE") == 0) {
				system_cfg.port_type = EZapiChannel_EthIFType_40GE;
			} else if (strcmp(optarg, "100GE") == 0) {
				system_cfg.port_type = EZapiChannel_EthIFType_100GE;
			} else {
				write_log(LOG_CRIT, "Port type argument is invalid (%s), valid values are 10GE, 40GE and 100GE.", optarg);
				abort();
			}
			break;
		case 'b':
			if (strlen(optarg) > 256) {
				write_log(LOG_ERR, "dp_bin_file argument length is more than 256 bytes");
				abort();
			}
			strcpy(system_cfg.dp_bin_file, optarg);
			write_log(LOG_INFO, "DP Bin File: %s", system_cfg.dp_bin_file);
			break;
		case 'r':
			strcpy(system_cfg.run_cpus, optarg);
			write_log(LOG_INFO, "Run CPUs On DP: %s", system_cfg.run_cpus);
			break;
		case 'w':
			strcpy(system_cfg.if0_name, optarg);
			write_log(LOG_INFO, "IF0 name %s", system_cfg.if0_name);
			break;
		case 'x':
			strcpy(system_cfg.if1_name, optarg);
			write_log(LOG_INFO, "IF1 name %s", system_cfg.if1_name);
			break;
		case 'y':
			strcpy(system_cfg.if2_name, optarg);
			write_log(LOG_INFO, "IF2 name %s", system_cfg.if2_name);
			break;
		case 'z':
			strcpy(system_cfg.if3_name, optarg);
			write_log(LOG_INFO, "IF3 name %s", system_cfg.if3_name);
			break;
		case '?':
			break;

		default:
		{
			write_log(LOG_ERR, "Unknown argument type!!");
			abort();
		}
	}
	}
}

void system_cfg_print(void)
{
	write_log(LOG_INFO, "Starting ALVS daemon application (port type = %s,  AGT enabled = %s, Print Statistics = %s) ...",
		  (system_cfg.port_type == EZapiChannel_EthIFType_10GE ? "10GE" : (system_cfg.port_type == EZapiChannel_EthIFType_40GE ? "40GE" : "100GE")),
		  (system_cfg.agt ? "True" : "False"),
		  (system_cfg.stats ? "True" : "False"));

	write_log(LOG_INFO, "Available Applications  %s %s %s",
		  (system_cfg.applications.routing_en  ? "ROUTING_APP" : ""),
		  (system_cfg.applications.qos_en  ? "QoS_APP" : ""),
		  (system_cfg.applications.firewall_en  ? "FIREWALL_APP" : ""));

	write_log(LOG_INFO, "Available Modes  %s %s",
		  (system_cfg.remote_cntrl ? "REMOTE_CONTROL" : ""),
		  (system_cfg.lag_en ? "LAG" : ""));

	write_log(LOG_INFO, "Run CPUs  %s",
		  (system_cfg.run_cpus));

	write_log(LOG_INFO, "DP binary  %s",
		  (system_cfg.dp_bin_file));

	write_log(LOG_INFO, "NPS Interfaces:  if0: name = %s ", (system_cfg.if0_name));
	write_log(LOG_INFO, "NPS Interfaces:  if1: name = %s ", (system_cfg.if1_name));
	write_log(LOG_INFO, "NPS Interfaces:  if2: name = %s ", (system_cfg.if2_name));
	write_log(LOG_INFO, "NPS Interfaces:  if3: name = %s ", (system_cfg.if3_name));
}

bool system_cfg_is_routing_app_en(void)
{
	return system_cfg.applications.routing_en;
}

bool system_cfg_is_firewall_app_en(void)
{
	return system_cfg.applications.firewall_en;
}

bool system_cfg_is_qos_app_en(void)
{
	return system_cfg.applications.qos_en;
}

bool system_cfg_is_agt_en(void)
{
	return system_cfg.agt;
}

bool system_cfg_is_print_staistics_en(void)
{
	return system_cfg.stats;
}

bool system_cfg_is_remote_control_en(void)
{
	return system_cfg.remote_cntrl;
}

EZapiChannel_EthIFType system_cfg_get_port_type(void)
{
	return system_cfg.port_type;
}

bool system_cfg_is_lag_en(void)
{
	return system_cfg.lag_en;
}

char *system_cfg_get_dp_bin_file(void)
{
	return system_cfg.dp_bin_file;
}

char *system_cfg_get_run_cpus(void)
{
	return system_cfg.run_cpus;
}

char *system_cfg_get_if_name(int if_id)
{
	switch (if_id) {
	case 0:
	{
		return system_cfg.if0_name;
	}
	case 1:
	{
		return system_cfg.if1_name;
	}
	case 2:
	{
		return system_cfg.if2_name;
	}
	case 3:
	{
		return system_cfg.if3_name;
	}
	default:
	{
		write_log(LOG_ERR, "Requested data interfaces illegal = %d", if_id);
		return NULL;
	}
	}
}

#if 0
char *system_cfg_get_data_if(void)
{
	return system_cfg.data_if;
}


struct if_info *system_cfg_get_nps_if(int if_id)
{
	switch (if_id) {
	case 0:
	{
		return &system_cfg.if0;
	}
	case 1:
	{
		return &system_cfg.if1;
	}
	case 2:
	{
		return &system_cfg.if2;
	}
	case 3:
	{
		return &system_cfg.if3;
	}
	default:
	{
		write_log(LOG_ERR, "Requested data interfaces illegal = %d", if_id);
		return NULL;
	}
	}
}
#endif
