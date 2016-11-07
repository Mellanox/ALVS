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
*  File:                nw_api.c
*  Desc:                Implementation of network database common API.
*
*/

#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <byteswap.h>
#include <arpa/inet.h>
#include <linux/rtnetlink.h>
#include <linux/ethtool.h>

#include <netlink/route/neighbour.h> /* todo */
#include <arpa/inet.h>

#include "log.h"
#include "nw_api.h"
#include "sqlite3.h"
#include "nw_conf.h"
#include "infrastructure_utils.h"
#include "nw_search_defs.h"
#include "cfg.h"

/* Global pointer to the DB */
sqlite3 *nw_db;
uint32_t fib_entry_count;

extern const char *nw_if_posted_stats_offsets_names[];
extern const char *remote_if_posted_stats_offsets_names[];

#define NW_DB_FILE_NAME "nw.db"

extern char *addr_to_str(struct nl_addr *addr);

/**************************************************************************//**
 * \brief       internal inet_ntoa - IP to string
 *
 * \param[in]   ip - IP to convert
 *
 * \return      String of IP
 */
char *nw_inet_ntoa(in_addr_t ip)
{
	struct in_addr ip_addr = {.s_addr = ip};

	return inet_ntoa(ip_addr);
}

/**************************************************************************//**
 * \brief       Create internal DB
 *
 * \param[in]   none
 *
 * \return      NW_API_DB_ERROR - Failed to create internal DB
 *              NW_API_OK             - Created successfully
 */
enum nw_api_rc nw_api_init_db(void)
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
		return NW_API_DB_ERROR;
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
	 *
	 * Key:
	 * dest_ip
	 * mask_length
	 */
	sql = "CREATE TABLE fib_entries("
		"dest_ip INT NOT NULL,"
		"mask_length INT NOT NULL,"
		"result_type INT NOT NULL,"
		"next_hop INT NOT NULL,"
		"nps_index INT NOT NULL,"
		"PRIMARY KEY (dest_ip,mask_length));";

	/* Execute SQL statement */
	rc = sqlite3_exec(nw_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return NW_API_DB_ERROR;
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
		return NW_API_DB_ERROR;
	}


	/* Create the arp_entries table:
	 *
	 * Fields:
	 * entry_ip
	 * is_lag
	 * lag_group_id
	 *
	 * Key:
	 * entry_ip
	 */

	sql = "CREATE TABLE arp_entries("
		"entry_ip INT NOT NULL,"		/* 0 */
		"is_lag INT NOT NULL,"			/* 1 */
		"output_index INT NOT NULL,"		/* 2 */
		"dest_mac_address BIGINT NOT NULL,"	/* 3 */
		"PRIMARY KEY (entry_ip));";

	/* Execute SQL statement */
	rc = sqlite3_exec(nw_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return NW_API_DB_ERROR;
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
			return NW_API_DB_ERROR;
		}
	}

	return NW_API_OK;

}

/**************************************************************************//**
 * \brief       Destroy internal DB
 *
 * \param[in]   none
 *
 * \return      none
 */
void nw_api_destroy_db(void)
{
	/* Close the DB file */
	sqlite3_close(nw_db);

}

/**************************************************************************//**
 * \brief       Add an entry to internal DB
 *
 * \param[in]   entry_data   - reference to entry data
 *                             (nw_db_lag_group_entry or nw_db_fib_entry or nw_db_nw_interface)
 *
 * \return      NW_API_OK - Add to internal DB succeed
 *              NW_API_DB_ERROR - failed to communicate with DB
 *
 */
enum nw_api_rc internal_db_add_entry(enum internal_db_table_name table_name, void *entry_data)
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
			"(dest_ip, mask_length, result_type, next_hop, nps_index) "
			"VALUES (%d, %d, %d, %d, %d);",
			fib_entry->dest_ip, fib_entry->mask_length, fib_entry->result_type, fib_entry->next_hop, fib_entry->nps_index);
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
			"is_direct_output_lag, direct_output_if, app_bitmap, output_channel, sft_en, stats_base) "
			"VALUES (%d, %d, %d, %d, %ld, %d, %d, %d, %d, %d, %d, %d );",
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
			nw_interface->stats_base);

		break;
	default:
		return NW_API_FAILURE;
	}

	/* Execute SQL statement */
	rc = sqlite3_exec(nw_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		write_log(LOG_CRIT, "Last SQL command: %s", sql);
		sqlite3_free(zErrMsg);
		return NW_API_DB_ERROR;
	}

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       Modify an entry in internal DB
 *
 * \param[in]   fib_entry   - reference to fib entry
 *
 * \return      NW_API_OK - Modify of internal DB succeed
 *              NW_API_DB_ERROR - failed to communicate with DB
 *              NW_API_FAILURE - wrong table name
 */
enum nw_api_rc internal_db_modify_entry(enum internal_db_table_name table_name, void *entry_data)
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
			"SET result_type=%d, next_hop=%d, nps_index=%d "
			"WHERE dest_ip=%d AND mask_length=%d;",
			fib_entry->result_type, fib_entry->next_hop, fib_entry->nps_index, fib_entry->dest_ip, fib_entry->mask_length);
		break;

	case ARP_ENTRIES_INTERNAL_DB:
		arp_entry = (struct nw_db_arp_entry *)entry_data;
		memcpy(&mac_address_casting, arp_entry->dest_mac_address.ether_addr_octet, ETH_ALEN);
		sprintf(sql, "UPDATE arp_entries "
			"SET is_lag=%d, output_index=%d, dest_mac_address=%ld "
			"WHERE entry_ip=%d;",
			arp_entry->is_lag,
			arp_entry->output_index,
			mac_address_casting,
			arp_entry->entry_ip);
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
			"WHERE interface_id=%d;",
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
			nw_interface->interface_id);
		break;

	default:
		write_log(LOG_NOTICE, "Trying to modify an entry from a wrong internal db table name");
		return NW_API_FAILURE;
	}

	/* Execute SQL statement */
	rc = sqlite3_exec(nw_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		write_log(LOG_CRIT, "Last SQL command: %s", sql);
		sqlite3_free(zErrMsg);
		return NW_API_DB_ERROR;
	}

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       Remove an_entry from internal DB
 *
 * \param[in]   fib_entry   - reference to fib entry
 *

 * \return      NW_API_OK - remove from internal db succeed
 *              NW_API_DB_ERROR - failed to communicate with DB
 *
 *              NW_API_FAILURE - wrong table name
 */
enum nw_api_rc internal_db_remove_entry(enum internal_db_table_name table_name, void *entry_data)
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
			"WHERE entry_ip=%d;",
			arp_entry->entry_ip);
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
			"WHERE interface_id=%d;",
			nw_interface->interface_id);
		break;

	default:
		write_log(LOG_NOTICE, "Trying to remove an entry from a wrong internal db table name");
		return NW_API_FAILURE;
	}

	/* Execute SQL statement */
	rc = sqlite3_exec(nw_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		write_log(LOG_CRIT, "Last SQL command: %s", sql);
		sqlite3_free(zErrMsg);
		return NW_API_DB_ERROR;
	}

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       Searches an entry in internal DB
 *
 * \param[in]   entry   - reference to entry
 *
 * \return      NW_API_OK - entry found
 *              NW_API_FAILURE - entry not found
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc internal_db_get_entry(enum internal_db_table_name table_name, void *entry_data)
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
			"WHERE entry_ip=%d;",
			arp_entry->entry_ip);
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
		return NW_API_FAILURE;
	}

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(nw_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(nw_db));
		return NW_API_DB_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(nw_db));
		sqlite3_finalize(statement);
		return NW_API_DB_ERROR;
	}

	/* FIB entry not found */
	if (rc == SQLITE_DONE) {
		sqlite3_finalize(statement);
		return NW_API_FAILURE;
	}

	switch (table_name) {
	case FIB_ENTRIES_INTERNAL_DB:
		/* retrieve fib entry from result,
		 * finalize SQL statement and return
		 */
		fib_entry->result_type = (enum nw_fib_type)sqlite3_column_int(statement, 2);
		fib_entry->next_hop = sqlite3_column_int(statement, 3);
		fib_entry->nps_index = sqlite3_column_int(statement, 4);
		break;

	case ARP_ENTRIES_INTERNAL_DB:
		/* retrieve arp entry from result,
		 * finalize SQL statement and return
		 */
		arp_entry->is_lag = (bool)sqlite3_column_int(statement, 1);
		arp_entry->output_index = sqlite3_column_int(statement, 2);
		mac_address_casting = sqlite3_column_int64(statement, 3);
		memcpy(arp_entry->dest_mac_address.ether_addr_octet, &mac_address_casting, ETH_ALEN);
		break;

	case LAG_GROUPS_INTERNAL_DB:
		/* retrieve fib entry from result,
		 * finalize SQL statement and return
		 */
		lag_group->admin_state = sqlite3_column_int(statement, 1);
		mac_address_casting = sqlite3_column_int64(statement, 2);
		memcpy(lag_group->mac_address.ether_addr_octet, &mac_address_casting, ETH_ALEN);
		break;

	case NW_INTERFACES_INTERNAL_DB:
		/* retrieve fib entry from result,
		 * finalize SQL statement and return
		 */
		nw_interface->admin_state = sqlite3_column_int(statement, 1);
		nw_interface->lag_group_id = sqlite3_column_int(statement, 2);
		nw_interface->is_lag = sqlite3_column_int(statement, 3);
		mac_address_casting = sqlite3_column_int64(statement, 4);
		memcpy(nw_interface->mac_address.ether_addr_octet, &mac_address_casting, ETH_ALEN);
		nw_interface->path_type = sqlite3_column_int(statement, 5);
		nw_interface->is_direct_output_lag = sqlite3_column_int(statement, 6);
		nw_interface->direct_output_if = sqlite3_column_int(statement, 7);
		app_bitmap_u16_casting = sqlite3_column_int(statement, 8);
		memcpy(&nw_interface->app_bitmap, &app_bitmap_u16_casting, sizeof(uint16_t));
		nw_interface->output_channel = sqlite3_column_int(statement, 9);
		nw_interface->sft_en = sqlite3_column_int(statement, 10);
		nw_interface->stats_base = sqlite3_column_int(statement, 11);
		break;
	}

	sqlite3_finalize(statement);

	return NW_API_OK;
}
/******************************************************************************
 * \brief    translate Linux neighbor entry to ARP table key & result
 *
 * \return   void
 */
