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
*  File:                tc_db.c
*  Desc:                Implementation of TC (traffic control) Database API.
*
*/


#include "sqlite3.h"
#include "tc_manager.h"
#include "tc_api.h"
#include "tc_flower.h"
#include "nw_ops.h"

#define TC_FLOWER_DB_FILE_NAME       "tc_flower.db"
#define TC_DB_SQL_COMMAND_SIZE       512
sqlite3 *tc_db;

/**************************************************************************//**
 * \brief       Create internal DB
 *
 * \param[in]   none
 *
 * \return      TC_API_DB_ERROR - Failed to create internal DB
 *              TC_API_OK       - Created successfully
 */
bool tc_init_db(void)
{
	int rc;
	char *sql;
	char *zErrMsg = NULL;

	/* Delete existing DB file */
	(void)remove(TC_FLOWER_DB_FILE_NAME);

	/* Open the DB file */
	rc = sqlite3_open(TC_FLOWER_DB_FILE_NAME, &tc_db);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "Can't open database: %s", sqlite3_errmsg(tc_db));
		return false;
	}

	write_log(LOG_DEBUG, "TC_DB: Opened database successfully");

	/* Create the filter table:
	 *
	 * Fields:
	 * interface
	 * priority
	 * handle
	 * created_timestamp
	 * num_of_actions
	 * actions (Action index, Timestamp index, Type (general or standard), Statistics base), max 33 actions
	 * filter_rule_index
	 * eth_dst_key
	 * eth_src_key
	 * eth_type_key
	 * ip_proto_key
	 * ipv4_src_key
	 * ipv4_dst_key
	 * l4_src_key
	 * l4_dst_key
	 * eth_dst_mask
	 * eth_src_mask
	 * eth_type_mask
	 * ip_proto_mask
	 * ipv4_src_mask
	 * ipv4_dst_mask
	 * l4_src_mask
	 * l4_dst_mask
	 * mask_flow_bitmap
	 * filter_actions_index
	 * protocol
	 *
	 * Key:
	 * priority
	 * handle
	 * interface
	 */
	sql = "CREATE TABLE filters_table("
		"interface INT NOT NULL,"		/* 0 */
		"priority INT NOT NULL,"		/* 1 */
		"handle INT NOT NULL,"			/* 2 */
		"num_of_actions INT NOT NULL,"		/* 3 */
		"actions_array BLOB NOT NULL,"		/* 4 - struct action_id * num of actions on filter */
		/* multi priority list info */
		"rule_list_index INT NOT NULL,"		/* 5 */
		/* filter key */
		"eth_dst_key BIGINT NOT NULL,"		/* 6 */
		"eth_src_key BIGINT NOT NULL,"		/* 7 */
		"eth_type_key INT NOT NULL,"		/* 8 */
		"ip_proto_key INT NOT NULL,"		/* 9 */
		"ipv4_src_key INT NOT NULL,"		/* 10 */
		"ipv4_dst_key INT NOT NULL,"		/* 11 */
		"l4_src_key INT NOT NULL,"		/* 12 */
		"l4_dst_key INT NOT NULL,"		/* 13 */
		/* filter mask */
		"eth_dst_mask BIGINT NOT NULL,"		/* 14 */
		"eth_src_mask BIGINT NOT NULL,"		/* 15 */
		"eth_type_mask INT NOT NULL,"		/* 16 */
		"ip_proto_mask INT NOT NULL,"		/* 17 */
		"ipv4_src_mask INT NOT NULL,"		/* 18 */
		"ipv4_dst_mask INT NOT NULL,"		/* 19 */
		"l4_src_mask INT NOT NULL,"		/* 20 */
		"l4_dst_mask INT NOT NULL,"		/* 21 */
		"mask_flow_bitmap INT NOT NULL,"	/* 22 */
		"filter_actions_index INT NOT NULL,"	/* 23 */
		"protocol INT NOT NULL,"		/* 24 */
		"PRIMARY KEY (priority, handle, interface));";

	/* Execute SQL statement */
	rc = sqlite3_exec(tc_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		sqlite3_free(zErrMsg);
		return false;
	}

	write_log(LOG_DEBUG, "TC_DB: Opened database successfully 1");
	/* Create the masks table:
	 *
	 * Fields:
	 * interface
	 * mask_index
	 * eth_type_mask
	 * eth_dst_mask
	 * eth_src_mask
	 * eth_type_mask
	 * ip_proto_mask
	 * ipv4_src_mask
	 * ipv4_dst_mask
	 * l4_src_mask
	 * l4_dst_mask
	 * result_bitmap_mask
	 *
	 * Key:
	 * interface
	 * eth_type
	 * mask_index
	 */

	sql = "CREATE TABLE masks_table("
		"interface INT NOT NULL,"		/* 0 */
		"mask_index INT NOT NULL,"		/* 1 */
		"eth_type_mask INT NOT NULL,"		/* 2 */
		"eth_dst_mask BIGINT NOT NULL,"		/* 3 */
		"eth_src_mask BIGINT NOT NULL,"		/* 4 */
		"ip_proto_mask INT NOT NULL,"		/* 5 */
		"ipv4_src_mask INT NOT NULL,"		/* 6 */
		"ipv4_dst_mask INT NOT NULL,"		/* 7 */
		"l4_src_mask INT NOT NULL,"		/* 8 */
		"l4_dst_mask INT NOT NULL,"		/* 9 */
		"result_bitmap_mask INT_NOT_NULL,"	/* 10 */
		"PRIMARY KEY (interface, mask_index));";

	/* Execute SQL statement */
	rc = sqlite3_exec(tc_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return false;
	}

	write_log(LOG_DEBUG, "TC_DB: Opened database successfully 2");
	/* Create the actions table:
	 *
	 * Fields:
	 * linux_action_index
	 * action_family_type
	 * created_timestamp
	 * nps_index
	 * statistics_base
	 * independent_action
	 * bind_value
	 * action_data
	 *
	 * Key:
	 * linux_action_index
	 * action_family_type
	 */

	sql = "CREATE TABLE actions_table("
		"linux_action_index INT NOT NULL,"		/* 0 */
		"action_family_type INT NOT NULL,"		/* 1 */
		"created_timestamp INT NOT NULL,"		/* 2 */
		"nps_index BIGINT NOT NULL,"			/* 3 */
		"statistics_base BIGINT NOT NULL,"		/* 4 */
		"independent_action INT NOT NULL,"		/* 5 */
		"bind_value INT NOT NULL,"			/* 6 */
		"capab INT NOT NULL,"				/* 7 */
		"refcnt INT NOT NULL,"				/* 8 */
		"action_type INT NOT NULL,"			/* 9 */
		"action_data BLOB NOT NULL,"			/* 10 */
		"PRIMARY KEY (linux_action_index, action_family_type));";

	/* Execute SQL statement */
	rc = sqlite3_exec(tc_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return false;
	}
	write_log(LOG_DEBUG, "TC_DB: Opened database successfully 3");

	return true;
}

/******************************************************************************
 * \brief	close ATC DB
 *
 *
 * \return	bool
 *              true -  function succeed
 *              false - function failed
 */
bool tc_destroy_db(void)
{
	int rc;

	rc = sqlite3_close(tc_db);
	if (rc != SQLITE_OK) {
		write_log(LOG_ERR, "Return Code for sqlite3_close is %d", rc);
		return false;
	}
	return true;
}

/*********************************************************************************************/
/************************************* Actions Table *****************************************/
/*********************************************************************************************/

/******************************************************************************
 * \brief	add action to DB
 *
 * \param[in]   tc_action_params    - action configuration (received from netlink)
 * \param[in]   action_info         - action extra information (NPS data, entry index, statistics)
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc add_tc_action_to_db(struct tc_action *tc_action_params, struct action_info *action_info)
{
	enum tc_api_rc rc;
	char *zErrMsg = NULL;
	char sql[TC_DB_SQL_COMMAND_SIZE];
	sqlite3_stmt *statement;

	if (infra_read_real_time_clock_seconds(&action_info->created_timestamp) == false) {
		return TC_API_FAILURE;
	}

	snprintf(sql, TC_DB_SQL_COMMAND_SIZE,
		 "INSERT INTO actions_table "
		 "(linux_action_index, "
		 "action_family_type, "
		 "created_timestamp, "
		 "nps_index, "
		 "statistics_base, "
		 "independent_action, "
		 "bind_value, "
		 "capab, "
		 "refcnt, "
		 "action_type, "
		 "action_data) "
		 "VALUES (%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, ?);",
		 tc_action_params->general.index,			/* linux_action_index */
		 tc_action_params->general.family_type,			/* action_family_type */
		 action_info->created_timestamp,			/* created_timestamp */
		 action_info->nps_index,				/* nps_index */
		 action_info->statistics_base.raw_data,			/* statistics_base */
		 action_info->independent_action,			/* independent_action */
		 tc_action_params->general.bindcnt,			/* bind_value */
		 tc_action_params->general.capab,			/* capab */
		 tc_action_params->general.refcnt,			/* refcnt */
		 tc_action_params->general.type);			/* action_type */

	write_log(LOG_DEBUG, "sql command: %s", sql);

	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		return TC_API_DB_ERROR;
	}

	rc = sqlite3_bind_blob(statement, 1, action_info, sizeof(struct action_info), SQLITE_STATIC);

	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		return TC_API_DB_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);
	if (rc != SQLITE_DONE) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		write_log(LOG_CRIT, "Last SQL command: %s", sql);
		sqlite3_free(zErrMsg);
		return TC_API_DB_ERROR;
	}

	write_log(LOG_DEBUG, "Added action to Database (action index %d, action family type %d, nps_index 0x%x)",
		  tc_action_params->general.index,
		  tc_action_params->general.family_type,
		  action_info->nps_index);

	sqlite3_finalize(statement);

	return TC_API_OK;
}

