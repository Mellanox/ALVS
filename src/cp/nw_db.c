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

#include <stdio.h>
#include <string.h>
#include "log.h"
#include "infrastructure.h"
#include "sqlite3.h"
#include "nw_search_defs.h"
#include "cfg.h"
#include "nw_db.h"

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
bool nw_db_create(void)
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

	/* Create the interface table: */
	sql = "CREATE TABLE nw_interfaces("
		"interface_id INT NOT NULL,"		/* 0 */
		"admin_state INT NOT NULL,"		/* 1 */
		"lag_group_id INT NOT NULL,"		/* 2 */
		"is_lag INT NOT NULL,"			/* 3 */
		"logical_id INT NOT NULL,"		/* 4 */
		"path_type INT NOT NULL,"		/* 5 */
		"is_direct_output_lag INT NOT NULL,"	/* 6 */
		"direct_output_if INT NOT NULL,"	/* 7 */
		"app_bitmap INT NOT NULL,"		/* 8 */
		"output_channel INT NOT NULL,"		/* 9 */
		"sft_en INT NOT NULL,"			/* 10 */
		"stats_base INT NOT NULL,"		/* 11 */
		"mac_address BIGINT NOT NULL,"		/* 12 */
		"PRIMARY KEY (interface_id, logical_id));";

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

enum nw_db_rc internal_db_add_entry(enum internal_db_table_name table_name, void *entry_data)
{
	int rc;
	char sql[512];
	char *zErrMsg = NULL;
	struct nw_db_lag_group_entry *lag_group = NULL;
	struct nw_db_fib_entry *fib_entry = NULL;
	struct nw_db_arp_entry *arp_entry = NULL;
	struct nw_db_nw_interface *nw_interface = NULL;
	uint16_t app_bitmap_u16_casting = 0;
	uint64_t mac_address_casting = 0;

	switch (table_name) {
	case FIB_ENTRIES_INTERNAL_DB:
		fib_entry = (struct nw_db_fib_entry *)entry_data;
		sprintf(sql, "INSERT INTO fib_entries "
			"(dest_ip, mask_length, result_type, next_hop, nps_index, is_lag, output_index) "
			"VALUES (%d, %d, %d, %d, %d, %d, %d);",
			fib_entry->dest_ip, fib_entry->mask_length, fib_entry->result_type, fib_entry->next_hop, fib_entry->nps_index, fib_entry->is_lag, fib_entry->output_index);
		break;

	case ARP_ENTRIES_INTERNAL_DB:
		arp_entry = (struct nw_db_arp_entry *)entry_data;
		memcpy(&mac_address_casting, arp_entry->dest_mac_address.ether_addr_octet, ETH_ALEN);
		sprintf(sql, "INSERT INTO arp_entries "
			"(entry_ip, is_lag, output_index, dest_mac_address) "
			"VALUES (%d, %d, %d, %ld);",
			arp_entry->entry_ip, arp_entry->is_lag,
			arp_entry->output_index,
			mac_address_casting);
		break;

	case LAG_GROUPS_INTERNAL_DB:
		lag_group = (struct nw_db_lag_group_entry *)entry_data;
		memcpy(&mac_address_casting, lag_group->mac_address.ether_addr_octet, ETH_ALEN);
		sprintf(sql, "INSERT INTO lag_groups "
			"(lag_group_id, admin_state, mac_address) "
			"VALUES (%d, %d, %ld);",
			lag_group->lag_group_id, (uint32_t)lag_group->admin_state,
			mac_address_casting);
		break;

	case NW_INTERFACES_INTERNAL_DB:
		nw_interface = (struct nw_db_nw_interface *)entry_data;
		memcpy(&app_bitmap_u16_casting, &nw_interface->app_bitmap, sizeof(uint16_t));
		memcpy(&mac_address_casting, nw_interface->mac_address.ether_addr_octet, ETH_ALEN);
		sprintf(sql, "INSERT INTO nw_interfaces "
			"(interface_id, admin_state, lag_group_id, is_lag, mac_address, path_type, "
			"is_direct_output_lag, direct_output_if, app_bitmap, output_channel, sft_en, stats_base, logical_id) "
			"VALUES (%d, %d, %d, %d, %ld, %d, %d, %d, %d, %d, %d, %d, %d );",
			nw_interface->interface_id,
			nw_interface->admin_state,
			nw_interface->lag_group_id, nw_interface->is_lag,
			mac_address_casting,
			nw_interface->path_type,
			nw_interface->is_direct_output_lag,
			nw_interface->direct_output_if,
			app_bitmap_u16_casting,
			nw_interface->output_channel,
			nw_interface->sft_en,
			nw_interface->stats_base,
			nw_interface->logical_id);

		break;
	default:
		write_log(LOG_NOTICE, "Trying to add an entry to a bad internal db table name");
		return NW_DB_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_exec(nw_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		write_log(LOG_CRIT, "Last SQL command: %s", sql);
		sqlite3_free(zErrMsg);
		return NW_DB_ERROR;
	}

	return NW_DB_OK;
}

enum nw_db_rc internal_db_modify_entry(enum internal_db_table_name table_name, void *entry_data)
{
	int rc;
	char sql[512];
	char *zErrMsg = NULL;