void neighbor_to_arp_entry(struct rtnl_neigh *neighbor, struct nw_arp_key *key, struct nw_arp_result *result)
{
	if (key) {
		key->real_server_address = *(uint32_t *)nl_addr_get_binary_addr(rtnl_neigh_get_dst(neighbor));
	}
	if (result) {
		memcpy(result->dest_mac_addr.ether_addr_octet, nl_addr_get_binary_addr(rtnl_neigh_get_lladdr(neighbor)), 6);

		result->output_index.output_interface = NW_BASE_LOGICAL_ID; /* todo will be changed in the future, to take the real port id */
	}
}

/**************************************************************************//**
 * \brief       count the number of arp entries on internal db
 *
 * \param[out]  arp_entries_count - will get the num of arp_entries on internal db
 *
 * \return      NW_API_OK - function succeed
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc internal_db_get_arp_entries_count(unsigned int *arp_entries_count)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[256];

	sprintf(sql, "SELECT COUNT (entry_ip) AS arp_entries_count FROM arp_entries;");

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(nw_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(nw_db));
		return NW_API_DB_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* In case of error return 0 */
	if (rc != SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(nw_db));
		sqlite3_finalize(statement);
		return NW_API_DB_ERROR;
	}

	/* retrieve count from result,
	 * finalize SQL statement and return count
	 */
	*arp_entries_count = sqlite3_column_int(statement, 0);
	sqlite3_finalize(statement);
	return NW_API_OK;
}

/******************************************************************************
 * \brief    Receives a neighbor and adds it as an entry to ARP table
 *           If an error is received from CP, exits the application.
 *
 * \return   NW_API_OK - entry found
 *           NW_API_FAILURE - arp entry already exist
 *           NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc nw_db_add_arp_entry(struct rtnl_neigh *neighbor)
{
	struct nw_arp_result result;
	struct nw_arp_key key;
	struct nw_db_arp_entry arp_entry;
	unsigned int arp_entries_count = 0;

	write_log(LOG_DEBUG, "Add neighbor to table    IP = %s MAC = %s", addr_to_str(rtnl_neigh_get_dst(neighbor)), addr_to_str(rtnl_neigh_get_lladdr(neighbor)));
	neighbor_to_arp_entry(neighbor, &key, &result);

	/* check if arp entry already exist on internal db */
	arp_entry.entry_ip = key.real_server_address;

	switch (internal_db_get_entry(ARP_ENTRIES_INTERNAL_DB, (void *)&arp_entry)) {
	case NW_API_OK:
		/* FIB entry already exists */
		write_log(LOG_NOTICE, "Can't add ARP entry. Entry (IP=%s) already exists.",
			  nw_inet_ntoa(arp_entry.entry_ip));
		return NW_API_FAILURE;
	case NW_API_DB_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Error on internal DB");
		return NW_API_DB_ERROR;
	case NW_API_FAILURE:
		/* ARP entry doesn't exist on NW DB */
		break;
	default:
		return NW_API_DB_ERROR;
	}

	/* check if reached to max num of arp entries */
	if (internal_db_get_arp_entries_count(&arp_entries_count) != NW_API_OK) {
		write_log(LOG_ERR, "Error while counting arp entries on internal db");
		return NW_API_FAILURE;
	}

	/* check if reached to max num of entries */
	if (arp_entries_count >= NW_ARP_TABLE_MAX_ENTRIES) {
		write_log(LOG_ERR, "Error, reached to the maximum number of arp entries");
		return NW_API_FAILURE;
	}

	/* set lag output index on lag mode */
	if (system_cfg_is_lag_en() == true) {
		struct nw_db_lag_group_entry lag_group;

		lag_group.lag_group_id = 0; /* todo change? */
		if (internal_db_get_entry(LAG_GROUPS_INTERNAL_DB, (void *)&lag_group) != NW_API_OK) {
			write_log(LOG_NOTICE, "Error on internal db or lag group is not exist");
			return NW_API_DB_ERROR;
		}
		result.is_lag = true;
		result.output_index.lag_index = 0; /* default group, todo change? */
	} else {
		result.is_lag = false;
	}

	/* add arp entry to internal db */
	arp_entry.entry_ip = key.real_server_address;
	arp_entry.is_lag = result.is_lag;
	arp_entry.output_index = result.output_index.lag_index;
	memcpy(arp_entry.dest_mac_address.ether_addr_octet, result.dest_mac_addr.ether_addr_octet, ETH_ALEN);
	if (internal_db_add_entry(ARP_ENTRIES_INTERNAL_DB, (void *)&arp_entry) != NW_API_OK) {
		write_log(LOG_NOTICE, "Error on internal db while adding arp entry");
		return NW_API_DB_ERROR;
	}

	/* add entry to DP table */
	if (!infra_add_entry(STRUCT_ID_NW_ARP, &key, sizeof(key), &result, sizeof(result))) {
		write_log(LOG_ERR, "Cannot add entry to ARP table key= 0x%X08 result= %02x:%02x:%02x:%02x:%02x:%02x", key.real_server_address,  result.dest_mac_addr.ether_addr_octet[0], result.dest_mac_addr.ether_addr_octet[1], result.dest_mac_addr.ether_addr_octet[2], result.dest_mac_addr.ether_addr_octet[3], result.dest_mac_addr.ether_addr_octet[4], result.dest_mac_addr.ether_addr_octet[5]);
		return NW_API_DB_ERROR;
	}

	return NW_API_OK;
}

/******************************************************************************
 * \brief    Receives a neighbor and removes it from internal db and NPS ARP table
 *           If an error is received from CP, exits the application.
 *
 * \return   NW_API_OK - entry found
 *           NW_API_FAILURE - arp entry not exist
 *           NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc nw_db_remove_arp_entry(struct rtnl_neigh *neighbor)
{
	struct nw_arp_key key;
	struct nw_db_arp_entry arp_entry;

	write_log(LOG_DEBUG, "Delete neighbor from table IP = %s MAC = %s", addr_to_str(rtnl_neigh_get_dst(neighbor)), addr_to_str(rtnl_neigh_get_lladdr(neighbor)));
	neighbor_to_arp_entry(neighbor, &key, NULL);


	/* check if arp entry exist on internal db */
	arp_entry.entry_ip = key.real_server_address;
	switch (internal_db_get_entry(ARP_ENTRIES_INTERNAL_DB, (void *)&arp_entry)) {
	case NW_API_OK:
		/* FIB entry exists */
		write_log(LOG_DEBUG, "ARP entry (IP=%s) found in internal DB",
			  nw_inet_ntoa(arp_entry.entry_ip));
		break;
	case NW_API_DB_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find ARP entry (internal error).");
		return NW_API_DB_ERROR;
	case NW_API_FAILURE:
		/* ARP entry doesn't exist in NW DB, no need to delete */
		return NW_API_FAILURE;
	default:
		return NW_API_DB_ERROR;
	}

	/* remove arp entry from internal db */
	arp_entry.entry_ip = key.real_server_address;
	if (internal_db_remove_entry(ARP_ENTRIES_INTERNAL_DB, (void *)&arp_entry) != NW_API_OK) {
		write_log(LOG_NOTICE, "Error on internal db while deleting arp entry");
		return NW_API_DB_ERROR;
	}

	/* remove entry from DP table */
	if (!infra_delete_entry(STRUCT_ID_NW_ARP, &key, sizeof(key))) {
		write_log(LOG_ERR, "Cannot remove entry from ARP table key= 0x%X08", key.real_server_address);
		return NW_API_DB_ERROR;
	}

	return NW_API_OK;
}