/******************************************************************************
 * \brief	delete action to DB
 *
 * \param[in]   tc_action_params    - action configuration
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc delete_tc_action_from_db(struct tc_action *tc_action_params)
{
	enum tc_api_rc rc;
	char *zErrMsg = NULL;
	char sql[TC_DB_SQL_COMMAND_SIZE];

	snprintf(sql, TC_DB_SQL_COMMAND_SIZE,
		 "DELETE FROM actions_table "
		 "WHERE linux_action_index=%d AND action_family_type=%d;",
		 tc_action_params->general.index,
		 tc_action_params->general.family_type);

	/* Execute SQL statement */
	rc = sqlite3_exec(tc_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return TC_API_DB_ERROR;
	}

	return TC_API_OK;
}

/******************************************************************************
 * \brief	modify tc action on DB
 *
 * \param[in]   tc_action_params  - holds action index and family type
 * \param[in]   action_info       - action extra information (NPS data, entry index, statistics)
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc modify_tc_action_on_db(struct tc_action *tc_action_params, struct action_info *action_info)
{
	enum tc_api_rc rc;
	char *zErrMsg = NULL;
	char sql[TC_DB_SQL_COMMAND_SIZE];
	sqlite3_stmt *statement;

	snprintf(sql, TC_DB_SQL_COMMAND_SIZE,
		 "UPDATE actions_table "
		 "SET bind_value=%d, capab=%d, refcnt=%d, action_type=%d, action_data=? "
		 "WHERE linux_action_index=%d AND action_family_type=%d;",
		 tc_action_params->general.bindcnt,		/* bind_value */
		 tc_action_params->general.capab,		/* capab */
		 tc_action_params->general.refcnt,		/* refcnt */
		 tc_action_params->general.type,		/* action_type */
		 tc_action_params->general.index,		/* linux_action_index */
		 tc_action_params->general.family_type);	/* action_family_type */

	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		return TC_API_DB_ERROR;
	}

	rc = sqlite3_bind_blob(statement, 1, action_info, sizeof(struct action_info), SQLITE_STATIC);

	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		return TC_API_DB_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);
	if (rc != SQLITE_DONE) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		write_log(LOG_CRIT, "Last SQL command: %s", sql);
		sqlite3_free(zErrMsg);
		return TC_API_DB_ERROR;
	}

	write_log(LOG_DEBUG, "Modified action on Database (action index %d, action type %d)",
		  tc_action_params->general.index,
		  tc_action_params->general.family_type);

	sqlite3_finalize(statement);

	return TC_API_OK;
}

/******************************************************************************
 * \brief	get action from DB
 *
 * \param[in]   tc_action_params  - holds action index and family type
 * \param[out]  is_action_exist   - bool, set to true if action exist on DB
 * \param[out]  action_info       - action extra information (NPS data, entry index, statistics)
 * \param[out]  bind_count        - action bind cound value
 * \param[out]  tc_action_from_db - action configuration received from DB
 *
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_tc_action_from_db(struct tc_action   *tc_action_params,
				     bool               *is_action_exist,
				     struct action_info *action_info,
				     int                *bind_count,
				     struct tc_action   *tc_action_from_db)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[TC_DB_SQL_COMMAND_SIZE];
	uint32_t action_bytes = 0;

	snprintf(sql,
		 TC_DB_SQL_COMMAND_SIZE,
		 "SELECT * FROM actions_table WHERE "
		 "linux_action_index=%d AND action_family_type=%d;",
		 tc_action_params->general.index,
		 tc_action_params->general.family_type);

	write_log(LOG_DEBUG, "sql command: %s", sql);
	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		return TC_API_DB_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	/* action entry not found */
	if (rc == SQLITE_DONE) {
		write_log(LOG_INFO, "Action (index %d, type %d) wasn't found on internal DB", tc_action_params->general.index, tc_action_params->general.family_type);
		*is_action_exist = false;
		return TC_API_OK;
	}

	write_log(LOG_NOTICE, "Action (index %d, type %d) was found on internal DB", tc_action_params->general.index, tc_action_params->general.family_type);
	*is_action_exist = true;

	if (action_info != NULL) {
		/* Retrieve action info */
		action_bytes = sqlite3_column_bytes(statement, 10);
		memcpy(action_info, sqlite3_column_blob(statement, 10), action_bytes);
	}

	if (tc_action_from_db != NULL) {
		struct action_info l_action_info;

		tc_action_from_db->general.index                       = sqlite3_column_int(statement, 0);
		tc_action_from_db->general.family_type                 = sqlite3_column_int(statement, 1);
		tc_action_from_db->action_statistics.created_timestamp = sqlite3_column_int(statement, 2);
		tc_action_from_db->general.bindcnt                     = sqlite3_column_int(statement, 6);
		tc_action_from_db->general.capab                       = sqlite3_column_int(statement, 7);
		tc_action_from_db->general.refcnt                      = sqlite3_column_int(statement, 8);
		tc_action_from_db->general.type                        = sqlite3_column_int(statement, 9);

		action_bytes = sqlite3_column_bytes(statement, 10);
		memcpy(&l_action_info, sqlite3_column_blob(statement, 10), action_bytes);
		memcpy(&tc_action_from_db->action_data, &l_action_info.action_data, sizeof(tc_action_from_db->action_data));
	}

	if (bind_count != NULL) {
		*bind_count = sqlite3_column_int(statement, 6);
	}

	sqlite3_finalize(statement);

	return TC_API_OK;
}

/******************************************************************************
 * \brief	check if TC action exist on DB
 *
 * \param[in]   tc_action_params  - holds action index and family type
 * \param[out]  is_action_exist   - bool, set to true if action exist on DB
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc check_if_tc_action_exist(struct tc_action *tc_action_params, bool *is_action_exist)
{
	return get_tc_action_from_db(tc_action_params, is_action_exist, NULL, NULL, NULL);
}

/******************************************************************************
 * \brief	get number of actions on DB from a specific family type
 *
 * \param[in]   type           - family type of actions
 * \param[out]  num_of_actions - number of actions that we found
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_type_num_of_actions_from_db(enum tc_action_type type, uint32_t *num_of_actions)
{

	int rc;
	sqlite3_stmt *statement;
	char sql[TC_DB_SQL_COMMAND_SIZE];

	/* check if exist on sql filter table another entry with the same mask, table but different priority */
	snprintf(sql,
		 TC_DB_SQL_COMMAND_SIZE,
		 "SELECT COUNT (*) AS actions_count FROM actions_table WHERE "
		 "action_family_type=%d;",
				 type);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		return TC_API_DB_ERROR;
	}
	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	/* mask not found */
	if (rc == SQLITE_DONE) {
		write_log(LOG_DEBUG, "Not Found mask on mask_table");
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	*num_of_actions = sqlite3_column_int(statement, 0);

	sqlite3_finalize(statement);

	return TC_API_OK;
}

/******************************************************************************
 * \brief	get the array of actions from DB that belong to this family type
 *
 * \param[in]   type           - family type of actions
 * \param[out]  actions_array  - array (size num_of_actions) of the action that we found
 * \param[in]   num_of_actions - number of actions that we found
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_type_action_indexes_from_db(enum tc_action_type type, uint32_t *actions_array, uint32_t num_of_actions)
{
	int rc;
	uint32_t i;
	sqlite3_stmt *statement;
	char sql[TC_DB_SQL_COMMAND_SIZE];

	snprintf(sql,
		 TC_DB_SQL_COMMAND_SIZE,
		 "SELECT * FROM actions_table WHERE "
		 "action_family_type=%d;",
		 type);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		return TC_API_DB_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	i = 0;
	while (rc == SQLITE_ROW) {

		if (i >= num_of_actions) {
			write_log(LOG_ERR, "found too many results on this query");
			return TC_API_FAILURE;
		}

		actions_array[i] = sqlite3_column_int(statement, 0);

		write_log(LOG_DEBUG, "add action index (linux) 0x%08x to array of actions (type %d)", actions_array[i], type);

		rc = sqlite3_step(statement);

		i++;
	}

	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	/* finalize SQL statement */
	sqlite3_finalize(statement);

	return TC_API_OK;
}

/******************************************************************************
 * \brief	increment bind value on DB
 *
 * \param[in]   tc_action - reference to the action configuration
 *
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc increment_tc_action_bind_value(struct tc_action *tc_action)
{
	bool is_action_exist;
	enum tc_api_rc rc;
	char *zErrMsg = NULL;
	char sql[TC_DB_SQL_COMMAND_SIZE];
	struct tc_action action_on_db;

	memset(&action_on_db, 0, sizeof(struct tc_action));

	TC_CHECK_ERROR(get_tc_action_from_db(tc_action, &is_action_exist, NULL, NULL, &action_on_db));

	action_on_db.general.bindcnt++;
	action_on_db.general.refcnt++;

	snprintf(sql, TC_DB_SQL_COMMAND_SIZE,
		 "UPDATE actions_table "
		 "SET bind_value=%d, refcnt=%d "
		 "WHERE linux_action_index=%d AND action_family_type=%d;",
		 action_on_db.general.bindcnt,		/* bind_value */
		 action_on_db.general.refcnt,		/* bind_value */
		 tc_action->general.index,		/* linux_action_index */
		 tc_action->general.family_type);	/* action_family_type */

	/* Execute SQL statement */
	rc = sqlite3_exec(tc_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return TC_API_DB_ERROR;
	}

	write_log(LOG_DEBUG, "Bind value & refcnt value were updated to value (bind=%d, refcnt=%d) on action on Database (action index %d, action type %d)",
		  action_on_db.general.bindcnt,
		  action_on_db.general.refcnt,
		  tc_action->general.index,
		  tc_action->general.family_type);

	return TC_API_OK;
}