	struct nw_db_lag_group_entry *lag_group = NULL;
	struct nw_db_nw_interface *nw_interface = NULL;
	struct nw_db_fib_entry *fib_entry = NULL;
	struct nw_db_arp_entry *arp_entry = NULL;
	uint16_t app_bitmap_u16_casting = 0;
	uint64_t mac_address_casting = 0;

	switch (table_name) {
	case FIB_ENTRIES_INTERNAL_DB:

		fib_entry = (struct nw_db_fib_entry *)entry_data;
		sprintf(sql, "UPDATE fib_entries "
			"SET result_type=%d, next_hop=%d, nps_index=%d, is_lag=%d, output_index=%d "
			"WHERE dest_ip=%d AND mask_length=%d;",
			fib_entry->result_type, fib_entry->next_hop, fib_entry->nps_index, fib_entry->is_lag, fib_entry->output_index, fib_entry->dest_ip, fib_entry->mask_length);
		break;

	case ARP_ENTRIES_INTERNAL_DB:
		arp_entry = (struct nw_db_arp_entry *)entry_data;
		memcpy(&mac_address_casting, arp_entry->dest_mac_address.ether_addr_octet, ETH_ALEN);
		sprintf(sql, "UPDATE arp_entries "
			"SET dest_mac_address=%ld "
			"WHERE entry_ip=%d AND is_lag=%d AND output_index=%d;",
			mac_address_casting,
			arp_entry->entry_ip,
			arp_entry->is_lag,
			arp_entry->output_index);
		break;

	case LAG_GROUPS_INTERNAL_DB:
		lag_group = (struct nw_db_lag_group_entry *)entry_data;
		memcpy(&mac_address_casting, lag_group->mac_address.ether_addr_octet, ETH_ALEN);
		sprintf(sql, "UPDATE lag_groups "
			"SET admin_state=%d, mac_address=%ld "
			"WHERE lag_group_id=%d;",
			lag_group->admin_state,
			mac_address_casting,
			lag_group->lag_group_id);

		break;

	case NW_INTERFACES_INTERNAL_DB:
		nw_interface = (struct nw_db_nw_interface *)entry_data;
		memcpy(&app_bitmap_u16_casting, &nw_interface->app_bitmap, sizeof(uint16_t));
		memcpy(&mac_address_casting, nw_interface->mac_address.ether_addr_octet, ETH_ALEN);
		sprintf(sql, "UPDATE nw_interfaces "
			"SET admin_state=%d, lag_group_id=%d, is_lag=%d, "
			"mac_address=%ld, path_type=%d, is_direct_output_lag=%d,"
			"direct_output_if=%d, app_bitmap=%d, output_channel=%d, sft_en=%d, stats_base=%d "
			"WHERE interface_id=%d AND logical_id=%d ;",
			nw_interface->admin_state,
			nw_interface->lag_group_id,
			nw_interface->is_lag,
			mac_address_casting,
			nw_interface->path_type,
			nw_interface->is_direct_output_lag,
			nw_interface->direct_output_if,
			app_bitmap_u16_casting,
			nw_interface->output_channel,
			nw_interface->sft_en,
			nw_interface->stats_base,
			nw_interface->interface_id,
			nw_interface->logical_id);
		break;
	default:
		write_log(LOG_NOTICE, "Trying to modify an entry from a wrong internal db table name");
		return NW_DB_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_exec(nw_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		write_log(LOG_CRIT, "Last SQL command: %s", sql);
		sqlite3_free(zErrMsg);
		return NW_DB_ERROR;
	}

	return NW_DB_OK;
}

enum nw_db_rc internal_db_remove_entry(enum internal_db_table_name table_name, void *entry_data)
{
	int rc;
	char sql[512];
	char *zErrMsg = NULL;
	struct nw_db_lag_group_entry *lag_group = NULL;
	struct nw_db_fib_entry *fib_entry = NULL;
	struct nw_db_arp_entry *arp_entry = NULL;
	struct nw_db_nw_interface *nw_interface = NULL;