/******************************************************************************
 * \brief    Receives a neighbor and modifies it on internal db and NPS ARP table
 *           If an error is received from CP, exits the application.
 *
 * \return      NW_API_OK - function succeed
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc nw_db_modify_arp_entry(struct rtnl_neigh *neighbor)
{
	struct nw_arp_result result;
	struct nw_arp_key key;
	struct nw_db_arp_entry arp_entry;

	write_log(LOG_DEBUG, "Modify neighbor on table IP = %s MAC = %s", addr_to_str(rtnl_neigh_get_dst(neighbor)), addr_to_str(rtnl_neigh_get_lladdr(neighbor)));
	neighbor_to_arp_entry(neighbor, &key, &result);

	/* check if arp entry exist on internal db */
	arp_entry.entry_ip = key.real_server_address;
	switch (internal_db_get_entry(ARP_ENTRIES_INTERNAL_DB, (void *)&arp_entry)) {
	case NW_API_OK:
		/* FIB entry exists */
		write_log(LOG_DEBUG, "ARP entry (IP=%s) found on internal DB",
			  nw_inet_ntoa(arp_entry.entry_ip));
		break;
	case NW_API_DB_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find ARP entry (internal error).");
		return NW_API_DB_ERROR;
	case NW_API_FAILURE:
		/* ARP entry doesn't exist in NW DB, create a new one */
		write_log(LOG_DEBUG, "ARP Entry is not exist on CP, create a new entry (IP=%s)", nw_inet_ntoa(arp_entry.entry_ip));
		return nw_db_add_arp_entry(neighbor);

	default:
		return NW_API_DB_ERROR;
	}

	/* todo - common function (build result) with the add arp entry */
	/* set lag output index on lag mode */
	if (system_cfg_is_lag_en() == true) {
		struct nw_db_lag_group_entry lag_group;

		lag_group.lag_group_id = 0; /* todo change? */
		if (internal_db_get_entry(LAG_GROUPS_INTERNAL_DB, (void *)&lag_group) != NW_API_OK) {
			write_log(LOG_NOTICE, "Error on internal db or associated lag group is not exist");
			return NW_API_DB_ERROR;
		}
		result.is_lag = true;
		result.output_index.lag_index = 0; /* default group, todo change? */
	} else {
		result.is_lag = false;
		result.output_index.lag_index = 0; /* todo need to change it */
	}

	/* modify arp entry on internal db */
	arp_entry.entry_ip = key.real_server_address;
	arp_entry.is_lag = result.is_lag;
	arp_entry.output_index = result.output_index.lag_index;
	memcpy(arp_entry.dest_mac_address.ether_addr_octet, result.dest_mac_addr.ether_addr_octet, ETH_ALEN);
	if (internal_db_modify_entry(ARP_ENTRIES_INTERNAL_DB, (void *)&arp_entry) != NW_API_OK) {
		write_log(LOG_NOTICE, "Error on internal db while modify arp entry");
		return NW_API_DB_ERROR;
	}

	/* modify entry on DP table */
	if (infra_modify_entry(STRUCT_ID_NW_ARP, &key, sizeof(key), &result, sizeof(result)) == false) {
		write_log(LOG_ERR, "Cannot modify entry on ARP table key= 0x%X08", key.real_server_address);
		return NW_API_DB_ERROR;
	}

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       create lag members array
 *
 * \param[in]   lag_group_id  - lag group id
 * \param[in]   interface_id  - the interface id to not include (if using remove)
 * \param[out]  members_count - num of members in group
 * \param[out]  lag_members   - the members array
 * \param[in]   include_disabled_ports  - add the disabled ports of this group also when counting members and creating members array
 *
 * \return      NW_API_OK - function succeed
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc get_lag_group_members_array(int lag_group_id, uint8_t *members_count, uint8_t *lag_members, bool include_disabled_ports)
{
	int rc, member_index;
	sqlite3_stmt *statement;
	char sql[256];

	/* read from db the nw interfaces table and search which interface belong to this lag group */
	sprintf(sql, "SELECT * FROM nw_interfaces "
		"WHERE is_lag=1 AND lag_group_id=%d AND admin_state=1;",
		lag_group_id);

	if (include_disabled_ports == true) {
		sprintf(sql, "SELECT * FROM nw_interfaces "
			"WHERE is_lag=1 AND lag_group_id=%d;",
			lag_group_id);
	}

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(nw_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(nw_db));
		return NW_API_DB_ERROR;
	}
	/* Execute SQL statement */
	rc = sqlite3_step(statement);
	member_index = 0;
	while (rc == SQLITE_ROW) {
		lag_members[member_index] = sqlite3_column_int (statement, 0); /* copy interface id to the members array */
		member_index++;
		rc = sqlite3_step(statement);
	}
	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(nw_db));
		sqlite3_finalize(statement);
		return NW_API_DB_ERROR;
	}

	/* finalize SQL statement */
	sqlite3_finalize(statement);

	*members_count = member_index;

	return NW_API_OK;
}

/******************************************************************************
 * \brief    build the result for the DP table of the nw_interface
 *
 * \param[out]  if_result      - reference to the result struct.
 *  param[in]   interface_data - the values to store in the result struct
 *
 * \return   void
 */
void build_nps_nw_interface_result(struct nw_if_result *if_result, struct nw_db_nw_interface interface_data)
{
	if_result->admin_state = interface_data.admin_state;
	if_result->app_bitmap = (struct nw_if_apps)interface_data.app_bitmap;
	if_result->direct_output_if = interface_data.direct_output_if;
	if_result->is_direct_output_lag = interface_data.is_direct_output_lag;
	memcpy(if_result->mac_address.ether_addr_octet, interface_data.mac_address.ether_addr_octet, ETH_ALEN);
	if_result->output_channel = interface_data.output_channel;
	if_result->path_type = (enum dp_path_type)interface_data.path_type;
	if_result->sft_en = interface_data.sft_en;
	if_result->stats_base = interface_data.stats_base;

}

/**************************************************************************//**
 * \brief       set mac to interface
 *
 * \param[in]   interface_id   - the interface id of the interface
 *              new_my_mac     - the new my mac value
 *
 * \return      NW_API_OK - function succeed
 *              NW_API_FAILURE - interface is not exist
 *              NW_API_DB_ERROR - database error
 *
 */
enum nw_api_rc nw_api_set_nw_interface_mac(unsigned int interface_id, uint8_t *new_my_mac)
{
	struct nw_if_key if_key;
	struct nw_if_result if_result;
	struct nw_db_nw_interface interface_data;
	enum nw_api_rc nw_api_rc;

	/* read interface id from internal db */
	interface_data.interface_id = interface_id;
	nw_api_rc = internal_db_get_entry(NW_INTERFACES_INTERNAL_DB, (void *)&interface_data);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "read nw interface %d from internal table was failed, failure on internal db or interface id is not exist", interface_id);
		return nw_api_rc;
	}

	/* change my mac value */
	memcpy(interface_data.mac_address.ether_addr_octet, new_my_mac, ETH_ALEN);

	/* change interface on internal db */
	nw_api_rc = internal_db_modify_entry(NW_INTERFACES_INTERNAL_DB, (void *)&interface_data);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "modify interface id %d on internal table was failed, failure on internal db or interface id is not exist", interface_id);
		return nw_api_rc;
	}

	/* change my mac on DP table */
	if_key.logical_id = interface_id;
	build_nps_nw_interface_result(&if_result, interface_data);
	if (infra_modify_entry(STRUCT_ID_NW_INTERFACES, &if_key, sizeof(if_key), &if_result, sizeof(if_result)) == false) {
		write_log(LOG_NOTICE, "failed to change my mac value on interface cp/dp table");
		return NW_API_DB_ERROR;
	}

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       set mac to lag group
 *
 * \param[in]   lag_group_id   - the lag group id of the interface
 *              new_my_mac     - the new my mac value
 *
 * \return      NW_API_OK - function succeed
 *              NW_API_FAILURE - group was not exist
 *              NW_API_DB_ERROR - database error
 *
 */
enum nw_api_rc nw_api_set_lag_group_mac(unsigned int lag_group_id, uint8_t *new_my_mac)
{
	struct nw_db_lag_group_entry lag_group;
	uint8_t members_array[LAG_GROUP_MAX_MEMBERS] = {0}, lag_members_count = 0;
	int i;
	enum nw_api_rc nw_api_rc;

	/* read lag group from internal db, check if lag group exists */
	lag_group.lag_group_id = lag_group_id;
	nw_api_rc = internal_db_get_entry(LAG_GROUPS_INTERNAL_DB, (void *)&lag_group);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "read lag group %d from internal table was failed, failure on internal db or lag group is not exist", lag_group_id);
		return nw_api_rc;
	}

	/* change my mac value */
	memcpy(lag_group.mac_address.ether_addr_octet, new_my_mac, ETH_ALEN);

	/* change lag group on internal db */
	nw_api_rc = internal_db_modify_entry(LAG_GROUPS_INTERNAL_DB, (void *)&lag_group);
	if (nw_api_rc != NW_API_OK) {
		/* return to original value */
		write_log(LOG_NOTICE, "modify lag group %d from internal table was failed, failure on internal db or lag group is not exist", lag_group_id);
		return nw_api_rc;
	}

	if (get_lag_group_members_array(lag_group.lag_group_id, &lag_members_count, members_array, false) != NW_API_OK) {
		write_log(LOG_NOTICE, "Error while counting members on lag group %d", lag_group_id);
		return NW_API_DB_ERROR;
	}

	write_log(LOG_DEBUG, "Set mac to all lag_members %d", lag_members_count);
	for (i = 0; i < lag_members_count; i++) {
		write_log(LOG_DEBUG, "Set mac to members_array[%d] = %d", i, members_array[i]);
		nw_api_rc = nw_api_set_nw_interface_mac(members_array[i], new_my_mac);
		if (nw_api_rc != NW_API_OK) {
			write_log(LOG_ERR, "Set mac to nw interace %d failed", members_array[i]);
			return nw_api_rc;
		}
	}

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       get mac from nw interface
 *
 * \param[in]   interface_id - interface id
 * \param[out]  mac - mac address return value
 *
 * \return      NW_API_OK - function succeed
 *              NW_API_FAILURE - interface is not exist
 *              NW_API_DB_ERROR - database error
 */
enum nw_api_rc get_nw_interface_mac(unsigned int interface_id, struct ether_addr *mac)
{
	/* read interface id from internal db */
	struct nw_db_nw_interface interface_data;
	enum nw_api_rc nw_api_rc;

	interface_data.interface_id = interface_id;
	nw_api_rc = internal_db_get_entry(NW_INTERFACES_INTERNAL_DB, (void *)&interface_data);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "read interface %d from internal table was failed, failure on internal db or interface id is not exist", interface_data.interface_id);
		return nw_api_rc;
	}
	memcpy(mac, interface_data.mac_address.ether_addr_octet, ETH_ALEN);

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       get mac from lag group
 *
 * \param[in]   lag_group_id - interface id
 * \param[out]  mac - mac address return value
 *
 * \return      NW_API_OK - function succeed
 *              NW_API_FAILURE - lag group is not exist
 *              NW_API_DB_ERROR - database error
 */
enum nw_api_rc get_lag_group_mac(unsigned int lag_group_id, struct ether_addr *mac)
{
	struct nw_db_lag_group_entry lag_group;
	enum nw_api_rc nw_api_rc;

	/* read lag group from internal db, check if lag group exists */
	lag_group.lag_group_id = lag_group_id;
	nw_api_rc = internal_db_get_entry(LAG_GROUPS_INTERNAL_DB, (void *)&lag_group);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "read lag group %d from internal table was failed, failure on internal db or lag group is not exist", lag_group_id);
		return nw_api_rc;
	}
	memcpy(mac, lag_group.mac_address.ether_addr_octet, ETH_ALEN);

	return NW_API_OK;
}

