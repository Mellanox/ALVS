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
*  File:                infrastructure_conf.h
*  Desc:                Infrastructure configuration file
*
*/

#ifndef _INFRASTRUCTURE_CONF_H_
#define _INFRASTRUCTURE_CONF_H_

/* AGT port */
#ifndef INFRA_AGT_PORT
#	define INFRA_AGT_PORT                      1234
#endif

/* Interfaces */
#ifndef INFRA_NW_IF_NUM
#	define	INFRA_NW_IF_NUM                     4
#endif
#ifndef INFRA_NW_IF_TYPE
#	define	INFRA_NW_IF_TYPE                    EZapiChannel_EthIFType_10GE
#endif

/* Host interface parameters */
#define INFRA_HOST_IF_SIDE          1
#define INFRA_HOST_IF_PORT          24

/* Network interface #0 parameters */
#define INFRA_NW_IF_0_SIDE          0
#define INFRA_NW_IF_0_PORT          0

/* Network interface #1 parameters */
#define INFRA_NW_IF_1_SIDE          0
#define INFRA_NW_IF_1_PORT          12

/* Network interface #2 parameters */
#define INFRA_NW_IF_2_SIDE          1
#define INFRA_NW_IF_2_PORT          0

/* Network interface #3 parameters */
#define INFRA_NW_IF_3_SIDE          1
#define INFRA_NW_IF_3_PORT          12

enum infra_interface_params {
	INFRA_INTERFACE_PARAMS_SIDE             = 0,
	INFRA_INTERFACE_PARAMS_PORT             = 1,
	INFRA_NUM_OF_INTERFACE_PARAMS
};

static
uint32_t network_interface_params[INFRA_NW_IF_NUM][INFRA_NUM_OF_INTERFACE_PARAMS] = {
		{INFRA_NW_IF_0_SIDE, INFRA_NW_IF_0_PORT},
		{INFRA_NW_IF_1_SIDE, INFRA_NW_IF_1_PORT},
		{INFRA_NW_IF_2_SIDE, INFRA_NW_IF_2_PORT},
		{INFRA_NW_IF_3_SIDE, INFRA_NW_IF_3_PORT}
};

/* Memory spaces */
#define INFRA_HALF_CLUSTER_DATA_SIZE        0
#define INFRA_X1_CLUSTER_DATA_SIZE          0
#define INFRA_X2_CLUSTER_DATA_SIZE          0
#define INFRA_X4_CLUSTER_DATA_SIZE          0
#define INFRA_X16_CLUSTER_DATA_SIZE         0
#define INFRA_ALL_CLUSTER_DATA_SIZE         0
#define INFRA_EMEM_DATA_NO_ECC_SIZE         0
#define INFRA_EMEM_DATA_IN_BAND_SIZE        0
#define INFRA_EMEM_DATA_OUT_OF_BAND_SIZE    0

#define INFRA_HALF_CLUSTER_SEARCH_SIZE      0
#define INFRA_X1_CLUSTER_SEARCH_SIZE        4
#define INFRA_X2_CLUSTER_SEARCH_SIZE        0
#define INFRA_X4_CLUSTER_SEARCH_SIZE        0
#define INFRA_X16_CLUSTER_SEARCH_SIZE       0
#define INFRA_ALL_CLUSTER_SEARCH_SIZE       0
#define INFRA_EMEM_SEARCH_SIZE              256

/* Statistics */
#define INFRA_STATS_POSTED_GROUP_SIZE       (120*1024)
#define INFRA_STATS_POSTED_NUM              (16*1024)

#endif /* _INFRASTRUCTURE_CONF_H_ */
