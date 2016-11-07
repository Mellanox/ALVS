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
*  File:                cfg.h
*  Desc:                Configuration API.
*
*/

#ifndef SRC_CP_CFG_H_
#define SRC_CP_CFG_H_

#include <EZapiChannel.h>

/**************************************************************************//**
 * \brief       initialize internal configuration data structure
 *
 * \param[in]   argc - number of arguments
 * \param[in]   argv - array of arguments
 *
 * \return      none
 */
void system_configuration(int argc, char **argv);

/**************************************************************************//**
 * \brief       Print system configuration including available applications,
 *              modes, general information
 *
 * \return      none
 */
void system_cfg_print(void);

/**************************************************************************//**
 * \brief       Check if ROUTING application enable
 *
 * \return      true - if ROUTING application enable
 *              false - otherwise
 */
bool system_cfg_is_routing_app_en(void);

/**************************************************************************//**
 * \brief       Check if AGT  enable
 *
 * \return      true - if AGT  enable
 *              false - otherwise
 */
bool system_cfg_is_agt_en(void);

/**************************************************************************//**
 * \brief       Check if Print statistics  enable
 *
 * \return      true - if Print statistics  enable
 *              false - otherwise
 */
bool system_cfg_is_print_staistics_en(void);

/**************************************************************************//**
 * \brief       Check if Remote control mode  enable
 *
 * \return      true - if Remote control mode  enable
 *              false - otherwise
 */
bool system_cfg_is_remote_control_en(void);

/**************************************************************************//**
 * \brief       return portType
 *
 * \return      eth IF type
 */
EZapiChannel_EthIFType system_cfg_get_port_type(void);

/**************************************************************************//**
 * \brief       Check if LAG mode  enable
 *
 *
 * \return      true - if LAG  enable
 *              false - otherwise
 */
bool system_cfg_is_lag_en(void);

/**************************************************************************//**
 * \brief       Check if Firewall application enable
 *
 * \return      true - if Firewall application enable
 *              false - otherwise
 */
bool system_cfg_is_firewall_app_en(void);

/**************************************************************************//**
 * \brief       Check if QoS application enable
 *
 * \return      true - if QoS application enable
 *              false - otherwise
 */
bool system_cfg_is_qos_app_en(void);

/**************************************************************************//**
 * \brief       get dp binary filename
 *
 * \return      string that contains the file name
 */
char *system_cfg_get_dp_bin_file(void);

/**************************************************************************//**
 * \brief       get run_cpus string
 *
 * \return      string that contains run cpus
 */
char *system_cfg_get_run_cpus(void);


#endif /* SRC_CP_CFG_H_ */