/******************************************************************************
 * \brief    build the result for entry in the DP table of the nw_interface
 *
 * \param[out]  if_result      - reference to the result struct.
 *  param[in]   interface_data - the reference to interface data to "put" on the result struct
 *
 * \return   true - function succeed
 *           false - function failed
 */
bool build_nps_lag_group_result(struct nw_lag_group_result *lag_result, struct nw_db_lag_group_entry *lag_group)
{
	/* update members_cound and members array */
	if (get_lag_group_members_array(lag_group->lag_group_id, &lag_result->members_count, lag_result->lag_member, false) != NW_API_OK) {
		write_log(LOG_NOTICE, "error on internal db while creating members array");
		return false;
	}
	lag_result->admin_state = lag_group->admin_state;

	return true;
}

/******************************************************************************
 * \brief    add a port to lag group
 *
 * \param[in]   lag_group_id   - lag group to use
 *              interface_id   - interface id to add
 *
 * \return      NW_API_OK - function succeed
 *              NW_API_FAILURE - interface or group is not exist
 *              NW_API_DB_ERROR - database error
 */
enum nw_api_rc nw_api_add_port_to_lag_group(unsigned int lag_group_id, unsigned int interface_id)
{
	struct nw_lag_group_key lag_key;
	struct nw_lag_group_result lag_result;
	struct nw_db_lag_group_entry lag_group;
	struct nw_db_nw_interface interface_data;
	enum nw_api_rc nw_api_rc;

	memset(&lag_key, 0, sizeof(lag_key));
	memset(&lag_result, 0, sizeof(lag_result));

	/* read lag group from internal db */
	lag_group.lag_group_id = lag_group_id;
	nw_api_rc = internal_db_get_entry(LAG_GROUPS_INTERNAL_DB, (void *)&lag_group);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "read lag group %d from internal table was failed, failure on internal db or group id is not exist", lag_group_id);
		return nw_api_rc;
	}

	/* read interface id from internal db */
	interface_data.interface_id = interface_id;
	nw_api_rc = internal_db_get_entry(NW_INTERFACES_INTERNAL_DB, (void *)&interface_data);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "read interface %d from internal table was failed, failure on internal db or interface id is not exist", interface_id);
		return nw_api_rc;
	}

	/* get the num of members on this group (including disabled ports) */
	nw_api_rc = get_lag_group_members_array(lag_group_id, &lag_result.members_count, lag_result.lag_member, true);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "error on internal db while creating members array");
		return nw_api_rc;
	}
	/* check that we didnt reached the max num of members */
	if (lag_result.members_count >= LAG_GROUP_MAX_MEMBERS) {
		write_log(LOG_NOTICE, "Cannot add interface %d to lag group , lag group %d reached to maximum members", interface_id, lag_group_id);
		return NW_API_FAILURE;
	}

	/* set this interface to be a part of this lag group (on internal db) */
	interface_data.interface_id = interface_id;
	interface_data.is_lag = true;
	interface_data.lag_group_id = lag_group_id;
	nw_api_rc = internal_db_modify_entry(NW_INTERFACES_INTERNAL_DB, (void *)&interface_data);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "read interface %d from internal table was failed, failure on internal db or interface id is not exist", interface_id);
		return nw_api_rc;
	}

	/* update key and result on lag group DP table */
	lag_key.lag_group_id = lag_group_id;
	if (build_nps_lag_group_result(&lag_result, &lag_group) == false) {
		write_log(LOG_ERR, "Error while updating result on the DP table");
		return NW_API_DB_ERROR;
	}
	if (infra_modify_entry(STRUCT_ID_NW_LAG_GROUPS, &lag_key, sizeof(lag_key), &lag_result, sizeof(lag_result)) == false) {
		write_log(LOG_CRIT, "Adding lag member to lag group %d failed", lag_group_id);
		return NW_API_DB_ERROR;
	}

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       enable interface
 *
 * \param[in]   interface_id   - the interface id of the interface
 *
 * \return      NW_API_OK - function succeed
 *              NW_API_FAILURE - interface is not exist
 *              NW_API_DB_ERROR - database error
 */
enum nw_api_rc nw_api_enable_nw_interface(unsigned int interface_id)
{
	struct nw_if_key if_key;
	struct nw_if_result if_result;
	struct nw_db_nw_interface interface_data;
	enum nw_api_rc nw_api_rc;

	/* read interface id from internal db, check if interface exists */
	interface_data.interface_id = interface_id;
	nw_api_rc = internal_db_get_entry(NW_INTERFACES_INTERNAL_DB, (void *)&interface_data);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "read interface %d from internal table was failed, failure on internal db or interface id is not exist", interface_id);
		return nw_api_rc;
	}

	/* if admin state is already enable no need to change anything */
	if (interface_data.admin_state == true) {
		write_log(LOG_NOTICE, "interface is already enabled");
		return NW_API_FAILURE;
	}

	/* change admin state value */
	interface_data.admin_state = true;

	/* change on internal db */
	nw_api_rc = internal_db_modify_entry(NW_INTERFACES_INTERNAL_DB, (void *)&interface_data);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "modify interface id %d on internal table was failed, failure on internal db or interface id is not exist", interface_id);
		return nw_api_rc;
	}

	if_key.logical_id = interface_id;
	build_nps_nw_interface_result(&if_result, interface_data);

	if (infra_modify_entry(STRUCT_ID_NW_INTERFACES, &if_key, sizeof(if_key), &if_result, sizeof(if_result)) == false) {
		write_log(LOG_NOTICE, "failed to change my mac value on interface cp/dp table");
		return NW_API_DB_ERROR;
	}

	/* check if this port is a part of lag group --> add this port back to the lag group */
	if (interface_data.is_lag) {
		nw_api_rc = nw_api_add_port_to_lag_group(interface_data.lag_group_id, interface_id);
		if (nw_api_rc != NW_API_OK) {
			write_log(LOG_ERR, "Error while adding port to lag group");
			return nw_api_rc;
		}
	}

	return NW_API_OK;
}

/******************************************************************************
 * \brief    modify admin state to a member on lag group
 *
 * \param[in]   lag_group_id    - lag group lag group to use
 *              interface_id    - interface id to add
 *              new_admin_state - the new admin state ,true-enable, false-disable
 *
 * \return      NW_API_OK - function succeed
 *              NW_API_FAILURE - interface or group is not exist
 *              NW_API_DB_ERROR - database error
 */
enum nw_api_rc nw_api_modify_lag_member_admin_state(unsigned int lag_group_id, unsigned int interface_id, bool new_admin_state)
{
	struct nw_lag_group_key lag_key;
	struct nw_lag_group_result lag_result;

	struct nw_db_lag_group_entry lag_group;
	struct nw_db_nw_interface interface_data;
	enum nw_api_rc nw_api_rc;

	/* read interface id from internal db */
	interface_data.interface_id = interface_id;
	nw_api_rc = internal_db_get_entry(NW_INTERFACES_INTERNAL_DB, (void *)&interface_data);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "read interface %d from internal table was failed, failure on internal db or interface id is not exist", interface_id);
		return nw_api_rc;
	}

	/* if admin_state is already disabled - no need to do anything */
	if (interface_data.admin_state == new_admin_state) {
		write_log(LOG_NOTICE, "interface is already updated");
		return NW_API_FAILURE;
	}

	/* change admin state value */
	interface_data.admin_state = new_admin_state;

	/* change on internal db */
	nw_api_rc = internal_db_modify_entry(NW_INTERFACES_INTERNAL_DB, (void *)&interface_data);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "modify interface id %d on internal table was failed, failure on internal db or interface id is not exist", interface_id);
		return nw_api_rc;
	}

	/* read lag group from internal db */
	lag_group.lag_group_id = lag_group_id;
	nw_api_rc = internal_db_get_entry(LAG_GROUPS_INTERNAL_DB, (void *)&lag_group);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "read lag group %d from internal table was failed, failure on internal db or group id is not exist", lag_group_id);
		return nw_api_rc;
	}

	/* update key and result on lag group DP table */
	lag_key.lag_group_id = lag_group_id;
	if (build_nps_lag_group_result(&lag_result, &lag_group) == false) {
		write_log(LOG_ERR, "Error while updating result on the DP table");
		return NW_API_FAILURE;
	}
	if (infra_modify_entry(STRUCT_ID_NW_LAG_GROUPS, &lag_key, sizeof(lag_key), &lag_result, sizeof(lag_result)) == false) {
		write_log(LOG_CRIT, "Removing lag member from lag group %d failed", lag_group_id);
		return NW_API_DB_ERROR;
	}

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       disable interface
 *
 * \param[in]   interface_id   - the interface id of the interface
 *
 * \return      NW_API_OK - function succeed
 *              NW_API_FAILURE - interface is not exist
 *              NW_API_DB_ERROR - database error
 */
enum nw_api_rc nw_api_disable_nw_interface(unsigned int interface_id)
{
	struct nw_if_key if_key;
	struct nw_if_result if_result;
	struct nw_db_nw_interface interface_data;
	enum nw_api_rc nw_api_rc;