/******************************************************************************
 * \brief	decrement bind value on DB
 *
 * \param[in]   tc_action  - reference to the action configuration
 * \param[out]  bind_value - reference to the new bind value
 *
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc decrement_tc_action_bind_value(struct tc_action *tc_action, int *bind_value)
{

	bool is_action_exist;
	enum tc_api_rc rc;
	char *zErrMsg = NULL;
	char sql[TC_DB_SQL_COMMAND_SIZE];
	struct tc_action action_on_db;

	memset(&action_on_db, 0, sizeof(struct tc_action));

	TC_CHECK_ERROR(get_tc_action_from_db(tc_action, &is_action_exist, NULL, NULL, &action_on_db));

	action_on_db.general.bindcnt--;
	*bind_value = action_on_db.general.bindcnt;

	action_on_db.general.refcnt--;

	snprintf(sql, TC_DB_SQL_COMMAND_SIZE,
		 "UPDATE actions_table "
		 "SET bind_value=%d, refcnt=%d "
		 "WHERE linux_action_index=%d AND action_family_type=%d;",
		 action_on_db.general.bindcnt,		/* bind_value */
		 action_on_db.general.refcnt,		/* bind_value */
		 tc_action->general.index,		/* linux_action_index */
		 tc_action->general.family_type);	/* action_family_type */

	/* Execute SQL statement */
	rc = sqlite3_exec(tc_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return TC_API_DB_ERROR;
	}

	write_log(LOG_DEBUG, "Bind value & refcnt value were updated to value (bind=%d, refcnt=%d) on action on Database (action index %d, action type %d)",
		  action_on_db.general.bindcnt,
		  action_on_db.general.refcnt,
		  tc_action->general.index,
		  tc_action->general.family_type);

	return TC_API_OK;
}

/****************************************************************************************************/
/************************************ Flower Filter Table *******************************************/
/****************************************************************************************************/

/******************************************************************************
 * \brief	add flower filter to DB
 *
 * \param[in]   tc_filter_params     - filter configurations
 * \param[in]   filter_actions_index - filter actions index of this filter
 * \param[in]   rule_list_index      - rule list index of this filter
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc tc_add_flower_filter_to_db(struct tc_filter *tc_filter_params,
					  uint32_t filter_actions_index,
					  uint16_t rule_list_index)
{
	enum tc_api_rc rc;
	char *zErrMsg = NULL;
	char sql[TC_DB_SQL_COMMAND_SIZE];
	sqlite3_stmt *statement;
	uint32_t i;
	struct action_id actions_array[MAX_NUM_OF_ACTIONS_IN_FILTER];

	uint64_t eth_src_key_uint64 = 0;
	uint64_t eth_dst_key_uint64 = 0;
	uint64_t eth_src_mask_uint64 = 0;
	uint64_t eth_dst_mask_uint64 = 0;

	memcpy(&eth_dst_key_uint64, tc_filter_params->flower_rule_policy.key_eth_dst, ETH_ALEN);
	memcpy(&eth_src_key_uint64, tc_filter_params->flower_rule_policy.key_eth_src, ETH_ALEN);
	memcpy(&eth_dst_mask_uint64, tc_filter_params->flower_rule_policy.mask_eth_dst, ETH_ALEN);
	memcpy(&eth_src_mask_uint64, tc_filter_params->flower_rule_policy.mask_eth_src, ETH_ALEN);

	snprintf(sql, TC_DB_SQL_COMMAND_SIZE,
		 "INSERT INTO filters_table "
		 "(interface,"
		 "priority,"
		 "handle,"
		 "num_of_actions,"
		 "actions_array,"
		 /* multi priority list info */
		"rule_list_index,"
		/* filter key */
		"eth_dst_key,"
		"eth_src_key,"
		"eth_type_key,"
		"ip_proto_key,"
		"ipv4_src_key,"
		"ipv4_dst_key,"
		"l4_src_key,"
		"l4_dst_key,"
		/* filter mask */
		"eth_dst_mask,"
		"eth_src_mask,"
		"eth_type_mask,"
		"ip_proto_mask,"
		"ipv4_src_mask,"
		"ipv4_dst_mask,"
		"l4_src_mask,"
		"l4_dst_mask, "
		"mask_flow_bitmap, "
		"filter_actions_index, "
		"protocol) "
		 "VALUES (%d, %d, %d, %d, ?, %d, %ld, %ld, %d, %d, %d, %d, %d, %d, %ld, %ld, %d, %d, %d, %d, %d, %d, %d, %d, %d);",
		 tc_filter_params->ifindex,			/* interface */
		 tc_filter_params->priority,			/* priority */
		 tc_filter_params->handle,			/* handle */
		 tc_filter_params->actions.num_of_actions,	/* num_of_actions */
		 rule_list_index,				/* current_filter_rule_index */
		 eth_dst_key_uint64,					/* eth_dst_key */
		 eth_src_key_uint64,					/* eth_src_key */
		 tc_filter_params->flower_rule_policy.key_eth_type,	/* eth_type_key */
		 tc_filter_params->flower_rule_policy.key_ip_proto,	/* ip_proto_key */
		 tc_filter_params->flower_rule_policy.key_ipv4_src,	/* ipv4_src_key */
		 tc_filter_params->flower_rule_policy.key_ipv4_dst,	/* ipv4_dst_key */
		 tc_filter_params->flower_rule_policy.key_l4_src,	/* l4_src_key */
		 tc_filter_params->flower_rule_policy.key_l4_dst,	/* l4_dst_key */
		 eth_dst_mask_uint64,					/* eth_dst_mask */
		 eth_src_mask_uint64,					/* eth_src_mask */
		 tc_filter_params->flower_rule_policy.mask_eth_type,	/* eth_type_mask */
		 tc_filter_params->flower_rule_policy.mask_ip_proto,	/* ip_proto_mask */
		 tc_filter_params->flower_rule_policy.mask_ipv4_src,	/* ipv4_src_mask */
		 tc_filter_params->flower_rule_policy.mask_ipv4_dst,	/* ipv4_dst_mask */
		 tc_filter_params->flower_rule_policy.mask_l4_src,	/* l4_src_mask */
		 tc_filter_params->flower_rule_policy.mask_l4_dst,	/* l4_dst_mask */
		 tc_filter_params->flower_rule_policy.mask_bitmap.raw_data, /* mask_flow_bitmap */
		 filter_actions_index,					/* filter_actions_index */
		 tc_filter_params->protocol);				/* protocol */

	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		return TC_API_DB_ERROR;
	}

	/* fill the actions data (type & index) */
	for (i = 0; i < tc_filter_params->actions.num_of_actions; i++) {
		actions_array[i].action_family_type = tc_filter_params->actions.action[i].general.family_type;
		actions_array[i].linux_action_index = tc_filter_params->actions.action[i].general.index;
	}

	rc = sqlite3_bind_blob(statement, 1, actions_array, tc_filter_params->actions.num_of_actions*sizeof(struct action_id), SQLITE_STATIC);

	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		return TC_API_DB_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);
	if (rc != SQLITE_DONE) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		write_log(LOG_CRIT, "Failed to bind blob actions: return message %d", rc);
		sqlite3_free(zErrMsg);
		return TC_API_DB_ERROR;
	}

	write_log(LOG_DEBUG, "Filter with priority %d, handle %d and interface %d was added to db successfully",
		  tc_filter_params->priority,
		  tc_filter_params->handle,
		  tc_filter_params->ifindex);

	sqlite3_finalize(statement);

	return TC_API_OK;
}

/******************************************************************************
 * \brief	modify flower filter on DB
 *
 * \param[in]   tc_filter_params     - reference to filter configurations
 * \param[in]   action_info_array    - array of all the actions on this filter (each element has action information)
 * \param[in]   filter_actions_index - filter actions index of this filter
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc tc_modify_flower_filter_on_db(struct tc_filter *tc_filter_params,
					     struct action_info *action_info_array,
					     uint32_t filter_actions_index)
{
	enum tc_api_rc rc;
	char *zErrMsg = NULL;
	char sql[TC_DB_SQL_COMMAND_SIZE];
	sqlite3_stmt *statement;
	struct action_id action_id_array[MAX_NUM_OF_ACTIONS_IN_FILTER];
	uint32_t i;
	uint64_t eth_src_key_uint64 = 0;
	uint64_t eth_dst_key_uint64 = 0;

	memcpy(&eth_dst_key_uint64, tc_filter_params->flower_rule_policy.key_eth_dst, ETH_ALEN);
	memcpy(&eth_src_key_uint64, tc_filter_params->flower_rule_policy.key_eth_src, ETH_ALEN);

	snprintf(sql, TC_DB_SQL_COMMAND_SIZE,
		 "UPDATE filters_table "
		 "SET interface=%d, "
		 "num_of_actions=%d, "
		 "actions_array=?, "
		 "eth_dst_key=%ld, "
		 "eth_src_key=%ld, "
		 "eth_type_key=%d, "
		 "ip_proto_key=%d, "
		 "ipv4_src_key=%d, "
		 "ipv4_dst_key=%d, "
		 "l4_src_key=%d, "
		 "l4_dst_key=%d, "
		 "filter_actions_index=%d, "
		 "protocol=%d "
		 "WHERE priority=%d AND handle=%d AND interface=%d;",
		 tc_filter_params->ifindex,			/* interface */
		 tc_filter_params->actions.num_of_actions,	/* num_of_actions */
		 eth_dst_key_uint64,					/* eth_dst_key */
		 eth_src_key_uint64,					/* eth_src_key */
		 tc_filter_params->flower_rule_policy.key_eth_type,	/* eth_type_key */
		 tc_filter_params->flower_rule_policy.key_ip_proto,	/* ip_proto_key */
		 tc_filter_params->flower_rule_policy.key_ipv4_src,	/* ipv4_src_key */
		 tc_filter_params->flower_rule_policy.key_ipv4_dst,	/* ipv4_dst_key */
		 tc_filter_params->flower_rule_policy.key_l4_src,	/* l4_src_key */
		 tc_filter_params->flower_rule_policy.key_l4_dst,
		 filter_actions_index,
		 tc_filter_params->protocol,
		 tc_filter_params->priority,
		 tc_filter_params->handle,
		 tc_filter_params->ifindex);				/* l4_dst_key */

	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		return TC_API_DB_ERROR;
	}

	for (i = 0; i < tc_filter_params->actions.num_of_actions; i++) {
		action_id_array[i].action_family_type = action_info_array[i].action_id.action_family_type;
		action_id_array[i].linux_action_index = action_info_array[i].action_id.linux_action_index;
		write_log(LOG_DEBUG, "action_id_array[%d].action_family_type = %d", i, action_id_array[i].action_family_type);
		write_log(LOG_DEBUG, "action_id_array[%d].linux_action_index = %d", i, action_id_array[i].linux_action_index);
	}

	rc = sqlite3_bind_blob(statement, 1, action_id_array, tc_filter_params->actions.num_of_actions*sizeof(struct action_id), SQLITE_STATIC);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		return TC_API_DB_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);
	if (rc != SQLITE_DONE) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		write_log(LOG_CRIT, "Last SQL command: %s", sql);
		sqlite3_free(zErrMsg);
		return TC_API_DB_ERROR;
	}

	sqlite3_finalize(statement);

	return TC_API_OK;
}