	switch (table_name) {
	case FIB_ENTRIES_INTERNAL_DB:
		fib_entry = (struct nw_db_fib_entry *)entry_data;
		sprintf(sql, "DELETE FROM fib_entries "
			"WHERE dest_ip=%d AND mask_length=%d;",
			fib_entry->dest_ip, fib_entry->mask_length);
		break;

	case ARP_ENTRIES_INTERNAL_DB:
		arp_entry = (struct nw_db_arp_entry *)entry_data;
		sprintf(sql, "DELETE FROM arp_entries "
			"WHERE entry_ip=%d AND is_lag=%d AND output_index=%d;",
			arp_entry->entry_ip, arp_entry->is_lag, arp_entry->output_index);
		break;

	case LAG_GROUPS_INTERNAL_DB:
		lag_group = (struct nw_db_lag_group_entry *)entry_data;
		sprintf(sql, "DELETE FROM lag_groups "
			"WHERE lag_group_id=%d;",
			lag_group->lag_group_id);
		break;

	case NW_INTERFACES_INTERNAL_DB:
		nw_interface = (struct nw_db_nw_interface *)entry_data;
		sprintf(sql, "DELETE FROM nw_interfaces "
			"WHERE interface_id=%d AND logical_id=%d;",
			nw_interface->interface_id, nw_interface->logical_id);
		break;
	default:
		write_log(LOG_NOTICE, "Trying to remove an entry from a bad internal db table name");
		return NW_DB_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_exec(nw_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		write_log(LOG_CRIT, "Last SQL command: %s", sql);
		sqlite3_free(zErrMsg);
		return NW_DB_ERROR;
	}

	return NW_DB_OK;
}

enum nw_db_rc internal_db_get_entry(enum internal_db_table_name table_name, void *entry_data)
{
	int rc;
	char sql[512];
	sqlite3_stmt *statement;
	struct nw_db_lag_group_entry *lag_group = NULL;
	struct nw_db_fib_entry *fib_entry = NULL;
	struct nw_db_arp_entry *arp_entry = NULL;
	struct nw_db_nw_interface *nw_interface = NULL;
	uint16_t app_bitmap_u16_casting = 0;
	uint64_t mac_address_casting = 0;

