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
*	file - tc_common_utils.h
*	description - contains APIs of tc_common_utils.c
*/

#ifndef _TC_COMMON_UTILS_H_
#define _TC_COMMON_UTILS_H_



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
			     int                  len);


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
			       int                  len);


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
			int                  len);


/******************************************************************************
 * \brief      Convert linux GACT action index to tc_action_type
 *
 * \param[in]  linux_action  - GACT Linux action ID
 * \param[out] type          - pointer to tc_action_type result
 *
 * \return     ( 0) - success
 *             (-1) - failure
 */
int convert_linux_gact_action_to_action_type(int linux_action,
					     enum tc_action_type *type);


/******************************************************************************
 * \brief      Get GACT action type from GACT action name (string)
 *
 * \param[in]  action        - GACT action name
 * \param[out] type          - pointer to tc_action_type result
 *
 * \return     ( 0) - success
 *             (-1) - failure
 */
int tc_get_gact_action_type(char *action, enum tc_action_type *type);


/******************************************************************************
 * \brief      Get action family type from action name (string)
 *
 * \param[in]  action        - action name
 * \param[out] type          - pointer to tc_action_type result
 *
 * \return     ( 0) - success
 *             (-1) - failure
 */
int tc_get_action_family_type(char *action, enum tc_action_type *type);

#endif  /* def _TC_COMMON_UTILS_H_ */