/******************************************************************************
 * \brief	check if the handle of the filter is the highest in priority
 *
 * \param[in]   tc_filter_params - reference to filter configurations
 * \param[in]   result           - reference to boolean variable, true if the handle is the highest or false if not
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc check_if_handle_is_highest(struct tc_filter *tc_filter_params, bool *result)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[TC_DB_SQL_COMMAND_SIZE];
	uint64_t eth_src_key_uint64 = 0;
	uint64_t eth_dst_key_uint64 = 0;
	uint64_t eth_src_mask_uint64 = 0;
	uint64_t eth_dst_mask_uint64 = 0;
	struct tc_flower_rule_policy *flower_rule_policy;
	uint32_t highest_handle_found, num_of_handles_found;

	flower_rule_policy = &tc_filter_params->flower_rule_policy;

	memcpy(&eth_dst_key_uint64, flower_rule_policy->key_eth_dst, ETH_ALEN);
	memcpy(&eth_src_key_uint64, flower_rule_policy->key_eth_src, ETH_ALEN);
	memcpy(&eth_dst_mask_uint64, flower_rule_policy->mask_eth_dst, ETH_ALEN);
	memcpy(&eth_src_mask_uint64, flower_rule_policy->mask_eth_src, ETH_ALEN);

	/* check if exist on sql filter table another entry with the same mask, table but different priority */
	snprintf(sql,
		 TC_DB_SQL_COMMAND_SIZE,
		 "SELECT MAX(handle) as max_handle, COUNT(handle) as num_of_handles "
		 "FROM filters_table WHERE "
		 /* filter key */
		 "interface = %d AND "
		 "eth_dst_key = %ld AND "
		 "eth_src_key = %ld AND "
		 "eth_type_key = %d AND "
		 "ip_proto_key = %d AND "
		 "ipv4_src_key = %d AND "
		 "ipv4_dst_key = %d AND "
		 "l4_src_key = %d AND "
		 "l4_dst_key = %d AND "
		 /* filter mask */
		 "eth_dst_mask = %ld AND "
		 "eth_src_mask = %ld AND "
		 "eth_type_mask = %d AND "
		 "ip_proto_mask = %d AND "
		 "ipv4_src_mask = %d AND "
		 "ipv4_dst_mask = %d AND "
		 "l4_src_mask = %d AND "
		 "l4_dst_mask = %d AND "
		 "priority = %d;",
		 tc_filter_params->ifindex,
		 eth_dst_key_uint64,
		 eth_src_key_uint64,
		 flower_rule_policy->key_eth_type,
		 flower_rule_policy->key_ip_proto,
		 flower_rule_policy->key_ipv4_src,
		 flower_rule_policy->key_ipv4_dst,
		 flower_rule_policy->key_l4_src,
		 flower_rule_policy->key_l4_dst,
		 eth_dst_mask_uint64,
		 eth_src_mask_uint64,
		 flower_rule_policy->mask_eth_type,
		 flower_rule_policy->mask_ip_proto,
		 flower_rule_policy->mask_ipv4_src,
		 flower_rule_policy->mask_ipv4_dst,
		 flower_rule_policy->mask_l4_src,
		 flower_rule_policy->mask_l4_dst,
		 tc_filter_params->priority);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		return TC_API_DB_ERROR;
	}
	/* Execute SQL statement */
	rc = sqlite3_step(statement);
	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}
	/* Filter not found */
	if (rc == SQLITE_DONE) {
		write_log(LOG_DEBUG, "Not Found filter with the same key, mask & priority");
		sqlite3_finalize(statement);
		*result = true;
		return TC_API_OK;
	}

	highest_handle_found = sqlite3_column_int(statement, 0);
	num_of_handles_found = sqlite3_column_int(statement, 1);

	write_log(LOG_DEBUG, "Highest handle from the same priority (%d) found is %d, Number of Handles found is %d",
		  tc_filter_params->priority, highest_handle_found, num_of_handles_found);
	write_log(LOG_DEBUG, "Our handle value is %d", tc_filter_params->handle);
	/* check if it is the highest handle in the same key,mask,priority */
	if (highest_handle_found > tc_filter_params->handle) {
		*result = false;
	} else {
		/* this filter handle is the highest or equal to the highest (if already inserted this filter do db) */
		*result = true;
	}

	sqlite3_finalize(statement);

	return TC_API_OK;
}

/******************************************************************************
 * \brief	get the size of the rule list (rule list are all the filters that have the same key&mask and different priority)
 *
 * \param[in]   tc_filter_params - reference to filter configurations
 * \param[out]  rules_list_size  - reference to rule list size (uint32)
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_rules_list_size(struct tc_filter *tc_filter_params, uint32_t *rules_list_size)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[TC_DB_SQL_COMMAND_SIZE];
	uint64_t eth_src_key_uint64 = 0;
	uint64_t eth_dst_key_uint64 = 0;
	uint64_t eth_src_mask_uint64 = 0;
	uint64_t eth_dst_mask_uint64 = 0;
	struct tc_flower_rule_policy *flower_rule_policy;

	flower_rule_policy = &tc_filter_params->flower_rule_policy;

	memcpy(&eth_dst_key_uint64, flower_rule_policy->key_eth_dst, ETH_ALEN);
	memcpy(&eth_src_key_uint64, flower_rule_policy->key_eth_src, ETH_ALEN);
	memcpy(&eth_dst_mask_uint64, flower_rule_policy->mask_eth_dst, ETH_ALEN);
	memcpy(&eth_src_mask_uint64, flower_rule_policy->mask_eth_src, ETH_ALEN);

	/* check if exist on sql filter table another entry with the same mask, table but different priority */
	snprintf(sql,
		 TC_DB_SQL_COMMAND_SIZE,
		 "SELECT COUNT (DISTINCT priority) as rules_list_size FROM filters_table WHERE "
		 /* filter key */
		 "interface = %d AND "
		 "eth_dst_key = %ld AND "
		 "eth_src_key = %ld AND "
		 "eth_type_key = %d AND "
		 "ip_proto_key = %d AND "
		 "ipv4_src_key = %d AND "
		 "ipv4_dst_key = %d AND "
		 "l4_src_key = %d AND "
		 "l4_dst_key = %d AND "
		 /* filter mask */
		 "eth_dst_mask = %ld AND "
		 "eth_src_mask = %ld AND "
		 "eth_type_mask = %d AND "
		 "ip_proto_mask = %d AND "
		 "ipv4_src_mask = %d AND "
		 "ipv4_dst_mask = %d AND "
		 "l4_src_mask = %d AND "
		 "l4_dst_mask = %d;",
		 tc_filter_params->ifindex,
		 eth_dst_key_uint64,
		 eth_src_key_uint64,
		 flower_rule_policy->key_eth_type,
		 flower_rule_policy->key_ip_proto,
		 flower_rule_policy->key_ipv4_src,
		 flower_rule_policy->key_ipv4_dst,
		 flower_rule_policy->key_l4_src,
		 flower_rule_policy->key_l4_dst,
		 eth_dst_mask_uint64,
		 eth_src_mask_uint64,
		 flower_rule_policy->mask_eth_type,
		 flower_rule_policy->mask_ip_proto,
		 flower_rule_policy->mask_ipv4_src,
		 flower_rule_policy->mask_ipv4_dst,
		 flower_rule_policy->mask_l4_src,
		 flower_rule_policy->mask_l4_dst);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		return TC_API_DB_ERROR;
	}
	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* mask not found */
	if (rc != SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	*rules_list_size = sqlite3_column_int(statement, 0);

	sqlite3_finalize(statement);

	return TC_API_OK;
}