	/* read interface id from internal db */
	interface_data.interface_id = interface_id;
	nw_api_rc = internal_db_get_entry(NW_INTERFACES_INTERNAL_DB, (void *)&interface_data);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "read interface %d from internal table was failed, failure on internal db or interface id is not exist", interface_id);
		return nw_api_rc;
	}

	/* if admin_state is already disabled - no need to do anything */
	if (interface_data.admin_state == false) {
		write_log(LOG_NOTICE, "interface is already disabled");
		return NW_API_FAILURE;
	}

	/* change admin state value */
	interface_data.admin_state = false;

	/* change on internal db */
	nw_api_rc = internal_db_modify_entry(NW_INTERFACES_INTERNAL_DB, (void *)&interface_data);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "modify interface id %d on internal table was failed, failure on internal db or interface id is not exist", interface_id);
		return nw_api_rc;
	}

	/* if it is a lag interface, we need to update the members array */
	if (interface_data.is_lag) {
		nw_api_rc = nw_api_modify_lag_member_admin_state(interface_data.lag_group_id, interface_id, false);
		if (nw_api_rc != NW_API_OK) {
			write_log(LOG_ERR, "Error while disable lag member");
			return nw_api_rc;
		}
	}

	/* change operation state on DP table */
	if_key.logical_id = interface_id;
	build_nps_nw_interface_result(&if_result, interface_data);
	if (infra_modify_entry(STRUCT_ID_NW_INTERFACES, &if_key, sizeof(if_key), &if_result, sizeof(if_result)) == false) {
		write_log(LOG_NOTICE, "failed to change admin state on interface cp/dp table");
		return NW_API_DB_ERROR;
	}

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       count the number of lag groups on internal db
 *
 * \param[out]  lag_groups_count - will get the num of lag groups on internal db
 *
 * \return      NW_API_OK - function succeed
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc internal_db_get_lag_groups_count(unsigned int *lag_groups_count)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[256];

	sprintf(sql, "SELECT COUNT (lag_group_id) AS lag_groups_count FROM lag_groups;");

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(nw_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(nw_db));
		return NW_API_DB_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* In case of error return 0 */
	if (rc != SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(nw_db));
		sqlite3_finalize(statement);
		return NW_API_DB_ERROR;
	}

	/* retrieve count from result,
	 * finalize SQL statement and return count
	 */
	*lag_groups_count = sqlite3_column_int(statement, 0);
	sqlite3_finalize(statement);
	return NW_API_OK;
}

/******************************************************************************
 * \brief    add lag group entry to table,
 *           add to internal db and cp/dp table
 *
 * \param[in]   lag_group_id   - the lag group id
 *              admin_state - the admin state of the new lag group
 *
 * \return      NW_API_OK - function succeed
 *              NW_API_FAILURE - reached maximum or group exist
 *              NW_API_DB_ERROR - database error
 */
enum nw_api_rc nw_api_add_lag_group_entry(unsigned int lag_group_id)
{
	struct nw_lag_group_key lag_key;
	struct nw_lag_group_result lag_result;
	struct nw_db_lag_group_entry lag_group;
	unsigned int num_of_lag_groups;
	enum nw_api_rc nw_api_rc;
	/* check if group id is already exist on system */
	lag_group.lag_group_id = lag_group_id;
	nw_api_rc = internal_db_get_entry(LAG_GROUPS_INTERNAL_DB, (void *)&lag_group);
	if (nw_api_rc == NW_API_OK) {
		write_log(LOG_NOTICE, "cannot add group, group id %d is already exist", lag_group_id);
		return nw_api_rc;
	}

	/* check if reached max num of lag groups on internal db */
	if (internal_db_get_lag_groups_count(&num_of_lag_groups) != NW_API_OK) {
		write_log(LOG_ERR, "error on internal db");
		return NW_API_DB_ERROR;
	}
	if (num_of_lag_groups >= NW_LAG_GROUPS_TABLE_MAX_ENTRIES) {
		write_log(LOG_ERR, "Error, can not add lag group, reached to the max number of lag groups (%d)", NW_LAG_GROUPS_TABLE_MAX_ENTRIES);
		return NW_API_FAILURE;
	}

	/* add the new group to internal db */
	memset(&lag_group, 0, sizeof(struct nw_db_lag_group_entry));
	lag_group.lag_group_id = lag_group_id;
	lag_group.admin_state = true;
	if (internal_db_add_entry(LAG_GROUPS_INTERNAL_DB, (void *)&lag_group) != NW_API_OK) {
		write_log(LOG_CRIT, "Failed to add lag group %d to internal db", lag_group_id);
		return NW_API_DB_ERROR;
	}

	/* add the new lag group to the DP table */
	memset(&lag_result, 0, sizeof(struct nw_lag_group_result));
	memset(&lag_key, 0, sizeof(struct nw_lag_group_key));
	lag_key.lag_group_id = lag_group_id;
	lag_result.admin_state = true;
	if (infra_add_entry(STRUCT_ID_NW_LAG_GROUPS, &lag_key, sizeof(lag_key), &lag_result, sizeof(lag_result)) == false) {
		write_log(LOG_CRIT, "Adding lag group failed, lag group id %d", lag_group_id);
		return NW_API_DB_ERROR;
	}

	return NW_API_OK;
}

#if 0
/**************************************************************************//**
 * \brief       remove all arp entries that leads to a lag group
 *
 * \param[in]   lag_group_id  - lag group id
 *
 * \return      NW_API_OK - function succeed
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc remove_lag_group_from_arp_table(unsigned int lag_group_id)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[256];
	struct nw_db_arp_entry arp_entry;
	struct nw_arp_key key;

	memset(&arp_entry, 0, sizeof(struct nw_db_arp_entry));

	/* read from db the nw interfaces table and search which interface belong to this lag group */
	sprintf(sql, "SELECT * FROM arp_entries "
		"WHERE is_lag=1 AND lag_group_id=%d;",
		lag_group_id);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(nw_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(nw_db));
		return NW_API_DB_ERROR;
	}
	/* Execute SQL statement */
	rc = sqlite3_step(statement);
	while (rc == SQLITE_ROW) {

		/* remove arp entry from internal db */
		arp_entry.entry_ip = sqlite3_column_int (statement, 0); /* copy interface id to the members array */
		if (internal_db_remove_entry(ARP_ENTRIES_INTERNAL_DB, (void *)&arp_entry) != NW_API_OK) {
			write_log(LOG_NOTICE, "Error on internal db while deleting arp entry");
			return NW_API_DB_ERROR;
		}

		/* remove entry from DP table */
		key.real_server_address = arp_entry.entry_ip;
		if (infra_delete_entry(STRUCT_ID_NW_ARP, &key, sizeof(key)) == false) {
			write_log(LOG_ERR, "Cannot remove entry from ARP table key= 0x%X08", key.real_server_address);
			return NW_API_DB_ERROR;
		}

		rc = sqlite3_step(statement);
	}
	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(nw_db));
		sqlite3_finalize(statement);
		return NW_API_DB_ERROR;
	}

	/* finalize SQL statement */
	sqlite3_finalize(statement);

	return NW_API_OK;
}
#endif

/******************************************************************************
 * \brief    remove a port from lag group
 *
 * \param[in]   lag_group_id   - lag group lag group to use
 *              interface_id   - interface id to add
 *
 * \return      NW_API_OK - function succeed
 *              NW_API_FAILURE - group or interface are not exist or port is not a part of this group
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc remove_port_from_lag_group(unsigned int lag_group_id, unsigned int interface_id)
{
	struct nw_lag_group_key lag_key;
	struct nw_lag_group_result lag_result;
	struct nw_db_lag_group_entry lag_group;
	struct nw_db_nw_interface interface_data;
	enum nw_api_rc nw_api_rc;

	/* read lag group from internal db */
	lag_group.lag_group_id = lag_group_id;
	nw_api_rc = internal_db_get_entry(LAG_GROUPS_INTERNAL_DB, (void *)&lag_group);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "read lag group %d from internal table was failed, failure on internal db or group id is not exist", lag_group_id);
		return nw_api_rc;
	}

	/* read interface id from internal db */
	interface_data.interface_id = interface_id;
	nw_api_rc = internal_db_get_entry(NW_INTERFACES_INTERNAL_DB, (void *)&interface_data);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "read interface %d from internal table was failed, failure on internal db or interface id is not exist", interface_id);
		return nw_api_rc;
	}

	/* check that this interface is a part of this lag group */
	if (interface_data.is_lag == false || interface_data.lag_group_id != lag_group_id) {
		write_log(LOG_NOTICE, "ERROR, interface %d is not a part of this lag group", interface_id);
		return NW_API_FAILURE;
	}

	/* remove member from lag group, disable mode we will set is_lag back to true later */
	interface_data.is_lag = false;
	nw_api_rc = internal_db_modify_entry(NW_INTERFACES_INTERNAL_DB, (void *)&interface_data);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "read interface %d from internal table was failed, failure on internal db or interface id is not exist", interface_id);
		return nw_api_rc;
	}

	/* update key and result on lag group DP table */
	lag_key.lag_group_id = lag_group_id;
	if (build_nps_lag_group_result(&lag_result, &lag_group) == false) {
		write_log(LOG_ERR, "Error while updating result on the DP table");
		return NW_API_DB_ERROR;
	}
	if (infra_modify_entry(STRUCT_ID_NW_LAG_GROUPS, &lag_key, sizeof(lag_key), &lag_result, sizeof(lag_result)) == false) {
		write_log(LOG_CRIT, "Removing lag member from lag group %d failed", lag_group_id);
		return NW_API_DB_ERROR;
	}

	return NW_API_OK;
}

