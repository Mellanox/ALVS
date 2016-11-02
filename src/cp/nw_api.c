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

#include "log.h"
#include "nw_api.h"
#include "sqlite3.h"
#include "defs.h"
#include "infrastructure.h"
#include "nw_search_defs.h"

/* Global pointer to the DB */
sqlite3 *nw_db;
uint32_t fib_entry_count;

extern const char *nw_if_posted_stats_offsets_names[];
extern const char *remote_if_posted_stats_offsets_names[];

struct nw_db_fib_entry {
	in_addr_t                  dest_ip;
	uint16_t                   mask_length;
	enum nw_fib_type           result_type;
	in_addr_t                  next_hop;
	uint16_t                   nps_index;
};

#define NW_DB_FILE_NAME "nw.db"

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
 * \brief       Add a fib_entry to internal DB
 *
 * \param[in]   fib_entry   - reference to fib entry
 *
 * \return      NW_API_OK - Add to internal DB succeed
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc internal_db_add_fib_entry(struct nw_db_fib_entry *fib_entry)
{
	int rc;
	char sql[256];
	char *zErrMsg = NULL;

	sprintf(sql, "INSERT INTO fib_entries "
		"(dest_ip, mask_length, result_type, next_hop, nps_index) "
		"VALUES (%d, %d, %d, %d, %d);",
		fib_entry->dest_ip, fib_entry->mask_length, fib_entry->result_type, fib_entry->next_hop, fib_entry->nps_index);

	/* Execute SQL statement */
	rc = sqlite3_exec(nw_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return NW_API_DB_ERROR;
	}

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       Modify a fib_entry in internal DB
 *
 * \param[in]   fib_entry   - reference to fib entry
 *
 * \return      NW_API_OK - Modify of internal DB succeed
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc internal_db_modify_fib_entry(struct nw_db_fib_entry *fib_entry)
{
	int rc;
	char sql[256];
	char *zErrMsg = NULL;

	sprintf(sql, "UPDATE fib_entries "
		"SET result_type=%d, next_hop=%d, nps_index=%d "
		"WHERE dest_ip=%d AND mask_length=%d;",
		fib_entry->result_type, fib_entry->next_hop, fib_entry->nps_index, fib_entry->dest_ip, fib_entry->mask_length);

	/* Execute SQL statement */
	rc = sqlite3_exec(nw_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return NW_API_DB_ERROR;
	}

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       Remove a fib_entry from internal DB
 *
 * \param[in]   fib_entry   - reference to fib entry
 *
 * \return      NW_API_OK - remove from internal db succeed
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc internal_db_remove_fib_entry(struct nw_db_fib_entry *fib_entry)
{
	int rc;
	char sql[256];
	char *zErrMsg = NULL;

	sprintf(sql, "DELETE FROM fib_entries "
		"WHERE dest_ip=%d AND mask_length=%d;",
		fib_entry->dest_ip, fib_entry->mask_length);

	/* Execute SQL statement */
	rc = sqlite3_exec(nw_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return NW_API_DB_ERROR;
	}

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       Searches a fib_entry in internal DB
 *
 * \param[in]   fib_entry   - reference to fib entry
 *
 * \return      NW_API_OK - FIB entry found
 *              NW_API_FAILURE - FIB entry not found
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc internal_db_get_fib_entry(struct nw_db_fib_entry *fib_entry)
{
	int rc;
	char sql[256];
	sqlite3_stmt *statement;

	sprintf(sql, "SELECT * FROM fib_entries "
		"WHERE dest_ip=%d AND mask_length=%d;",
		fib_entry->dest_ip, fib_entry->mask_length);

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

	/* retrieve fib entry from result,
	 * finalize SQL statement and return
	 */
	fib_entry->result_type = (enum nw_fib_type)sqlite3_column_int(statement, 2);
	fib_entry->next_hop = sqlite3_column_int(statement, 3);
	fib_entry->nps_index = sqlite3_column_int(statement, 4);

	sqlite3_finalize(statement);

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
			if (internal_db_modify_fib_entry(&tmp_fib_entry) == NW_API_DB_ERROR) {
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
			if (internal_db_modify_fib_entry(&tmp_fib_entry) == NW_API_DB_ERROR) {
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

	switch (internal_db_get_fib_entry(&cp_fib_entry)) {
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
	if (internal_db_add_fib_entry(&cp_fib_entry) == NW_API_DB_ERROR) {
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

	switch (internal_db_get_fib_entry(&cp_fib_entry)) {
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
	if (internal_db_remove_fib_entry(&cp_fib_entry) == NW_API_DB_ERROR) {
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

	switch (internal_db_get_fib_entry(&cp_fib_entry)) {
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
	if (internal_db_modify_fib_entry(&cp_fib_entry) == NW_API_DB_ERROR) {
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
	for (i = 0; i < USER_NW_IF_NUM; i++) {
		write_log(LOG_INFO, "Statistics of Network Interface %d", i);
		if (nw_db_print_interface_stats(EMEM_NW_IF_STATS_POSTED_OFFSET + ((USER_NW_BASE_LOGICAL_ID+i) * NW_NUM_OF_IF_STATS), nw_if_posted_stats_offsets_names, NW_NUM_OF_IF_STATS) != NW_API_OK) {
			return NW_API_DB_ERROR;
		}
	}

	/* printing host error stats */
	write_log(LOG_INFO, "Statistics of Host Interface");
	if (nw_db_print_interface_stats(EMEM_HOST_IF_STATS_POSTED_OFFSET + (USER_HOST_LOGICAL_ID * HOST_NUM_OF_IF_STATS), nw_if_posted_stats_offsets_names, HOST_NUM_OF_IF_STATS) != NW_API_OK) {
		return NW_API_DB_ERROR;
	}

	/* printing network interface error stats */
	for (i = 0; i < USER_REMOTE_IF_NUM; i++) {
		write_log(LOG_INFO, "Statistics of Remote Interface %d", i);
		if (nw_db_print_interface_stats(EMEM_REMOTE_IF_STATS_POSTED_OFFSET + ((USER_REMOTE_BASE_LOGICAL_ID+i) * REMOTE_NUM_OF_IF_STATS), remote_if_posted_stats_offsets_names, REMOTE_NUM_OF_IF_STATS) != NW_API_OK) {
			return NW_API_DB_ERROR;
		}
	}

	return NW_API_OK;

}