/******************************************************************************
 * \brief	prepare the rule list items on an array
 *
 * \param[in]   tc_filter_params - reference to filter configurations
 * \param[in]   rules_list_size  - reference to rule list array
 * \param[in]   rules_list_size  - reference to rule list size (uint32)
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc prepare_rules_list_items(struct tc_filter *tc_filter_params,
					struct rules_list_item *rules_list,
					uint32_t rules_list_size)
{
	uint32_t i;
	int rc;
	sqlite3_stmt *statement;
	char sql[TC_DB_SQL_COMMAND_SIZE];
	uint64_t eth_src_key_uint64 = 0;
	uint64_t eth_dst_key_uint64 = 0;
	uint64_t eth_src_mask_uint64 = 0;
	uint64_t eth_dst_mask_uint64 = 0;
	struct tc_flower_rule_policy *flower_rule_policy;

	flower_rule_policy = &tc_filter_params->flower_rule_policy;

	memcpy(&eth_dst_key_uint64, flower_rule_policy->key_eth_dst, ETH_ALEN);
	memcpy(&eth_src_key_uint64, flower_rule_policy->key_eth_src, ETH_ALEN);
	memcpy(&eth_dst_mask_uint64, flower_rule_policy->mask_eth_dst, ETH_ALEN);
	memcpy(&eth_src_mask_uint64, flower_rule_policy->mask_eth_src, ETH_ALEN);

	/* check if exist on sql filter table another entry with the same mask, table but different priority */
	snprintf(sql,
		 TC_DB_SQL_COMMAND_SIZE,
		 "SELECT * FROM filters_table WHERE "
		 /* filter key */
		 "interface = %d AND "
		 "eth_dst_key = %ld AND "
		 "eth_src_key = %ld AND "
		 "eth_type_key = %d AND "
		 "ip_proto_key = %d AND "
		 "ipv4_src_key = %d AND "
		 "ipv4_dst_key = %d AND "
		 "l4_src_key = %d AND "
		 "l4_dst_key = %d AND "
		 /* filter mask */
		 "eth_dst_mask = %ld AND "
		 "eth_src_mask = %ld AND "
		 "eth_type_mask = %d AND "
		 "ip_proto_mask = %d AND "
		 "ipv4_src_mask = %d AND "
		 "ipv4_dst_mask = %d AND "
		 "l4_src_mask = %d AND "
		 "l4_dst_mask = %d "
		 "GROUP BY priority "
		 "ORDER BY priority ASC, handle DESC;",
		 tc_filter_params->ifindex,
		 eth_dst_key_uint64,
		 eth_src_key_uint64,
		 flower_rule_policy->key_eth_type,
		 flower_rule_policy->key_ip_proto,
		 flower_rule_policy->key_ipv4_src,
		 flower_rule_policy->key_ipv4_dst,
		 flower_rule_policy->key_l4_src,
		 flower_rule_policy->key_l4_dst,
		 eth_dst_mask_uint64,
		 eth_src_mask_uint64,
		 flower_rule_policy->mask_eth_type,
		 flower_rule_policy->mask_ip_proto,
		 flower_rule_policy->mask_ipv4_src,
		 flower_rule_policy->mask_ipv4_dst,
		 flower_rule_policy->mask_l4_src,
		 flower_rule_policy->mask_l4_dst);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		return TC_API_DB_ERROR;
	}
	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	i = 0;
	while (rc == SQLITE_ROW && i < rules_list_size) {

		/* add item to rules list */
		rules_list[i].filter_actions_index = sqlite3_column_int(statement, 23);
		rules_list[i].priority = sqlite3_column_int(statement, 1);
		rules_list[i].rules_list_index = sqlite3_column_int(statement, 5);

		write_log(LOG_DEBUG,
			  "item %d on rules list is with priority %d and filter actions index %u, rules_list index %d",
			  i,
			  rules_list[i].priority,
			  rules_list[i].filter_actions_index,
			  rules_list[i].rules_list_index);

		rc = sqlite3_step(statement);

		i++;
	}

	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	/* finalize SQL statement */
	sqlite3_finalize(statement);

	return TC_API_OK;
}

/******************************************************************************
 * \brief	remove filter from DB
 *
 * \param[in]   tc_filter_params - reference to filter configurations
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc remove_filter_from_db(struct tc_filter *tc_filter_params)
{
	enum tc_api_rc rc;
	char *zErrMsg = NULL;
	char sql[TC_DB_SQL_COMMAND_SIZE];

	snprintf(sql, TC_DB_SQL_COMMAND_SIZE,
		 "DELETE FROM filters_table "
		 "WHERE interface=%d AND priority=%d AND handle=%d;",
		 tc_filter_params->ifindex,
		 tc_filter_params->priority,
		 tc_filter_params->handle);

	/* Execute SQL statement */
	rc = sqlite3_exec(tc_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return TC_API_DB_ERROR;
	}

	return TC_API_OK;
}

/******************************************************************************
 * \brief	check if filter exist on DB
 *
 * \param[in]   tc_filter_params - reference to filter configurations
 * \param[out]  is_exist - reference to boolean, return true if exist, false if not exist
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc check_if_filter_exist_on_db(struct tc_filter *tc_filter_params, bool *is_exist)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[TC_DB_SQL_COMMAND_SIZE];

	snprintf(sql,
		 TC_DB_SQL_COMMAND_SIZE,
		 "SELECT * FROM filters_table WHERE "
		 "handle=%d AND priority=%d AND interface=%d;",
		 tc_filter_params->handle,
		 tc_filter_params->priority,
		 tc_filter_params->ifindex);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		return TC_API_DB_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	/* filter entry not found */
	if (rc == SQLITE_DONE) {
		write_log(LOG_NOTICE, "Filter wasnt found on internal DB");
		*is_exist = false;
	} else {
		*is_exist = true;
	}

	sqlite3_finalize(statement);

	return TC_API_OK;
}

/******************************************************************************
 * \brief	get filter rule list index
 *
 * \param[in]   tc_filter_params - reference to filter configurations
 * \param[out]  filter_rule_index - reference to filter rule index
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_filter_rule_from_db(struct tc_filter *tc_filter_params, uint16_t *filter_rule_index)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[TC_DB_SQL_COMMAND_SIZE];

	snprintf(sql,
		 TC_DB_SQL_COMMAND_SIZE,
		 "SELECT * FROM filters_table WHERE "
		 "handle=%d AND priority=%d AND interface=%d;",
		 tc_filter_params->handle,
		 tc_filter_params->priority,
		 tc_filter_params->ifindex);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		return TC_API_DB_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	/* filter entry not found */
	if (rc == SQLITE_DONE) {
		write_log(LOG_NOTICE, "Filter wasn't found on internal DB");
		sqlite3_finalize(statement);
		return TC_API_FAILURE;
	}

	*filter_rule_index = sqlite3_column_int(statement, 5);

	sqlite3_finalize(statement);

	return TC_API_OK;
}

/******************************************************************************
 * \brief	check if filter mask is already exist on DB
 *
 * \param[in]   tc_filter_params - reference to filter configurations
 * \param[out]  is_mask_exist - reference to boolean, true if exist
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc check_if_filters_registered_on_mask(struct tc_filter *tc_filter_params, bool *is_mask_exist)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[TC_DB_SQL_COMMAND_SIZE];
	uint64_t eth_src_mask_uint64 = 0;
	uint64_t eth_dst_mask_uint64 = 0;
	struct tc_flower_rule_policy *flower_rule_policy;
	uint32_t mask_count;

	flower_rule_policy = &tc_filter_params->flower_rule_policy;

	memcpy(&eth_dst_mask_uint64, flower_rule_policy->mask_eth_dst, ETH_ALEN);
	memcpy(&eth_src_mask_uint64, flower_rule_policy->mask_eth_src, ETH_ALEN);

	/* check if exist on sql filter table another entry with the same mask, table but different priority */
	snprintf(sql,
		 TC_DB_SQL_COMMAND_SIZE,
		 "SELECT COUNT (*) AS mask_count FROM filters_table WHERE "
		 "interface=%d AND "
		 "eth_type_mask=%d AND "
		 "eth_dst_mask=%ld AND "
		 "eth_src_mask=%ld AND "
		 "ip_proto_mask=%d AND "
		 "ipv4_src_mask=%d AND "
		 "ipv4_dst_mask=%d AND "
		 "l4_src_mask=%d AND "
		 "l4_dst_mask=%d AND "
		 "mask_flow_bitmap=%d;",
		 tc_filter_params->ifindex,
		 flower_rule_policy->mask_eth_type,
		 eth_dst_mask_uint64,
		 eth_src_mask_uint64,
		 flower_rule_policy->mask_ip_proto,
		 flower_rule_policy->mask_ipv4_src,
		 flower_rule_policy->mask_ipv4_dst,
		 flower_rule_policy->mask_l4_src,
		 flower_rule_policy->mask_l4_dst,
		 flower_rule_policy->mask_bitmap.raw_data);

	/* Prepare SQL statement */
	write_log(LOG_DEBUG, "SQL command: %s", sql);
	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		return TC_API_DB_ERROR;
	}
	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	/* mask not found */
	if (rc == SQLITE_DONE) {
		write_log(LOG_DEBUG, "Not Found mask on mask_table");
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	mask_count = sqlite3_column_int(statement, 0);

	write_log(LOG_INFO, "mask_count = %d", mask_count);
	if (mask_count == 0) {
		*is_mask_exist = false;
		write_log(LOG_DEBUG, "this mask is not exist on masks table");
	} else {
		*is_mask_exist = true;
		write_log(LOG_DEBUG, "this mask is already exist on masks table");
	}

	sqlite3_finalize(statement);

	return TC_API_OK;
}