#if 0
/******************************************************************************
 * \brief    remove lag group entry,
 *           remove from internal db and dp table
 *
 * \return      NW_API_OK - function succeed
 *              NW_API_FAILURE - group is not exist
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc remove_lag_group_entry(unsigned int lag_group_id)
{
	struct nw_lag_group_key lag_key;
	struct nw_db_lag_group_entry lag_group;
	uint8_t members_array[LAG_GROUP_MAX_MEMBERS] = {0}, members_count = 0;
	int i;
	enum nw_api_rc nw_api_rc;

	memset(&lag_group, 0, sizeof(struct nw_db_lag_group_entry));
	lag_group.lag_group_id = lag_group_id;

	/* check if an entry with this lag group id exist */
	nw_api_rc = internal_db_get_entry(LAG_GROUPS_INTERNAL_DB, (void *)&lag_group);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "get lag group entry was failed, failure on db or lag group id is not exist");
		return nw_api_rc;
	}

	/* remove lag group from arp table */
	write_log(LOG_DEBUG, "removing all arp entries that leads to lag group %d", lag_group_id);
	if (remove_lag_group_from_arp_table(lag_group_id) != NW_API_OK) {
		write_log(LOG_NOTICE, "Error, remove lag group from ARP table failed");
		return NW_API_DB_ERROR;
	}

	/* remove ports from lag group */
	if (get_lag_group_members_array(lag_group_id, &members_count, members_array, true) != NW_API_OK) {
		write_log(LOG_NOTICE, "error on internal db while creating members array");
		return NW_API_DB_ERROR;
	}
	for (i = 0; i < members_count; i++) {
		nw_api_rc = remove_port_from_lag_group(lag_group_id, members_array[i]);
		if (nw_api_rc != NW_API_OK) {
			write_log(LOG_NOTICE, "Error while removing port from lag group");
			return nw_api_rc;
		}
	}

	/* remove lag group from internal db */
	nw_api_rc = internal_db_remove_entry(LAG_GROUPS_INTERNAL_DB, (void *)&lag_group);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_ERR, "remove lag group entry was failed, failure on db or lag group id is not exist");
		return nw_api_rc;
	}

	/* remove lag group entry from DP table */
	memset(&lag_key, 0, sizeof(struct nw_lag_group_key));
	lag_key.lag_group_id = lag_group_id;
	if (infra_delete_entry(STRUCT_ID_NW_LAG_GROUPS, &lag_key, sizeof(lag_key)) == false) {
		write_log(LOG_ERR, "Removing lag group failed, lag group id %d", lag_group_id);
		return NW_API_DB_ERROR;
	}

	/* remove lag group from arp table - remove arp entries twice since arp entries can be created (learned) since our prebious removal */
	write_log(LOG_DEBUG, "removing all arp entries that leads to lag group %d", lag_group_id);
	if (remove_lag_group_from_arp_table(lag_group_id) != NW_API_OK) {
		write_log(LOG_NOTICE, "Error, remove lag group from ARP table failed");
		return NW_API_DB_ERROR;
	}

	return NW_API_OK;
}
#endif

/******************************************************************************
 * \brief    modify lag group admin state
 *
 * \param[in]   lag_group_id   - lag group id to disable
 *              new_admin_state   - true (enable), false (disable)
 *
 * \return      NW_API_OK - function succeed
 *              NW_API_FAILURE - group is not exist
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc nw_api_modify_lag_group_admin_state(unsigned int lag_group_id, bool new_admin_state)
{
	struct nw_lag_group_key lag_key;
	struct nw_lag_group_result lag_result;
	struct nw_db_lag_group_entry lag_group;
	int i;
	uint8_t members_count = 0, members_array[LAG_GROUP_MAX_MEMBERS] = {0};
	enum nw_api_rc nw_api_rc;

	/* read lag group from internal db */
	lag_group.lag_group_id = lag_group_id;
	nw_api_rc = internal_db_get_entry(LAG_GROUPS_INTERNAL_DB, (void *)&lag_group);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "read lag group %d from internal table was failed, failure on internal db or group id is not exist", lag_group_id);
		return nw_api_rc;
	}

	/* check if lag group is already enabled */
	if (lag_group.admin_state == new_admin_state) {
		return NW_API_FAILURE;
	}

	/* change admin state on internal db */
	lag_group.admin_state = new_admin_state;
	nw_api_rc = internal_db_modify_entry(LAG_GROUPS_INTERNAL_DB, (void *)&lag_group);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "modify lag group %d on internal table was failed, failure on internal db or group id is not exist", lag_group_id);
		return nw_api_rc;
	}

	if (new_admin_state == true) {
		/* enable all ports on this group */
		nw_api_rc = get_lag_group_members_array(lag_group_id, &members_count, members_array, true);
		if (nw_api_rc != NW_API_OK) {
			write_log(LOG_NOTICE, "error on internal db while creating members array");
			return nw_api_rc;
		}
		for (i = 0; i < members_count; i++) {
			nw_api_rc = nw_api_modify_lag_member_admin_state(lag_group_id, members_array[i], true);
			if (nw_api_rc != NW_API_OK) {
				write_log(LOG_NOTICE, "Error while changing members admin state");
				return nw_api_rc;
			}
		}
	}

	/* change admin state on lag entry result, all other values will stay the same */
	memset(&lag_result, 0, sizeof(struct nw_lag_group_result));
	lag_key.lag_group_id = lag_group_id;
	if (build_nps_lag_group_result(&lag_result, &lag_group) == false) {
		write_log(LOG_ERR, "Error while updating result on the DP table");
		return NW_API_FAILURE;
	}
	if (infra_modify_entry(STRUCT_ID_NW_LAG_GROUPS, &lag_key, sizeof(lag_key), &lag_result, sizeof(lag_result)) == false) {
		write_log(LOG_ERR, "Adding lag group failed, lag group id %d", lag_group_id);
		return NW_API_DB_ERROR;
	}

	if (new_admin_state == false) {
		/* disable all ports on this group */
		if (get_lag_group_members_array(lag_group_id, &members_count, members_array, false) != NW_API_OK) {
			write_log(LOG_NOTICE, "error on internal db while creating members array");
			return NW_API_DB_ERROR;
		}
		for (i = 0; i < members_count; i++) {
			nw_api_rc = nw_api_modify_lag_member_admin_state(lag_group_id, members_array[i], false);
			if (nw_api_rc != NW_API_OK) {
				write_log(LOG_NOTICE, "Error while removing port from lag group");
				return nw_api_rc;
			}
		}
	}

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       build fib key and mask for NPS according to cp_fib_entry
 *
 * \param[in]   cp_fib_entry - reference to cp fib entry
 *              nps_fib_key  - reference to nps fib key
 *              nps_fib_mask - reference to nps fib mask
 *
 * \return      none
 */
void build_nps_fib_key_and_mask(struct nw_db_fib_entry *cp_fib_entry,
				struct nw_fib_key *nps_fib_key,
				struct nw_fib_key *nps_fib_mask)
{
	/* Mask for TCAM entry */
	if (cp_fib_entry->mask_length == 0) {
		nps_fib_mask->dest_ip = 0;
	} else {
		nps_fib_mask->dest_ip = bswap_32(0xFFFFFFFF << (32 - cp_fib_entry->mask_length));
	}

	nps_fib_key->dest_ip = cp_fib_entry->dest_ip;
}

/**************************************************************************//**
 * \brief       build fib result for NPS according to cp_fib_entry
 *
 * \param[in]   cp_fib_entry   - reference to cp fib entry
 *              nps_fib_result - reference to nps fib result
 *
 * \return      none
 */
void build_nps_fib_result(struct nw_db_fib_entry *cp_fib_entry,
			  struct nw_fib_result *nps_fib_result)
{
	nps_fib_result->dest_ip = cp_fib_entry->next_hop;
	nps_fib_result->result_type = cp_fib_entry->result_type;
}

/**************************************************************************//**
 * \brief       Add fib entry to NPS
 *
 * \param[in]   cp_fib_entry   - reference to fib entry
 *
 * \return      true  - success
 *              false - fail
 */
bool add_fib_entry_to_nps(struct nw_db_fib_entry *cp_fib_entry)
{
	struct nw_fib_key nps_fib_key;
	struct nw_fib_key nps_fib_mask;
	struct nw_fib_result nps_fib_result;

	memset(&nps_fib_key, 0, sizeof(nps_fib_key));
	memset(&nps_fib_mask, 0, sizeof(nps_fib_mask));
	memset(&nps_fib_result, 0, sizeof(nps_fib_result));

	/* Add entry to FIB TCAM table based on CP FIB entry */
	build_nps_fib_key_and_mask(cp_fib_entry, &nps_fib_key, &nps_fib_mask);
	build_nps_fib_result(cp_fib_entry, &nps_fib_result);
	return infra_add_tcam_entry(NW_FIB_TCAM_SIDE,
				    NW_FIB_TCAM_TABLE,
				    &nps_fib_key,
				    sizeof(struct nw_fib_key),
				    &nps_fib_mask,
				    cp_fib_entry->nps_index,
				    &nps_fib_result,
				    sizeof(struct nw_fib_result));
}

/**************************************************************************//**
 * \brief       Delete fib entry from NPS
 *
 * \param[in]   cp_fib_entry   - reference to fib entry
 *
 * \return      true  - success
 *              false - fail
 */
bool delete_fib_entry_from_nps(struct nw_db_fib_entry *cp_fib_entry)
{
	struct nw_fib_key nps_fib_key;
	struct nw_fib_key nps_fib_mask;
	struct nw_fib_result nps_fib_result;

	memset(&nps_fib_key, 0, sizeof(nps_fib_key));
	memset(&nps_fib_mask, 0, sizeof(nps_fib_mask));
	memset(&nps_fib_result, 0, sizeof(nps_fib_result));

	/* Add entry to FIB TCAM table based on CP FIB entry */
	build_nps_fib_key_and_mask(cp_fib_entry, &nps_fib_key, &nps_fib_mask);
	build_nps_fib_result(cp_fib_entry, &nps_fib_result);
	return infra_delete_tcam_entry(NW_FIB_TCAM_SIDE,
				    NW_FIB_TCAM_TABLE,
				    &nps_fib_key,
				    sizeof(struct nw_fib_key),
				    &nps_fib_mask,
				    cp_fib_entry->nps_index,
				    &nps_fib_result,
				    sizeof(struct nw_fib_result));
}