	switch (table_name) {
	case FIB_ENTRIES_INTERNAL_DB:
		fib_entry = (struct nw_db_fib_entry *)entry_data;
		sprintf(sql, "SELECT * FROM fib_entries "
			"WHERE dest_ip=%d AND mask_length=%d;",
			fib_entry->dest_ip, fib_entry->mask_length);
		break;

	case ARP_ENTRIES_INTERNAL_DB:
		arp_entry = (struct nw_db_arp_entry *)entry_data;
		sprintf(sql, "SELECT * FROM arp_entries "
			"WHERE entry_ip=%d AND is_lag=%d AND output_index=%d;",
			arp_entry->entry_ip, arp_entry->is_lag, arp_entry->output_index);
		break;

	case LAG_GROUPS_INTERNAL_DB:
		lag_group = (struct nw_db_lag_group_entry *)entry_data;
		sprintf(sql, "SELECT * FROM lag_groups "
			"WHERE lag_group_id=%d;",
			lag_group->lag_group_id);
		break;

	case NW_INTERFACES_INTERNAL_DB:
		nw_interface = (struct nw_db_nw_interface *)entry_data;
		sprintf(sql, "SELECT * FROM nw_interfaces "
			"WHERE interface_id=%d;",
			nw_interface->interface_id);
		break;

	default:
		write_log(LOG_NOTICE, "Trying to search an entry from a wrong internal db table name");
		return NW_DB_ERROR;
	}

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(nw_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(nw_db));
		return NW_DB_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(nw_db));
		sqlite3_finalize(statement);
		return NW_DB_ERROR;
	}

	/* Entry not found */
	if (rc == SQLITE_DONE) {
		sqlite3_finalize(statement);
		return NW_DB_FAILURE;
	}

	switch (table_name) {
	case FIB_ENTRIES_INTERNAL_DB:
		/* retrieve fib entry from result */
		fib_entry->result_type = (enum nw_fib_type)sqlite3_column_int(statement, 2);
		fib_entry->next_hop = sqlite3_column_int(statement, 3);
		fib_entry->nps_index = sqlite3_column_int(statement, 4);
		fib_entry->is_lag = sqlite3_column_int(statement, 5);
		fib_entry->output_index = sqlite3_column_int(statement, 6);
		break;

	case ARP_ENTRIES_INTERNAL_DB:
		/* retrieve arp entry from result */
		mac_address_casting = sqlite3_column_int64(statement, 3);
		memcpy(arp_entry->dest_mac_address.ether_addr_octet, &mac_address_casting, ETH_ALEN);
		break;

	case LAG_GROUPS_INTERNAL_DB:
		/* retrieve LAG group entry from result */
		lag_group->admin_state = sqlite3_column_int(statement, 1);
		mac_address_casting = sqlite3_column_int64(statement, 2);
		memcpy(lag_group->mac_address.ether_addr_octet, &mac_address_casting, ETH_ALEN);
		break;

	case NW_INTERFACES_INTERNAL_DB:
		/* retrieve NW IF entry from result */
		nw_interface->admin_state = sqlite3_column_int(statement, 1);
		nw_interface->lag_group_id = sqlite3_column_int(statement, 2);
		nw_interface->is_lag = sqlite3_column_int(statement, 3);
		nw_interface->logical_id = sqlite3_column_int(statement, 4);
		nw_interface->path_type = sqlite3_column_int(statement, 5);
		nw_interface->is_direct_output_lag = sqlite3_column_int(statement, 6);
		nw_interface->direct_output_if = sqlite3_column_int(statement, 7);
		app_bitmap_u16_casting = sqlite3_column_int(statement, 8);
		memcpy(&nw_interface->app_bitmap, &app_bitmap_u16_casting, sizeof(uint16_t));
		nw_interface->output_channel = sqlite3_column_int(statement, 9);
		nw_interface->sft_en = sqlite3_column_int(statement, 10);
		nw_interface->stats_base = sqlite3_column_int(statement, 11);
		mac_address_casting = sqlite3_column_int64(statement, 12);
		memcpy(nw_interface->mac_address.ether_addr_octet, &mac_address_casting, ETH_ALEN);
		break;
	}
	/* finalize SQL statement and return */
	sqlite3_finalize(statement);
	return NW_DB_OK;
}