/******************************************************************************
 * \brief	get the key&mask of the old filter (used this function on modify - modify can also change key&mask of filter)
 *
 * \param[in]   tc_filter_params     - reference to filter configurations
 * \param[out]  old_tc_filter_params - reference to the old tc filter configuration (key & mask)
 * \param[out]  is_key_changed       - reference to boolean, true if changed
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_old_filter_key(struct tc_filter *tc_filter_params, struct tc_filter *old_tc_filter_params, bool *is_key_changed)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[TC_DB_SQL_COMMAND_SIZE];
	uint64_t temp_mac_addr = 0;

	snprintf(sql,
		 TC_DB_SQL_COMMAND_SIZE,
		 "SELECT * FROM filters_table WHERE "
		 "handle=%d AND priority=%d AND interface=%d;",
		 tc_filter_params->handle,
		 tc_filter_params->priority,
		 tc_filter_params->ifindex);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		return TC_API_DB_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	/* filter entry not found */
	if (rc == SQLITE_DONE) {
		write_log(LOG_NOTICE, "Filter wasnwt found on internal DB");
		sqlite3_finalize(statement);
		return TC_API_FAILURE;
	}

	/* get filter key */
	temp_mac_addr = sqlite3_column_int64(statement, 6);
	memcpy(&old_tc_filter_params->flower_rule_policy.key_eth_dst, &temp_mac_addr, ETH_ALEN);
	temp_mac_addr = sqlite3_column_int64(statement, 7);
	memcpy(&old_tc_filter_params->flower_rule_policy.key_eth_src, &temp_mac_addr, ETH_ALEN);
	old_tc_filter_params->flower_rule_policy.key_eth_type = sqlite3_column_int(statement, 8);
	old_tc_filter_params->flower_rule_policy.key_ip_proto = sqlite3_column_int(statement, 9);
	old_tc_filter_params->flower_rule_policy.key_ipv4_src = sqlite3_column_int(statement, 10);
	old_tc_filter_params->flower_rule_policy.key_ipv4_dst = sqlite3_column_int(statement, 11);
	old_tc_filter_params->flower_rule_policy.key_l4_src = sqlite3_column_int(statement, 12);
	old_tc_filter_params->flower_rule_policy.key_l4_dst = sqlite3_column_int(statement, 13);
	old_tc_filter_params->actions.num_of_actions = sqlite3_column_int(statement, 3);

	/* get filter mask */
	temp_mac_addr = sqlite3_column_int64(statement, 14);
	memcpy(&old_tc_filter_params->flower_rule_policy.mask_eth_dst, &temp_mac_addr, ETH_ALEN);
	temp_mac_addr = sqlite3_column_int64(statement, 15);
	memcpy(&old_tc_filter_params->flower_rule_policy.mask_eth_src, &temp_mac_addr, ETH_ALEN);
	old_tc_filter_params->flower_rule_policy.mask_eth_type = sqlite3_column_int(statement, 16);
	old_tc_filter_params->flower_rule_policy.mask_ip_proto = sqlite3_column_int(statement, 17);
	old_tc_filter_params->flower_rule_policy.mask_ipv4_src = sqlite3_column_int(statement, 18);
	old_tc_filter_params->flower_rule_policy.mask_ipv4_dst = sqlite3_column_int(statement, 19);
	old_tc_filter_params->flower_rule_policy.mask_l4_src = sqlite3_column_int(statement, 20);
	old_tc_filter_params->flower_rule_policy.mask_l4_dst = sqlite3_column_int(statement, 21);
	old_tc_filter_params->flower_rule_policy.mask_bitmap.raw_data = sqlite3_column_int(statement, 22);

	sqlite3_finalize(statement);

	old_tc_filter_params->ifindex = tc_filter_params->ifindex;
	old_tc_filter_params->priority = tc_filter_params->priority;
	old_tc_filter_params->handle = tc_filter_params->handle;

	/* compare fields */
	if (memcmp(old_tc_filter_params->flower_rule_policy.key_eth_dst, tc_filter_params->flower_rule_policy.key_eth_dst, ETH_ALEN) != 0) {
		*is_key_changed = true;
	}
	if (memcmp(old_tc_filter_params->flower_rule_policy.key_eth_src, tc_filter_params->flower_rule_policy.key_eth_src, ETH_ALEN) != 0) {
		*is_key_changed = true;
	}
	if (old_tc_filter_params->flower_rule_policy.key_eth_type != tc_filter_params->flower_rule_policy.key_eth_type) {
		*is_key_changed = true;
	}
	if (old_tc_filter_params->flower_rule_policy.key_ip_proto != tc_filter_params->flower_rule_policy.key_ip_proto) {
		*is_key_changed = true;
	}
	if (old_tc_filter_params->flower_rule_policy.key_ipv4_src != tc_filter_params->flower_rule_policy.key_ipv4_src) {
		*is_key_changed = true;
	}
	if (old_tc_filter_params->flower_rule_policy.key_ipv4_dst != tc_filter_params->flower_rule_policy.key_ipv4_dst) {
		*is_key_changed = true;
	}
	if (old_tc_filter_params->flower_rule_policy.key_l4_src != tc_filter_params->flower_rule_policy.key_l4_src) {
		*is_key_changed = true;
	}
	if (old_tc_filter_params->flower_rule_policy.key_l4_dst != tc_filter_params->flower_rule_policy.key_l4_dst) {
		*is_key_changed = true;
	}
#if 0
	print_filter(old_tc_filter_params);
#endif

	return TC_API_OK;
}

/******************************************************************************
 * \brief	tc_delete_all_flower_filters_on_interface -
 *		Delete all filters with given interface. (all handles will be deleted)
 *
 * \param[in]   interface    - interface port of these filters
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc tc_delete_all_flower_filters_on_interface(uint32_t interface)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[TC_DB_SQL_COMMAND_SIZE];

	write_log(LOG_INFO, "Deleting all filters with interface = %d", interface);

	/* get all filters with this interface and later we will delete them */
	snprintf(sql,
		 TC_DB_SQL_COMMAND_SIZE,
		 "SELECT * FROM filters_table WHERE "
		 "interface = %d ORDER BY priority DESC, handle ASC;",
		 interface);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		return TC_API_DB_ERROR;
	}
	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	while (rc == SQLITE_ROW) {
		struct tc_filter tc_filter_params;

		tc_filter_params.handle = sqlite3_column_int(statement, 2);
		tc_filter_params.priority = sqlite3_column_int(statement, 1);
		tc_filter_params.ifindex = interface;

		TC_CHECK_ERROR(tc_int_delete_flower_filter(&tc_filter_params));

		write_log(LOG_DEBUG, "Filter was Deleted (interface = %d, priority 0x%x, handle 0x%x)",
			  tc_filter_params.ifindex, tc_filter_params.priority, tc_filter_params.handle);

		rc = sqlite3_step(statement);
	}

	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	/* finalize SQL statement */
	sqlite3_finalize(statement);
	return TC_API_OK;
}

/******************************************************************************
 * \brief	tc_delete_all_priority_flower_filters -
 *		Delete all filters with given priority & interface. (all handles will be deleted)
 *
 * \param[in]   interface    - interface port of these filters
 *              priority     - priority of these filters
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc tc_delete_all_priority_flower_filters(struct tc_filter *tc_filter_params)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[TC_DB_SQL_COMMAND_SIZE];

	write_log(LOG_INFO, "Deleting all filters with interface = %d and priority 0x%x",
		  tc_filter_params->ifindex, tc_filter_params->priority);

	/* get all filters with this priority and interface and later we will delete them */
	snprintf(sql,
		 TC_DB_SQL_COMMAND_SIZE,
		 "SELECT * FROM filters_table WHERE "
		 /* filter key */
		 "interface = %d AND "
		 "priority = %d ORDER BY handle ASC;",
		 tc_filter_params->ifindex,
		 tc_filter_params->priority);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		return TC_API_DB_ERROR;
	}
	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	while (rc == SQLITE_ROW) {
		struct tc_filter tc_filter_params_tmp;

		tc_filter_params_tmp.ifindex = tc_filter_params->ifindex;
		tc_filter_params_tmp.priority = tc_filter_params->priority;
		tc_filter_params_tmp.handle = sqlite3_column_int(statement, 2);

		TC_CHECK_ERROR(tc_int_delete_flower_filter(&tc_filter_params_tmp));

		write_log(LOG_DEBUG, "Filter was Deleted (interface = %d, priority 0x%x, handle 0x%x)",
			  tc_filter_params_tmp.ifindex, tc_filter_params_tmp.priority, tc_filter_params_tmp.handle);

		rc = sqlite3_step(statement);
	}

	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	/* finalize SQL statement */
	sqlite3_finalize(statement);
	return TC_API_OK;
}

/******************************************************************************
 * \brief	get all actions of the flower filter
 *
 * \param[in]   tc_filter_params - reference to the filter configuration
 * \param[out]  action_info - reference to array of elements of action info
 * \param[out]  num_of_actions - reference to the number if actions that we found (uint32)
 * \param[out]  filter_actions_index - reference to the filter actions index of the filter
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_flower_filter_actions(struct tc_filter *tc_filter_params,
					 struct action_id *actions_array,
					 uint32_t *num_of_actions,
					 uint32_t *filter_actions_index)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[TC_DB_SQL_COMMAND_SIZE];
	uint32_t action_array_bytes;

	/* set action index and priority value to each of the rule items on list */
	/* check if exist on sql filter table another entry with the same mask, table but different priority */
	snprintf(sql,
		 TC_DB_SQL_COMMAND_SIZE,
		 "SELECT * FROM filters_table WHERE "
		 /* filter key */
		 "interface = %d AND "
		 "priority = %d AND "
		 "handle = %d;",
		 tc_filter_params->ifindex,
		 tc_filter_params->priority,
		 tc_filter_params->handle);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		return TC_API_DB_ERROR;
	}
	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	/* filter entry not found */
	if (rc == SQLITE_DONE) {
		write_log(LOG_NOTICE, "Filter wasn't found on internal DB");
		sqlite3_finalize(statement);
		return TC_API_FAILURE;
	}

	memset(actions_array, 0, sizeof(struct action_id)*MAX_NUM_OF_ACTIONS_IN_FILTER);

	/* Retrieve action info */
	action_array_bytes = sqlite3_column_bytes(statement, 4);

	if (action_array_bytes > sizeof(struct action_id)*MAX_NUM_OF_ACTIONS_IN_FILTER) {
		write_log(LOG_ERR, "reading action info failed, number of bytes is too big");
		return TC_API_DB_ERROR;
	}

	memcpy(actions_array, sqlite3_column_blob(statement, 4), action_array_bytes);

	*num_of_actions = sqlite3_column_int(statement, 3);
	*filter_actions_index = sqlite3_column_int(statement, 23);

	/* finalize SQL statement */
	sqlite3_finalize(statement);
	return TC_API_OK;
}

/******************************************************************************
 * \brief	get the number of the flower filters that we found on DB (query by interface & priority)
 *
 * \param[in]   interface - interface index (linux index)
 * \param[out]  priority - priority value, if equal to 0 get all filters from this interface (all priorities)
 * \param[out]  num_of_filters - reference to the number if actions that we found (uint32)
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_num_of_flower_filters(uint32_t interface, uint32_t priority, uint32_t *num_of_filters)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[TC_DB_SQL_COMMAND_SIZE];

	/* check the number of filters with this interface */
	if (priority == 0) {
		snprintf(sql,
			 TC_DB_SQL_COMMAND_SIZE,
			 "SELECT COUNT (*) AS filters_count FROM filters_table WHERE "
			 "interface=%d;",
			 interface);
	} else {
		snprintf(sql,
			 TC_DB_SQL_COMMAND_SIZE,
			 "SELECT COUNT (*) AS filters_count FROM filters_table WHERE "
			 "interface=%d AND priority=%d;",
			 interface, priority);
	}

	/* Prepare SQL statement */
	write_log(LOG_DEBUG, "SQL command: %s", sql);
	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		return TC_API_DB_ERROR;
	}
	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	/* mask not found */
	if (rc == SQLITE_DONE) {
		*num_of_filters = 0;
	} else {
		*num_of_filters = sqlite3_column_int(statement, 0);
	}

	write_log(LOG_INFO, "number of filters returned is = %d", *num_of_filters);

	sqlite3_finalize(statement);

	return TC_API_OK;
}