/**************************************************************************//**
 * \brief       Push entries up, create a gap to insert the new entry, from each group of
 *              entries with the same mask we will move up to lower mask,
 *              for example if we will add an entry with mask 16, an entry with mask 0 will move up,
 *              instead of the moved entry we will move an entry from mask 1 group, and on.. until we have a gap
 *              to insert our new entry
 *
 * \param[in]   new_fib_entry   - reference to fib entry
 *
 * \return      NW_API_OK - All entries updated successfully
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc fib_reorder_push_entries_up(struct nw_db_fib_entry *new_fib_entry)
{
	int rc;
	char sql[256];
	sqlite3_stmt *statement;
	struct nw_db_fib_entry tmp_fib_entry;
	uint32_t current_mask, previous_updated_index = 0;

	memset(&tmp_fib_entry, 0, sizeof(tmp_fib_entry));

	write_log(LOG_DEBUG, "Reorder FIB table - push entries up.");


	for (current_mask = 0; current_mask < new_fib_entry->mask_length; current_mask++) {

		sprintf(sql, "SELECT * FROM fib_entries "
			"WHERE mask_length = %d "
			"ORDER BY nps_index ASC;", current_mask);

		/* Prepare SQL statement */
		rc = sqlite3_prepare_v2(nw_db, sql, -1, &statement, NULL);
		if (rc != SQLITE_OK) {
			write_log(LOG_CRIT, "SQL error: %s",
				  sqlite3_errmsg(nw_db));
			return NW_API_DB_ERROR;
		}

		/* Execute SQL statement */
		rc = sqlite3_step(statement);

		/* Error */
		if (rc < SQLITE_ROW) {
			write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(nw_db));
			sqlite3_finalize(statement);
			return NW_API_DB_ERROR;
		}

		if (rc == SQLITE_DONE) {
			/* No entries were found - continue to the next mask */
			write_log(LOG_DEBUG, "Reorder FIB table - no entries were found with mask %d", current_mask);
			sqlite3_finalize(statement);
			continue;
		} else {
			tmp_fib_entry.dest_ip = sqlite3_column_int(statement, 0);
			tmp_fib_entry.mask_length = sqlite3_column_int(statement, 1);
			tmp_fib_entry.result_type = (enum nw_fib_type)sqlite3_column_int(statement, 2);
			tmp_fib_entry.next_hop = sqlite3_column_int(statement, 3);
			if (current_mask == 0) {
				/* only one entry with mask 0, move its index up */
				previous_updated_index = sqlite3_column_int(statement, 4);
				tmp_fib_entry.nps_index = previous_updated_index + 1;
			} else {
				tmp_fib_entry.nps_index = previous_updated_index;
				previous_updated_index = sqlite3_column_int(statement, 4);
			}
			write_log(LOG_DEBUG, "Reorder FIB table - move entry (%s:%d) to index %d.",
				  nw_inet_ntoa(tmp_fib_entry.dest_ip), tmp_fib_entry.mask_length, tmp_fib_entry.nps_index);

			/* Update DBs */
			if (internal_db_modify_entry(FIB_ENTRIES_INTERNAL_DB, (void *)&tmp_fib_entry) == NW_API_DB_ERROR) {
				/* Internal error */
				write_log(LOG_CRIT, "Failed to update FIB entry (IP=%s, mask length=%d) (internal error).",
					  nw_inet_ntoa(tmp_fib_entry.dest_ip), tmp_fib_entry.mask_length);
				return NW_API_DB_ERROR;

			}
			if (add_fib_entry_to_nps(&tmp_fib_entry) == false) {
				write_log(LOG_CRIT, "Failed to update FIB entry (IP=%s, mask length=%d) in NPS.",
					  nw_inet_ntoa(tmp_fib_entry.dest_ip), tmp_fib_entry.mask_length);
				return NW_API_DB_ERROR;
			}
		}

		sqlite3_finalize(statement);


	}

	/* Take the index from the last updated entry  */
	new_fib_entry->nps_index = previous_updated_index;

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       Push entries down, instead of the deleted entry the function
 *              will insert an entry from s lower mask to the gap of the deleted entry,
 *              and later another move to the from lower mask and on until mask number 0,
 *
 * \param[in]   fib_entry   - reference to fib entry
 *
 * \return      NW_API_OK - All entries updated successfully
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc fib_reorder_push_entries_down(struct nw_db_fib_entry *fib_entry)
{
	int rc;
	char sql[256];
	sqlite3_stmt *statement;
	struct nw_db_fib_entry tmp_fib_entry;
	int current_mask;
	uint32_t previous_updated_index;

	memset(&tmp_fib_entry, 0, sizeof(tmp_fib_entry));

	write_log(LOG_DEBUG, "Reorder FIB table - push entries down.");

	previous_updated_index = fib_entry->nps_index;

	for (current_mask = fib_entry->mask_length; current_mask >= 0; current_mask--) {
		sprintf(sql, "SELECT * FROM fib_entries "
			"WHERE mask_length = %d "
			"ORDER BY nps_index DESC;",
			current_mask);

		/* Prepare SQL statement */
		rc = sqlite3_prepare_v2(nw_db, sql, -1, &statement, NULL);
		if (rc != SQLITE_OK) {
			write_log(LOG_CRIT, "SQL error: %s",
				  sqlite3_errmsg(nw_db));
			return NW_API_DB_ERROR;
		}

		/* Execute SQL statement */
		rc = sqlite3_step(statement);

		if (rc < SQLITE_ROW) {
			write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(nw_db));
			sqlite3_finalize(statement);
			return NW_API_DB_ERROR;
		}

		if (rc == SQLITE_DONE) {
			/* No entries were found - continue to the next mask */
			write_log(LOG_DEBUG, "Reorder FIB table - no entries were found with mask %d", current_mask);
			sqlite3_finalize(statement);
			continue;
		} else {

			tmp_fib_entry.dest_ip = sqlite3_column_int(statement, 0);
			tmp_fib_entry.mask_length = sqlite3_column_int(statement, 1);
			tmp_fib_entry.result_type = (enum nw_fib_type)sqlite3_column_int(statement, 2);
			tmp_fib_entry.next_hop = sqlite3_column_int(statement, 3);

			tmp_fib_entry.nps_index = previous_updated_index;
			previous_updated_index = sqlite3_column_int(statement, 4);

			write_log(LOG_DEBUG, "Reorder FIB table - move entry (%s:%d) to index %d.",
				nw_inet_ntoa(tmp_fib_entry.dest_ip), tmp_fib_entry.mask_length, tmp_fib_entry.nps_index);
			/* Update DBs */
			if (internal_db_modify_entry(FIB_ENTRIES_INTERNAL_DB, (void *)&tmp_fib_entry) == NW_API_DB_ERROR) {
				/* Internal error */
				write_log(LOG_CRIT, "Failed to update FIB entry (IP=%s, mask length=%d) (internal error).",
					  nw_inet_ntoa(tmp_fib_entry.dest_ip), tmp_fib_entry.mask_length);
				return NW_API_DB_ERROR;

			}
			if (add_fib_entry_to_nps(&tmp_fib_entry) == false) {
				write_log(LOG_CRIT, "Failed to update FIB entry (IP=%s, mask length=%d) in NPS.",
					  nw_inet_ntoa(tmp_fib_entry.dest_ip), tmp_fib_entry.mask_length);
				return NW_API_DB_ERROR;
			}
		}

		sqlite3_finalize(statement);

	}

	/* Update index of current entry to last index for deletion */
	fib_entry->nps_index = fib_entry_count - 1;

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       Fills fib_entry fields according to route_entry
 *
 * \param[in]   fib_entry        - reference to fib entry
 *              db_fib_entry     - reference to new db fib entry
 *
 * \return      none
 */
void set_fib_params(struct nw_api_fib_entry *fib_entry, struct nw_db_fib_entry *db_fib_entry)
{
	if (fib_entry->route_type == RTN_UNICAST) {
		if (fib_entry->next_hop_count == 0) {
			db_fib_entry->result_type = NW_FIB_NEIGHBOR;
			write_log(LOG_DEBUG, "FIB entry is NEIGHBOR - no next hop.");
		} else if (fib_entry->next_hop_count == 1) {
			/* Take next hop */
			db_fib_entry->next_hop = fib_entry->next_hop.in.s_addr;
			db_fib_entry->result_type = NW_FIB_GW;
			write_log(LOG_DEBUG, "FIB Entry is GW. next hop is %s", nw_inet_ntoa(db_fib_entry->next_hop));
		} else {
			/* Drop packet - DP will handle only single hop entries */
			db_fib_entry->result_type = NW_FIB_DROP;
			write_log(LOG_WARNING, "FIB entry (IP=%s, mask length=%d) has multiple hops - marked for drop.",
				  nw_inet_ntoa(db_fib_entry->dest_ip), db_fib_entry->mask_length);
		}
	} else {
		/* Drop packet - DP will handle only unicasts */
		db_fib_entry->result_type = NW_FIB_DROP;
		write_log(LOG_DEBUG, "FIB Entry is marked for drop.");
	}

}

/**************************************************************************//**
 * \brief       Add a fib_entry to NW DB
 *
 * \param[in]   fib_entry   - reference to fib entry
 *
 * \return      NW_API_OK - fib entry added successfully
 *              NW_API_DB_ERROR - failed to communicate with DB
 *              NW_API_FAILURE - fib entry already exists
 */
enum nw_api_rc nw_api_add_fib_entry(struct nw_api_fib_entry *fib_entry)
{
	struct nw_db_fib_entry cp_fib_entry;

	memset(&cp_fib_entry, 0, sizeof(cp_fib_entry));
	cp_fib_entry.mask_length = fib_entry->mask_length;
	cp_fib_entry.dest_ip = fib_entry->dest.in.s_addr;

	write_log(LOG_DEBUG, "Adding FIB entry. (IP=%s, mask length=%d) ",
		  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);

