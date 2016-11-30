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
*  File:                nw_db.c
*  Desc:                Implementation of network database init/destroy code.
*
*/

#include "log.h"
#include "infrastructure.h"
#include "sqlite3.h"
#include "nw_search_defs.h"
#include "cfg.h"

#define NW_DB_FILE_NAME "nw.db"

/* Global pointer to the DB */
sqlite3 *nw_db;
extern uint32_t fib_entry_count;

/**************************************************************************//**
 * \brief       Create internal DB
 *
 * \param[in]   none
 *
 * \return      false - Failed to create internal DB
 *              true  - Created successfully
 */
bool nw_db_init(void)
{
	int rc;
	char *sql;
	char *zErrMsg = NULL;

	/* Delete existing DB file */
	(void)remove(NW_DB_FILE_NAME);

	/* Open the DB file */
	rc = sqlite3_open(NW_DB_FILE_NAME, &nw_db);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "Can't open database: %s",
			  sqlite3_errmsg(nw_db));
		return false;
	}

	write_log(LOG_DEBUG, "NW_DB: Opened database successfully");

	/* Create the fib_entries table:
	 *
	 * Fields:
	 * dest_ip
	 * mask_length
	 * result_type
	 * next_hop
	 * nps_index
	 * is_lag
	 * output_index
	 *
	 * Key:
	 * dest_ip
	 * mask_length
	 */
	sql = "CREATE TABLE fib_entries("
		"dest_ip INT NOT NULL,"                 /* 0 */
		"mask_length INT NOT NULL,"             /* 1 */
		"result_type INT NOT NULL,"             /* 2 */
		"next_hop INT NOT NULL,"                /* 3 */
		"nps_index INT NOT NULL,"               /* 4 */
		"is_lag INT NOT NULL,"                  /* 5 */
		"output_index INT NOT NULL,"            /* 6 */
		"PRIMARY KEY (dest_ip,mask_length));";

	/* Execute SQL statement */
	rc = sqlite3_exec(nw_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return false;
	}

	fib_entry_count = 0;

	/* Create the interface table:
	 *
	 * Fields:
	 * interface_id
	 * admin_state
	 * lag_group_id
	 * is_lag
	 * my_mac
	 *
	 * Key:
	 * interface_id
	 */

	sql = "CREATE TABLE nw_interfaces("
		"interface_id INT NOT NULL,"		/* 0 */
		"admin_state INT NOT NULL,"		/* 1 */
		"lag_group_id INT NOT NULL,"		/* 2 */
		"is_lag INT NOT NULL,"			/* 3 */
		"mac_address BIGINT NOT NULL,"		/* 4 */
		"path_type INT NOT NULL,"		/* 5 */
		"is_direct_output_lag INT NOT NULL,"	/* 6 */
		"direct_output_if INT NOT NULL,"	/* 7 */
		"app_bitmap INT NOT NULL,"		/* 8 */
		"output_channel INT NOT NULL,"		/* 9 */
		"sft_en INT NOT NULL,"			/* 10 */
		"stats_base INT NOT NULL,"		/* 11 */
		"PRIMARY KEY (interface_id));";

	/* Execute SQL statement */
	rc = sqlite3_exec(nw_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return false;
	}


	/* Create the arp_entries table:
	 *
	 * Fields:
	 * entry_ip
	 * is_lag
	 * output_index
	 * dest_mac_address
	 *
	 * Key:
	 * entry_ip
	 * is_lag
	 * output_index
	 */

	sql = "CREATE TABLE arp_entries("
		"entry_ip INT NOT NULL,"		/* 0 */
		"is_lag INT NOT NULL,"			/* 1 */
		"output_index INT NOT NULL,"		/* 2 */
		"dest_mac_address BIGINT NOT NULL,"	/* 3 */
		"PRIMARY KEY (entry_ip,is_lag,output_index));";

	/* Execute SQL statement */
	rc = sqlite3_exec(nw_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return false;
	}

	if (system_cfg_is_lag_en() == true) {
		/* Create the lag_group table:
		 *
		 * Fields:
		 * lag_group_id
		 * admin_state
		 * mac_address
		 *
		 * Key:
		 * lag_group_id
		 */
		sql = "CREATE TABLE lag_groups("
			"lag_group_id INT NOT NULL,"		/* 0 */
			"admin_state INT NOT NULL,"		/* 1 */
			"mac_address BIGINT NOT NULL,"		/* 2 */
			"PRIMARY KEY (lag_group_id));";

		/* Execute SQL statement */
		rc = sqlite3_exec(nw_db, sql, NULL, NULL, &zErrMsg);
		if (rc != SQLITE_OK) {
			write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
			sqlite3_free(zErrMsg);
			return false;
		}
	}

	return true;
}

/**************************************************************************//**
 * \brief       Destroy internal DB
 *
 * \param[in]   none
 *
 * \return      none
 */
void nw_db_destroy(void)
{
	/* Close the DB file */
	sqlite3_close(nw_db);

}