/******************************************************************************
 * \brief	get array of the flower filters that we found on DB (query by interface & priority)
 *
 * \param[in]   interface - interface index (linux index)
 * \param[out]  priority - priority value, if equal to 0 get all filters from this interface (all priorities)
 * \param[out]  filters_array - reference of the array of the filters that we found
 * \param[in]   num_of_filters - the number if actions that we can store on the array (filters_array)
 *
 * \return	enum tc_api_rc:
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_flower_filters_id_from_db(uint32_t interface,
					     uint32_t priority,
					     struct tc_filter_id *filters_array,
					     uint32_t num_of_filters)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[TC_DB_SQL_COMMAND_SIZE];
	uint32_t i;

	/* check the number of filters with this interface */
	if (priority == 0) {
		snprintf(sql,
			 TC_DB_SQL_COMMAND_SIZE,
			 "SELECT * FROM filters_table WHERE "
			 "interface=%d;",
			 interface);
	} else {
		snprintf(sql,
			 TC_DB_SQL_COMMAND_SIZE,
			 "SELECT * FROM filters_table WHERE "
			 "interface=%d AND priority=%d;",
			 interface, priority);
	}


	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		return TC_API_DB_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	i = 0;
	while (rc == SQLITE_ROW) {

		if (i >= num_of_filters) {
			write_log(LOG_ERR, "wrong number of filters was allocated");
			return TC_API_FAILURE;
		}

		filters_array[i].ifindex = sqlite3_column_int(statement, 0);
		filters_array[i].priority = sqlite3_column_int(statement, 1);
		filters_array[i].handle = sqlite3_column_int(statement, 2);

		write_log(LOG_DEBUG, "add filter (interface %d, priority %d, handle %d) to array of filters",
			  filters_array[i].ifindex,
			  filters_array[i].priority,
			  filters_array[i].handle);

		rc = sqlite3_step(statement);

		i++;
	}

	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	/* finalize SQL statement */
	sqlite3_finalize(statement);

	return TC_API_OK;
}

/******************************************************************************
 * \brief	get the flower filter info from DB
 *
 * \param[in/out] tc_filter_params - reference to the filter configuration
 * \param[out]    is_filter_exists - reference to boolean, true if filter exist
 *
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_flower_filter_from_db(struct tc_filter *tc_filter_params, bool *is_filter_exists)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[TC_DB_SQL_COMMAND_SIZE];
	uint32_t actions_array_bytes, i;
	struct action_id actions_array[MAX_NUM_OF_ACTIONS_IN_FILTER];
	uint64_t temp_mac_addr;

	/* set filter index and priority value to each of the rule items on list */
	/* check if exist on sql filter table another entry with the same mask, table but different priority */
	snprintf(sql,
		 TC_DB_SQL_COMMAND_SIZE,
		 "SELECT * FROM filters_table WHERE "
		 /* filter key */
		 "interface = %d AND "
		 "priority = %d AND "
		 "handle = %d;",
		 tc_filter_params->ifindex,
		 tc_filter_params->priority,
		 tc_filter_params->handle);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		return TC_API_DB_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(tc_db));
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	/* filter entry not found */
	if (rc == SQLITE_DONE) {
		*is_filter_exists = false;
		write_log(LOG_NOTICE, "Filter wasn't found on internal DB");
		sqlite3_finalize(statement);
		return TC_API_OK;
	}

	*is_filter_exists = true;
	memset(actions_array, 0, sizeof(struct action_id)*MAX_NUM_OF_ACTIONS_IN_FILTER);

	/* Retrieve action info */
	actions_array_bytes = sqlite3_column_bytes(statement, 4);

	if (actions_array_bytes > sizeof(uint32_t)*MAX_NUM_OF_ACTIONS_IN_FILTER) {
		write_log(LOG_ERR, "reading actions array failed, number of bytes is too big");
		return TC_API_DB_ERROR;
	}

	memcpy(actions_array, sqlite3_column_blob(statement, 4), actions_array_bytes);

	tc_filter_params->type = TC_FILTER_FLOWER;
	tc_filter_params->actions.num_of_actions = sqlite3_column_int(statement, 3);

	/* get all actions of this filter */
	for (i = 0; i < tc_filter_params->actions.num_of_actions; i++) {
		tc_filter_params->actions.action[i].general.family_type = actions_array[i].action_family_type;
		tc_filter_params->actions.action[i].general.index = actions_array[i].linux_action_index;
	}

	/* get filter key */
	temp_mac_addr = sqlite3_column_int64(statement, 6);
	memcpy(&tc_filter_params->flower_rule_policy.key_eth_dst, &temp_mac_addr, ETH_ALEN);
	temp_mac_addr = sqlite3_column_int64(statement, 7);
	memcpy(&tc_filter_params->flower_rule_policy.key_eth_src, &temp_mac_addr, ETH_ALEN);
	tc_filter_params->flower_rule_policy.key_eth_type = sqlite3_column_int(statement, 8);
	tc_filter_params->flower_rule_policy.key_ip_proto = sqlite3_column_int(statement, 9);
	tc_filter_params->flower_rule_policy.key_ipv4_src = sqlite3_column_int(statement, 10);
	tc_filter_params->flower_rule_policy.key_ipv4_dst = sqlite3_column_int(statement, 11);
	tc_filter_params->flower_rule_policy.key_l4_src = sqlite3_column_int(statement, 12);
	tc_filter_params->flower_rule_policy.key_l4_dst = sqlite3_column_int(statement, 13);
	tc_filter_params->actions.num_of_actions = sqlite3_column_int(statement, 3);

	/* get filter mask */
	temp_mac_addr = sqlite3_column_int64(statement, 14);
	memcpy(&tc_filter_params->flower_rule_policy.mask_eth_dst, &temp_mac_addr, ETH_ALEN);
	temp_mac_addr = sqlite3_column_int64(statement, 15);
	memcpy(&tc_filter_params->flower_rule_policy.mask_eth_src, &temp_mac_addr, ETH_ALEN);
	tc_filter_params->flower_rule_policy.mask_eth_type = sqlite3_column_int(statement, 16);
	tc_filter_params->flower_rule_policy.mask_ip_proto = sqlite3_column_int(statement, 17);
	tc_filter_params->flower_rule_policy.mask_ipv4_src = sqlite3_column_int(statement, 18);
	tc_filter_params->flower_rule_policy.mask_ipv4_dst = sqlite3_column_int(statement, 19);
	tc_filter_params->flower_rule_policy.mask_l4_src = sqlite3_column_int(statement, 20);
	tc_filter_params->flower_rule_policy.mask_l4_dst = sqlite3_column_int(statement, 21);
	tc_filter_params->flower_rule_policy.mask_bitmap.raw_data = sqlite3_column_int(statement, 22);
	tc_filter_params->protocol = sqlite3_column_int(statement, 24);

	/* finalize SQL statement */
	sqlite3_finalize(statement);
	return TC_API_OK;
}

/****************************************************************************************************/
/**********************************     Masks Table     *********************************************/
/****************************************************************************************************/

/******************************************************************************
 * \brief	check if this mask is already exist on mask table on DB
 *
 * \param[in]   tc_filter_params - reference to the filter configuration
 * \param[out]  is_mask_exist - reference to boolean, true if mask exist
 *
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc check_if_mask_already_exist(struct tc_filter *tc_filter_params, bool *is_mask_exist)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[TC_DB_SQL_COMMAND_SIZE];
	uint64_t eth_src_mask_uint64 = 0;
	uint64_t eth_dst_mask_uint64 = 0;
	struct tc_flower_rule_policy *flower_rule_policy;
	uint32_t mask_count;

	flower_rule_policy = &tc_filter_params->flower_rule_policy;

	memcpy(&eth_dst_mask_uint64, flower_rule_policy->mask_eth_dst, ETH_ALEN);
	memcpy(&eth_src_mask_uint64, flower_rule_policy->mask_eth_src, ETH_ALEN);

	/* check if exist on sql filter table another entry with the same mask, table but different priority */
	snprintf(sql,
		 TC_DB_SQL_COMMAND_SIZE,
		 "SELECT COUNT (mask_index) AS mask_count FROM masks_table WHERE "
		 "interface=%d AND "
		 "eth_type_mask=%d AND "
		 "eth_dst_mask=%ld AND "
		 "eth_src_mask=%ld AND "
		 "ip_proto_mask=%d AND "
		 "ipv4_src_mask=%d AND "
		 "ipv4_dst_mask=%d AND "
		 "l4_src_mask=%d AND "
		 "l4_dst_mask=%d;",
		 tc_filter_params->ifindex,
		 flower_rule_policy->mask_eth_type,
		 eth_dst_mask_uint64,
		 eth_src_mask_uint64,
		 flower_rule_policy->mask_ip_proto,
		 flower_rule_policy->mask_ipv4_src,
		 flower_rule_policy->mask_ipv4_dst,
		 flower_rule_policy->mask_l4_src,
		 flower_rule_policy->mask_l4_dst);

	/* Prepare SQL statement */
	write_log(LOG_DEBUG, "SQL command: %s", sql);
	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		return TC_API_DB_ERROR;
	}
	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	/* mask not found */
	if (rc == SQLITE_DONE) {
		write_log(LOG_DEBUG, "Not Found mask on mask_table");
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	mask_count = sqlite3_column_int(statement, 0);

	write_log(LOG_INFO, "mask_count = %d", mask_count);
	if (mask_count == 0) {
		*is_mask_exist = false;
		write_log(LOG_DEBUG, "this mask is not exist on masks table");
	} else {
		*is_mask_exist = true;
		write_log(LOG_DEBUG, "this mask is already exist on masks table");
	}

	sqlite3_finalize(statement);

	return TC_API_OK;
}