	switch (internal_db_get_entry(FIB_ENTRIES_INTERNAL_DB, (void *)&cp_fib_entry)) {
	case NW_API_OK:
		/* FIB entry already exists */
		write_log(LOG_NOTICE, "Can't add FIB entry. Entry (IP=%s, mask length=%d) already exists.",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_API_FAILURE;
	case NW_API_DB_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find FIB entry (internal error).");
		return NW_API_DB_ERROR;
	case NW_API_FAILURE:
		/* FIB entry doesn't exist in NW DB */
		write_log(LOG_DEBUG, "FIB entry (IP=%s, mask length=%d) doesn't exist in DB",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		break;
	default:
		return NW_API_DB_ERROR;
	}
	/* Check we do not pass the TCAM limit size */
	if (fib_entry_count == NW_FIB_TCAM_MAX_SIZE) {
		write_log(LOG_ERR, "Can't add FIB entry (IP=%s, mask length=%d). out of memory.",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_API_DB_ERROR;
	}

	set_fib_params(fib_entry, &cp_fib_entry);

	/* Choose where to put FIB entry */
	enum nw_api_rc rc = fib_reorder_push_entries_up(&cp_fib_entry);

	if (rc != NW_API_OK) {
		write_log(LOG_CRIT, "Failed to add FIB entry (IP=%s, mask length=%d).",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return rc;
	}
	/* Add new entry to DBs */
	if (internal_db_add_entry(FIB_ENTRIES_INTERNAL_DB, (void *)&cp_fib_entry) == NW_API_DB_ERROR) {
		/* Internal error */
		write_log(LOG_CRIT, "Failed to add FIB entry (IP=%s, mask length=%d) (internal error).",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_API_DB_ERROR;

	}
	if (add_fib_entry_to_nps(&cp_fib_entry) == false) {
		write_log(LOG_CRIT, "Failed to add FIB entry (IP=%s, mask length=%d) to NPS.",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_API_DB_ERROR;
	}
	fib_entry_count++;

	write_log(LOG_DEBUG, "FIB entry Added successfully. (IP=%s, mask length=%d, nps_index=%d, result_type=%d) ",
		  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length, cp_fib_entry.nps_index, cp_fib_entry.result_type);

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       Delete a fib entry from NW DB
 *
 * \param[in]   fib_entry   - reference to fib entry
 *
 * \return      NW_API_OK - fib entry deleted successfully
 *              NW_API_DB_ERROR - failed to communicate with DB
 *              NW_API_FAILURE - fib entry does not exist
 */
enum nw_api_rc nw_api_remove_fib_entry(struct nw_api_fib_entry *fib_entry)
{
	struct nw_db_fib_entry cp_fib_entry;

	memset(&cp_fib_entry, 0, sizeof(cp_fib_entry));
	cp_fib_entry.mask_length = fib_entry->mask_length;
	cp_fib_entry.dest_ip = fib_entry->dest.in.s_addr;

	switch (internal_db_get_entry(FIB_ENTRIES_INTERNAL_DB, (void *)&cp_fib_entry)) {
	case NW_API_OK:
		/* FIB entry exists */
		write_log(LOG_DEBUG, "FIB entry (IP=%s, mask length=%d, index=%d) found in internal DB",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length, cp_fib_entry.nps_index);
		break;
	case NW_API_DB_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find FIB entry (internal error).");
		return NW_API_DB_ERROR;
	case NW_API_FAILURE:
		/* FIB entry doesn't exist in NW DB */
		write_log(LOG_NOTICE, "Can't delete FIB entry. Entry (IP=%s, mask length=%d) doesn't exist.",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_API_FAILURE;
	default:
		return NW_API_DB_ERROR;
	}

	/* move entries if needed */
	if (cp_fib_entry.nps_index != fib_entry_count - 1) {
		/* not last entry - need to move entries down */
		enum nw_api_rc rc = fib_reorder_push_entries_down(&cp_fib_entry);

		if (rc != NW_API_OK) {
			write_log(LOG_CRIT, "Failed to delete FIB entry (IP=%s, mask length=%d).",
				  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
			return rc;
		}
	}

	fib_entry_count--;

	write_log(LOG_DEBUG, "Remove FIB entry (IP=%s, mask length=%d) from index %d",
		  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length, cp_fib_entry.nps_index);

	/* Delete last entry from DBs */
	if (internal_db_remove_entry(FIB_ENTRIES_INTERNAL_DB, (void *)&cp_fib_entry) == NW_API_DB_ERROR) {
		/* Internal error */
		write_log(LOG_CRIT, "Failed to delete FIB entry (IP=%s, mask length=%d) (internal error).",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_API_DB_ERROR;

	}
	if (delete_fib_entry_from_nps(&cp_fib_entry) == false) {
		write_log(LOG_CRIT, "Failed to delete FIB entry (IP=%s, mask length=%d) from NPS.",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_API_DB_ERROR;
	}

	write_log(LOG_DEBUG, "FIB entry (IP=%s, mask length=%d) deleted successfully.",
		  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       Modify a fib entry in NW DB
 *
 * \param[in]   fib_entry   - reference to fib entry
 *
 * \return      NW_API_OK - fib entry modified successfully
 *              NW_API_DB_ERROR - failed to communicate with DB
 *              NW_API_FAILURE - fib entry does not exist
 */
enum nw_api_rc nw_api_modify_fib_entry(struct nw_api_fib_entry *fib_entry)
{
	struct nw_db_fib_entry cp_fib_entry;

	memset(&cp_fib_entry, 0, sizeof(cp_fib_entry));
	cp_fib_entry.mask_length = fib_entry->mask_length;
	cp_fib_entry.dest_ip = fib_entry->dest.in.s_addr;

	switch (internal_db_get_entry(FIB_ENTRIES_INTERNAL_DB, (void *)&cp_fib_entry)) {
	case NW_API_OK:
		/* FIB entry exists */
		write_log(LOG_DEBUG, "FIB entry (IP=%s, mask length=%d, index=%d) found in internal DB",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length, cp_fib_entry.nps_index);
		break;
	case NW_API_DB_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find FIB entry (internal error).");
		return NW_API_DB_ERROR;
	case NW_API_FAILURE:
		/* FIB entry doesn't exist in NW DB */
		write_log(LOG_NOTICE, "Can't modify FIB entry. Entry (IP=%s, mask length=%d) doesn't exist.",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_API_FAILURE;
	default:
		return NW_API_DB_ERROR;
	}

	set_fib_params(fib_entry, &cp_fib_entry);

	/* Modify entry in DBs */
	if (internal_db_modify_entry(FIB_ENTRIES_INTERNAL_DB, (void *)&cp_fib_entry) == NW_API_DB_ERROR) {
		/* Internal error */
		write_log(LOG_CRIT, "Failed to modify FIB entry (IP=%s, mask length=%d) (internal error).",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_API_DB_ERROR;

	}
	if (add_fib_entry_to_nps(&cp_fib_entry) == false) {
		write_log(LOG_CRIT, "Failed to modify FIB entry (IP=%s, mask length=%d) in NPS.",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_API_DB_ERROR;
	}

	write_log(LOG_DEBUG, "FIB entry (IP=%s, mask length=%d) modified successfully.",
		  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       print interface statistics
 *
 * \param[in]   if_stats_base                  - interface statistics base
 * \param[in]   if_posted_stats_offsets_names  - array of interface posted statistics names
 * \param[in]   num_of_if_stats                - number of interface statistics
 *
 * \return	NW_API_OK - - operation succeeded
 *		NW_API_DB_ERROR - fail to read statistics
 */
enum nw_api_rc nw_db_print_interface_stats(ezdp_sum_addr_t if_stats_base, const char *if_posted_stats_offsets_names[], uint32_t num_of_if_stats)
{
	uint32_t error_index;
	uint64_t temp_sum;
	uint64_t *interface_counters = (uint64_t *)malloc(num_of_if_stats * sizeof(uint64_t));

	if (interface_counters == NULL) {
		return NW_API_DB_ERROR;
	}

	memset(interface_counters, 0, num_of_if_stats * sizeof(uint64_t));

	if (infra_get_posted_counters(if_stats_base,
				      num_of_if_stats,
				      interface_counters) == false) {
		write_log(LOG_CRIT, "Failed to read error statistics counters");
		free(interface_counters);
		return NW_API_DB_ERROR;
	}

	temp_sum = 0;
	for (error_index = 0; error_index < num_of_if_stats; error_index++) {
		if (interface_counters[error_index] > 0) {
			if (if_posted_stats_offsets_names[error_index] != NULL) {
				write_log(LOG_INFO, "    %s Counter: %-20lu",
					  if_posted_stats_offsets_names[error_index],
					  interface_counters[error_index]);
			} else {
				write_log(LOG_ERR, "    Problem printing statistics for error type %d", error_index);
			}
		}
		temp_sum += interface_counters[error_index];
	}
	if (temp_sum == 0) {
		write_log(LOG_INFO, "    No Errors On Counters");
	}
	free(interface_counters);
	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       print all interfaces statistics
 *
 * \return	NW_API_OK - - operation succeeded
 *		NW_API_DB_ERROR - fail to read statistics
 */
enum nw_api_rc nw_api_print_if_stats(void)
{
	uint32_t i;

	/* printing network interface error stats */
	for (i = 0; i < NW_IF_NUM; i++) {
		write_log(LOG_INFO, "Statistics of Network Interface %d", i);
		if (nw_db_print_interface_stats(NW_IF_STATS_POSTED_OFFSET + ((NW_BASE_LOGICAL_ID+i) * NW_NUM_OF_IF_STATS), nw_if_posted_stats_offsets_names, NW_NUM_OF_IF_STATS) != NW_API_OK) {
			return NW_API_DB_ERROR;
		}
	}

	/* printing host error stats */
	write_log(LOG_INFO, "Statistics of Host Interface");
	if (nw_db_print_interface_stats(HOST_IF_STATS_POSTED_OFFSET + (HOST_LOGICAL_ID * HOST_NUM_OF_IF_STATS), nw_if_posted_stats_offsets_names, HOST_NUM_OF_IF_STATS) != NW_API_OK) {
		return NW_API_DB_ERROR;
	}

	/* printing network interface error stats */
	for (i = 0; i < REMOTE_IF_NUM; i++) {
		write_log(LOG_INFO, "Statistics of Remote Interface %d", i);
		if (nw_db_print_interface_stats(REMOTE_IF_STATS_POSTED_OFFSET + ((REMOTE_BASE_LOGICAL_ID + i) * REMOTE_NUM_OF_IF_STATS), remote_if_posted_stats_offsets_names, REMOTE_NUM_OF_IF_STATS) != NW_API_OK) {
			return NW_API_DB_ERROR;
		}
	}

	return NW_API_OK;

}