/******************************************************************************
 * \brief	get the next index in available on the mask table
 *		mask table entries should be sequence and without empty entries
 *
 * \param[in]   tc_filter_params - reference to the filter configuration
 * \param[out]  new_mask_index - reference to the new mask index that we found
 *
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_new_mask_index(struct tc_filter *tc_filter_params, uint32_t *new_mask_index)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[TC_DB_SQL_COMMAND_SIZE];

	/* check how many masks on db */
	snprintf(sql, TC_DB_SQL_COMMAND_SIZE,
		 "SELECT COUNT (mask_index) AS mask_count FROM masks_table WHERE interface=%d;",
		 tc_filter_params->ifindex);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "LAST SQL COMMAND: %s", sql);
		return TC_API_DB_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* In case of error return 0 */
	if (rc != SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(tc_db));
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	if (rc == SQLITE_DONE) {
		write_log(LOG_DEBUG, "Entries not exist on mask table, set the first mask index to 0");
		*new_mask_index = 0;
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	/* retrieve count from result,
	 * finalize SQL statement and return count
	 */
	*new_mask_index = sqlite3_column_int(statement, 0);

	sqlite3_finalize(statement);

	return TC_API_OK;
}

/******************************************************************************
 * \brief	add mask to mask table
 *
 * \param[in]   tc_filter_params - reference to the filter configuration
 * \param[in]   new_mask_index - new mask index of the entry that we want to write
 * \param[in]   tc_mask_info - the mask info that we want to save
 *
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc add_mask_to_mask_table_db(struct tc_filter *tc_filter_params,
					 uint32_t new_mask_index,
					 struct tc_mask_info tc_mask_info)
{
	int rc;
	char *zErrMsg = NULL;
	char sql[TC_DB_SQL_COMMAND_SIZE];
	uint64_t eth_src_mask_uint64 = 0;
	uint64_t eth_dst_mask_uint64 = 0;
	struct tc_flower_rule_policy *flower_rule_policy;
	uint32_t temp_bitmap_mask = 0;

	flower_rule_policy = &tc_filter_params->flower_rule_policy;

	/* add mask to db */
	memcpy(&eth_dst_mask_uint64, flower_rule_policy->mask_eth_dst, ETH_ALEN);
	memcpy(&eth_src_mask_uint64, flower_rule_policy->mask_eth_src, ETH_ALEN);
	memcpy(&temp_bitmap_mask, &tc_mask_info, sizeof(tc_mask_info));

	/* check if exist on sql filter table another entry with the same mask, table but different priority */
	snprintf(sql,
		 TC_DB_SQL_COMMAND_SIZE,
		 "INSERT INTO masks_table "
		 "(interface, eth_type_mask, mask_index, eth_dst_mask, eth_src_mask, ip_proto_mask, "
		 "ipv4_src_mask, ipv4_dst_mask, l4_src_mask, l4_dst_mask, result_bitmap_mask) "
		 "VALUES (%d, %d, %d, %ld, %ld, %d, %d, %d, %d, %d, %d);",
		 tc_filter_params->ifindex,
		 flower_rule_policy->mask_eth_type,
		 new_mask_index,
		 eth_dst_mask_uint64,
		 eth_src_mask_uint64,
		 flower_rule_policy->mask_ip_proto,
		 flower_rule_policy->mask_ipv4_src,
		 flower_rule_policy->mask_ipv4_dst,
		 flower_rule_policy->mask_l4_src,
		 flower_rule_policy->mask_l4_dst,
		 temp_bitmap_mask);

	/* Execute SQL statement */
	write_log(LOG_DEBUG, "SQL command: %s", sql);
	rc = sqlite3_exec(tc_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		write_log(LOG_CRIT, "Last SQL command: %s", sql);
		sqlite3_free(zErrMsg);
		return TC_API_DB_ERROR;
	}

	return TC_API_OK;
}

/******************************************************************************
 * \brief	get mask index of a specific mask entry (used on delete mask)
 *
 * \param[in]   tc_filter_params - reference to the filter configuration
 * \param[out]  mask_index_to_delete - reference to the mask index that we found
 *
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_mask_index_to_delete(struct tc_filter *tc_filter_params, uint32_t *mask_index_to_delete)
{
	enum tc_api_rc rc;
	char sql[TC_DB_SQL_COMMAND_SIZE];
	uint64_t eth_src_mask_uint64 = 0;
	uint64_t eth_dst_mask_uint64 = 0;
	struct tc_flower_rule_policy *flower_rule_policy;
	sqlite3_stmt *statement;

	flower_rule_policy = &tc_filter_params->flower_rule_policy;

	memcpy(&eth_dst_mask_uint64, flower_rule_policy->mask_eth_dst, ETH_ALEN);
	memcpy(&eth_src_mask_uint64, flower_rule_policy->mask_eth_src, ETH_ALEN);

	snprintf(sql,
		 TC_DB_SQL_COMMAND_SIZE,
		 "SELECT mask_index FROM masks_table WHERE "
		 "interface=%d AND "
		 "eth_type_mask=%d AND "
		 "eth_dst_mask=%ld AND "
		 "eth_src_mask=%ld AND "
		 "ip_proto_mask=%d AND "
		 "ipv4_src_mask=%d AND  "
		 "ipv4_dst_mask=%d AND "
		 "l4_src_mask=%d AND "
		 "l4_dst_mask=%d; ",
		 tc_filter_params->ifindex,
		 flower_rule_policy->mask_eth_type,
		 eth_dst_mask_uint64,
		 eth_src_mask_uint64,
		 flower_rule_policy->mask_ip_proto,
		 flower_rule_policy->mask_ipv4_src,
		 flower_rule_policy->mask_ipv4_dst,
		 flower_rule_policy->mask_l4_src,
		 flower_rule_policy->mask_l4_dst);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(tc_db));
		return TC_API_DB_ERROR;
	}
	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "Last SQL Command: %s", sql);
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	/* mask not found */
	if (rc == SQLITE_DONE) {
		write_log(LOG_ERR, "Mask Not Found with the same key");
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	/* get the mask index to delete */
	*mask_index_to_delete = sqlite3_column_int(statement, 0);

	return TC_API_OK;
}

/******************************************************************************
 * \brief	Delete mask entry from DB
 *
 * \param[in]   tc_filter_params - reference to the filter configuration
 * \param[out]  mask_index_to_delete - mask entry to delete
 *
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc delete_mask_entry_from_db(struct tc_filter *tc_filter_params, uint32_t mask_index_to_delete)
{
	enum tc_api_rc rc;
	char *zErrMsg = NULL;
	char sql[TC_DB_SQL_COMMAND_SIZE];

	snprintf(sql, TC_DB_SQL_COMMAND_SIZE,
		 "DELETE FROM masks_table "
		"WHERE interface=%d AND "
		"mask_index=%d;",
		tc_filter_params->ifindex,
		mask_index_to_delete);

	/* Execute SQL statement */
	rc = sqlite3_exec(tc_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return TC_API_DB_ERROR;
	}

	return TC_API_OK;
}

/******************************************************************************
 * \brief	repalce deleted entry on mask table with the last entry in mask table
 *		on delete we replace the last entry with the deleted entry (used only when we want to delete not the last entry)
 *
 * \param[in]   tc_filter_params - reference to the filter configuration
 * \param[in]   last_mask_index - mask entry of the last entry
 * \param[in]   mask_index_to_delete - mask entry to delete
 *
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc replace_deleted_entry_with_last_mask_entry(struct tc_filter *tc_filter_params,
							  uint32_t last_mask_index,
							  uint32_t mask_index_to_delete)
{
	enum tc_api_rc rc;
	char *zErrMsg = NULL;
	char sql[TC_DB_SQL_COMMAND_SIZE];

	snprintf(sql, TC_DB_SQL_COMMAND_SIZE,
		 "UPDATE masks_table "
		"SET mask_index=%d "
		"WHERE interface=%d AND mask_index=%d ",
		mask_index_to_delete,
		tc_filter_params->ifindex,
		last_mask_index);

	/* Execute SQL statement */
	rc = sqlite3_exec(tc_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return TC_API_DB_ERROR;
	}

	return TC_API_OK;
}

/******************************************************************************
 * \brief	get the mask info from mask table
 *
 * \param[in]   tc_filter_params - reference to the filter configuration
 * \param[in]   mask_index - mask index that we want to find
 * \param[out]  result_mask_info - reference of the mask info that we found on DB
 *
 *		TC_API_OK - function succeed
 *		TC_API_FAILURE - function failed due to wrong input params (not exist on DB)
 *		TC_API_DB_ERROR - function failed due to critical error
 */
enum tc_api_rc get_result_mask_bitmap(struct tc_filter *tc_filter_params,
				      uint32_t mask_index,
				      struct tc_mask_info *result_mask_info)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[TC_DB_SQL_COMMAND_SIZE];
	uint32_t temp_mask_bitmap;

	/* check how many masks on db */
	snprintf(sql, TC_DB_SQL_COMMAND_SIZE,
		 "SELECT result_bitmap_mask FROM masks_table WHERE interface=%d AND mask_index=%d;",
		 tc_filter_params->ifindex,
		 mask_index);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(tc_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "LAST SQL COMMAND: %s", sql);
		return TC_API_DB_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* In case of error return 0 */
	if (rc != SQLITE_ROW) {
		write_log(LOG_CRIT, "return value: %d, SQL error: %s", rc, sqlite3_errmsg(tc_db));
		write_log(LOG_CRIT, "last SQL command: %s", sql);
		sqlite3_finalize(statement);
		return TC_API_DB_ERROR;
	}

	/* retrieve count from result,
	 * finalize SQL statement and return count
	 */
	temp_mask_bitmap = sqlite3_column_int(statement, 0);
	memcpy(result_mask_info, &temp_mask_bitmap, sizeof(union tc_mask_bitmap));

	sqlite3_finalize(statement);


	return TC_API_OK;
}


