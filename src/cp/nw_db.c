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
*  Desc:                Implementation of network database.
*
*/

#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <byteswap.h>
#include <arpa/inet.h>
#include <EZapiTCAM.h>
#include "log.h"

#include <EZapiStat.h>
#include "nw_db.h"
#include "sqlite3.h"
#include "defs.h"
#include "infrastructure.h"

/* Global pointer to the DB */
sqlite3 *nw_db;
uint32_t fib_entry_count;

extern const char *nw_if_posted_stats_offsets_names[];

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
 * \return      NW_DB_INTERNAL_ERROR - Failed to create internal DB
 *              NW_DB_OK             - Created successfully
 */
enum nw_db_rc nw_db_init(void)
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
		return NW_DB_INTERNAL_ERROR;
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
		return NW_DB_INTERNAL_ERROR;
	}

	fib_entry_count = 0;

	return NW_DB_OK;
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

/**************************************************************************//**
 * \brief       Add a fib_entry to internal DB
 *
 * \param[in]   fib_entry   - reference to fib entry
 *
 * \return      NW_DB_OK - Add to internal DB succeed
 *              NW_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum nw_db_rc internal_db_add_fib_entry(struct nw_db_fib_entry *fib_entry)
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
		return NW_DB_INTERNAL_ERROR;
	}

	return NW_DB_OK;
}

/**************************************************************************//**
 * \brief       Modify a fib_entry in internal DB
 *
 * \param[in]   fib_entry   - reference to fib entry
 *
 * \return      NW_DB_OK - Modify of internal DB succeed
 *              NW_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum nw_db_rc internal_db_modify_fib_entry(struct nw_db_fib_entry *fib_entry)
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
		return NW_DB_INTERNAL_ERROR;
	}

	return NW_DB_OK;
}

/**************************************************************************//**
 * \brief       Remove a fib_entry from internal DB
 *
 * \param[in]   fib_entry   - reference to fib entry
 *
 * \return      NW_DB_OK - remove from internal db succeed
 *              NW_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum nw_db_rc internal_db_remove_fib_entry(struct nw_db_fib_entry *fib_entry)
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
		return NW_DB_INTERNAL_ERROR;
	}

	return NW_DB_OK;
}

/**************************************************************************//**
 * \brief       Searches a fib_entry in internal DB
 *
 * \param[in]   fib_entry   - reference to fib entry
 *
 * \return      NW_DB_OK - FIB entry found
 *              NW_DB_FAILURE - FIB entry not found
 *              NW_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum nw_db_rc internal_db_get_fib_entry(struct nw_db_fib_entry *fib_entry)
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
		return NW_DB_INTERNAL_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(nw_db));
		sqlite3_finalize(statement);
		return NW_DB_INTERNAL_ERROR;
	}

	/* FIB entry not found */
	if (rc == SQLITE_DONE) {
		sqlite3_finalize(statement);
		return NW_DB_FAILURE;
	}

	/* retrieve fib entry from result,
	 * finalize SQL statement and return
	 */
	fib_entry->result_type = (enum nw_fib_type)sqlite3_column_int(statement, 2);
	fib_entry->next_hop = sqlite3_column_int(statement, 3);
	fib_entry->nps_index = sqlite3_column_int(statement, 4);

	sqlite3_finalize(statement);

	return NW_DB_OK;
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
 * \brief       Take all entries in interanl DB with lower mask length than
 *              fib_entry and move them one index up.
 *              Also sets index of fib_entry to the new gap created (for insertion)
 *
 * \param[in]   fib_entry   - reference to fib entry
 *
 * \return      NW_DB_OK - All entries updated successfully
 *              NW_DB_INTERNAL_ERROR - failed to communicate with DB
 *              NW_DB_NPS_ERROR - failed to communicate with NPS
 */
enum nw_db_rc fib_reorder_push_entries_up(struct nw_db_fib_entry *new_fib_entry)
{
	int rc;
	char sql[256];
	sqlite3_stmt *statement;
	struct nw_db_fib_entry tmp_fib_entry;

	memset(&tmp_fib_entry, 0, sizeof(tmp_fib_entry));

	write_log(LOG_DEBUG, "Reorder FIB table - push entries up.");

	sprintf(sql, "SELECT * FROM fib_entries "
		"WHERE mask_length < %d "
		"ORDER BY nps_index DESC;",
		new_fib_entry->mask_length);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(nw_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(nw_db));
		return NW_DB_INTERNAL_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(nw_db));
		sqlite3_finalize(statement);
		return NW_DB_INTERNAL_ERROR;
	}

	if (rc == SQLITE_DONE) {
		/* No entries were found - put new entry at the end */
		write_log(LOG_DEBUG, "Reorder FIB table - no entries were found. put new entry at the end.");
		new_fib_entry->nps_index = fib_entry_count;
	} else {
		/* Go over all FIB entries and move them one index up */
		while (rc == SQLITE_ROW) {
			tmp_fib_entry.dest_ip = sqlite3_column_int(statement, 0);
			tmp_fib_entry.mask_length = sqlite3_column_int(statement, 1);
			tmp_fib_entry.result_type = (enum nw_fib_type)sqlite3_column_int(statement, 2);
			tmp_fib_entry.next_hop = sqlite3_column_int(statement, 3);
			tmp_fib_entry.nps_index = sqlite3_column_int(statement, 4);
			tmp_fib_entry.nps_index++;
			write_log(LOG_DEBUG, "Reorder FIB table - move entry (%s:%d) to index %d.",
				  nw_inet_ntoa(tmp_fib_entry.dest_ip), tmp_fib_entry.mask_length, tmp_fib_entry.nps_index);

			/* Update DBs */
			if (internal_db_modify_fib_entry(&tmp_fib_entry) == NW_DB_INTERNAL_ERROR) {
				/* Internal error */
				write_log(LOG_CRIT, "Failed to update FIB entry (IP=%s, mask length=%d) (internal error).",
					  nw_inet_ntoa(tmp_fib_entry.dest_ip), tmp_fib_entry.mask_length);
				return NW_DB_INTERNAL_ERROR;

			}
			if (add_fib_entry_to_nps(&tmp_fib_entry) == false) {
				write_log(LOG_CRIT, "Failed to update FIB entry (IP=%s, mask length=%d) in NPS.",
					  nw_inet_ntoa(tmp_fib_entry.dest_ip), tmp_fib_entry.mask_length);
				return NW_DB_NPS_ERROR;
			}
			rc = sqlite3_step(statement);
		}
		/* Error */
		if (rc < SQLITE_ROW) {
			write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(nw_db));
			sqlite3_finalize(statement);
			return NW_DB_INTERNAL_ERROR;
		}
		/* Take the index from the last updated entry (Add the new entry before all found entries) */
		new_fib_entry->nps_index = tmp_fib_entry.nps_index - 1;
	}
	sqlite3_finalize(statement);

	return NW_DB_OK;
}

/**************************************************************************//**
 * \brief       Take all entries in interanl DB with higher index than
 *              fib_entry and move them one index down.
 *              Also sets index of fib_entry to last (for deletion)
 *
 * \param[in]   fib_entry   - reference to fib entry
 *
 * \return      NW_DB_OK - All entries updated successfully
 *              NW_DB_INTERNAL_ERROR - failed to communicate with DB
 *              NW_DB_NPS_ERROR - failed to communicate with NPS
 */
enum nw_db_rc fib_reorder_push_entries_down(struct nw_db_fib_entry *fib_entry)
{
	int rc;
	char sql[256];
	sqlite3_stmt *statement;
	struct nw_db_fib_entry tmp_fib_entry;

	memset(&tmp_fib_entry, 0, sizeof(tmp_fib_entry));
	write_log(LOG_DEBUG, "Reorder FIB table - push entries down.");

	sprintf(sql, "SELECT * FROM fib_entries "
		"WHERE nps_index > %d "
		"ORDER BY nps_index ASC;",
		fib_entry->nps_index);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(nw_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(nw_db));
		return NW_DB_INTERNAL_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(nw_db));
		sqlite3_finalize(statement);
		return NW_DB_INTERNAL_ERROR;
	}

	/* Go over all FIB entries and move them one index down */
	while (rc == SQLITE_ROW) {
		tmp_fib_entry.dest_ip = sqlite3_column_int(statement, 0);
		tmp_fib_entry.mask_length = sqlite3_column_int(statement, 1);
		tmp_fib_entry.result_type = (enum nw_fib_type)sqlite3_column_int(statement, 2);
		tmp_fib_entry.next_hop = sqlite3_column_int(statement, 3);
		tmp_fib_entry.nps_index = sqlite3_column_int(statement, 4);
		tmp_fib_entry.nps_index--;
		write_log(LOG_DEBUG, "Reorder FIB table - move entry (%s:%d) to index %d.",
			nw_inet_ntoa(tmp_fib_entry.dest_ip), tmp_fib_entry.mask_length, tmp_fib_entry.nps_index);
		/* Update DBs */
		if (internal_db_modify_fib_entry(&tmp_fib_entry) == NW_DB_INTERNAL_ERROR) {
			/* Internal error */
			write_log(LOG_CRIT, "Failed to update FIB entry (IP=%s, mask length=%d) (internal error).",
				  nw_inet_ntoa(tmp_fib_entry.dest_ip), tmp_fib_entry.mask_length);
			return NW_DB_INTERNAL_ERROR;

		}
		if (add_fib_entry_to_nps(&tmp_fib_entry) == false) {
			write_log(LOG_CRIT, "Failed to update FIB entry (IP=%s, mask length=%d) in NPS.",
				  nw_inet_ntoa(tmp_fib_entry.dest_ip), tmp_fib_entry.mask_length);
			return NW_DB_NPS_ERROR;
		}
		rc = sqlite3_step(statement);
	}
	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(nw_db));
		sqlite3_finalize(statement);
		return NW_DB_INTERNAL_ERROR;
	}
	sqlite3_finalize(statement);

	/* Update index of current entry to last index for deletion */
	fib_entry->nps_index = fib_entry_count - 1;

	return NW_DB_OK;
}

/**************************************************************************//**
 * \brief       Fills fib_entry fields according to route_entry
 *
 * \param[in]   route_entry   - reference to route entry
 *              fib_entry     - reference to fib entry
 *
 * \return      none
 */
void set_fib_params(struct rtnl_route *route_entry, struct nw_db_fib_entry *fib_entry)
{
	if (rtnl_route_get_type(route_entry) == RTN_UNICAST) {
		/* Get next hop from route entry */
		struct nl_addr *next_hop_addr = rtnl_route_nh_get_gateway(rtnl_route_nexthop_n(route_entry, 0));

		if (next_hop_addr == NULL) {
			fib_entry->result_type = NW_FIB_NEIGHBOR;
			write_log(LOG_DEBUG, "FIB entry is NEIGHBOR - no next hop.");
		} else {
			if (rtnl_route_get_nnexthops(route_entry) > 1) {
				/* Drop packet - DP will handle only single hop entries */
				fib_entry->result_type = NW_FIB_DROP;
				write_log(LOG_WARNING, "FIB entry (IP=%s, mask length=%d) has multiple hops - marked for drop.",
					  nw_inet_ntoa(fib_entry->dest_ip), fib_entry->mask_length);
			} else {
				/* Take next hop */
				fib_entry->next_hop = *(uint32_t *)nl_addr_get_binary_addr(next_hop_addr);
				fib_entry->result_type = NW_FIB_GW;
				write_log(LOG_DEBUG, "FIB Entry is GW. next hop is %s", nw_inet_ntoa(fib_entry->next_hop));
			}
		}
	} else {
		/* Drop packet - DP will handle only unicasts */
		fib_entry->result_type = NW_FIB_DROP;
		write_log(LOG_DEBUG, "FIB Entry is marked for drop.");
	}

}

/**************************************************************************//**
 * \brief       Add a fib_entry to NW DB
 *
 * \param[in]   route_entry   - reference to route entry
 *
 * \return      NW_DB_OK - fib entry added successfully
 *              NW_DB_INTERNAL_ERROR - failed to communicate with DB
 *              NW_DB_NPS_ERROR - failed to communicate with NPS
 *              NW_DB_FAILURE - fib entry already exists
 */
enum nw_db_rc nw_db_add_fib_entry(struct rtnl_route *route_entry, bool reorder)
{
	struct nw_db_fib_entry cp_fib_entry;
	struct nl_addr *dst;

	memset(&cp_fib_entry, 0, sizeof(cp_fib_entry));

	dst = rtnl_route_get_dst(route_entry);
	cp_fib_entry.mask_length = (uint32_t)nl_addr_get_prefixlen(dst);
	cp_fib_entry.dest_ip = *(uint32_t *)nl_addr_get_binary_addr(dst);
	write_log(LOG_DEBUG, "Adding FIB entry. (IP=%s, mask length=%d) ",
		  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);

	switch (internal_db_get_fib_entry(&cp_fib_entry)) {
	case NW_DB_OK:
		/* FIB entry already exists */
		write_log(LOG_NOTICE, "Can't add FIB entry. Entry (IP=%s, mask length=%d) already exists.",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_DB_FAILURE;
	case NW_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find FIB entry (internal error).");
		return NW_DB_INTERNAL_ERROR;
	case NW_DB_FAILURE:
		/* FIB entry doesn't exist in NW DB */
		write_log(LOG_DEBUG, "FIB entry (IP=%s, mask length=%d) doesn't exist in DB",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		break;
	default:
		return NW_DB_INTERNAL_ERROR;
	}
	/* Check we do not pass the TCAM limit size */
	if (fib_entry_count == NW_FIB_TCAM_MAX_SIZE) {
		write_log(LOG_ERR, "Can't add FIB entry (IP=%s, mask length=%d). out of memory.",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_DB_INTERNAL_ERROR;
	}

	set_fib_params(route_entry, &cp_fib_entry);

	/* Choose where to put FIB entry */
	if (reorder) {
		enum nw_db_rc rc = fib_reorder_push_entries_up(&cp_fib_entry);

		if (rc != NW_DB_OK) {
			write_log(LOG_CRIT, "Failed to add FIB entry (IP=%s, mask length=%d).",
				  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
			return rc;
		}
	} else {
		/* Insert entry at the end (next free entry) */
		cp_fib_entry.nps_index = fib_entry_count;
	}
	/* Add new entry to DBs */
	if (internal_db_add_fib_entry(&cp_fib_entry) == NW_DB_INTERNAL_ERROR) {
		/* Internal error */
		write_log(LOG_CRIT, "Failed to add FIB entry (IP=%s, mask length=%d) (internal error).",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_DB_INTERNAL_ERROR;

	}
	if (add_fib_entry_to_nps(&cp_fib_entry) == false) {
		write_log(LOG_CRIT, "Failed to add FIB entry (IP=%s, mask length=%d) to NPS.",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_DB_NPS_ERROR;
	}
	fib_entry_count++;

	write_log(LOG_DEBUG, "FIB entry Added successfully. (IP=%s, mask length=%d, nps_index=%d, result_type=%d) ",
		  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length, cp_fib_entry.nps_index, cp_fib_entry.result_type);

	return NW_DB_OK;
}

/**************************************************************************//**
 * \brief       Delete a fib entry from NW DB
 *
 * \param[in]   route_entry   - reference to route entry
 *
 * \return      NW_DB_OK - fib entry deleted successfully
 *              NW_DB_INTERNAL_ERROR - failed to communicate with DB
 *              NW_DB_NPS_ERROR - failed to communicate with NPS
 *              NW_DB_FAILURE - fib entry do not exist
 */
enum nw_db_rc nw_db_delete_fib_entry(struct rtnl_route *route_entry)
{
	struct nw_db_fib_entry cp_fib_entry;
	struct nl_addr *dst;

	memset(&cp_fib_entry, 0, sizeof(cp_fib_entry));
	dst = rtnl_route_get_dst(route_entry);
	cp_fib_entry.mask_length = (uint32_t)nl_addr_get_prefixlen(dst);
	cp_fib_entry.dest_ip = *(uint32_t *)nl_addr_get_binary_addr(dst);

	switch (internal_db_get_fib_entry(&cp_fib_entry)) {
	case NW_DB_OK:
		/* FIB entry exists */
		write_log(LOG_DEBUG, "FIB entry (IP=%s, mask length=%d, index=%d) found in internal DB",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length, cp_fib_entry.nps_index);
		break;
	case NW_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find FIB entry (internal error).");
		return NW_DB_INTERNAL_ERROR;
	case NW_DB_FAILURE:
		/* FIB entry doesn't exist in NW DB */
		write_log(LOG_NOTICE, "Can't delete FIB entry. Entry (IP=%s, mask length=%d) doesn't exist.",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_DB_FAILURE;
	default:
		return NW_DB_INTERNAL_ERROR;
	}

	/* move entries if needed */
	if (cp_fib_entry.nps_index != fib_entry_count - 1) {
		/* not last entry - need to move entries down */
		enum nw_db_rc rc = fib_reorder_push_entries_down(&cp_fib_entry);

		if (rc != NW_DB_OK) {
			write_log(LOG_CRIT, "Failed to delete FIB entry (IP=%s, mask length=%d).",
				  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
			return rc;
		}
	}

	fib_entry_count--;

	write_log(LOG_DEBUG, "Remove FIB entry (IP=%s, mask length=%d) from index %d",
		  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length, cp_fib_entry.nps_index);

	/* Delete last entry from DBs */
	if (internal_db_remove_fib_entry(&cp_fib_entry) == NW_DB_INTERNAL_ERROR) {
		/* Internal error */
		write_log(LOG_CRIT, "Failed to delete FIB entry (IP=%s, mask length=%d) (internal error).",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_DB_INTERNAL_ERROR;

	}
	if (delete_fib_entry_from_nps(&cp_fib_entry) == false) {
		write_log(LOG_CRIT, "Failed to delete FIB entry (IP=%s, mask length=%d) from NPS.",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_DB_NPS_ERROR;
	}

	write_log(LOG_DEBUG, "FIB entry (IP=%s, mask length=%d) deleted successfully.",
		  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
	return NW_DB_OK;
}

/**************************************************************************//**
 * \brief       Modify a fib entry in NW DB
 *
 * \param[in]   route_entry   - reference to route entry
 *
 * \return      NW_DB_OK - fib entry modified successfully
 *              NW_DB_INTERNAL_ERROR - failed to communicate with DB
 *              NW_DB_NPS_ERROR - failed to communicate with NPS
 *              NW_DB_FAILURE - fib entry do not exist
 */
enum nw_db_rc nw_db_modify_fib_entry(struct rtnl_route *route_entry)
{
	struct nw_db_fib_entry cp_fib_entry;
	struct nl_addr *dst;

	memset(&cp_fib_entry, 0, sizeof(cp_fib_entry));
	dst = rtnl_route_get_dst(route_entry);
	cp_fib_entry.mask_length = (uint32_t)nl_addr_get_prefixlen(dst);
	cp_fib_entry.dest_ip = *(uint32_t *)nl_addr_get_binary_addr(dst);

	switch (internal_db_get_fib_entry(&cp_fib_entry)) {
	case NW_DB_OK:
		/* FIB entry exists */
		write_log(LOG_DEBUG, "FIB entry (IP=%s, mask length=%d, index=%d) found in internal DB",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length, cp_fib_entry.nps_index);
		break;
	case NW_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find FIB entry (internal error).");
		return NW_DB_INTERNAL_ERROR;
	case NW_DB_FAILURE:
		/* FIB entry doesn't exist in NW DB */
		write_log(LOG_NOTICE, "Can't modify FIB entry. Entry (IP=%s, mask length=%d) doesn't exist.",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_DB_FAILURE;
	default:
		return NW_DB_INTERNAL_ERROR;
	}

	set_fib_params(route_entry, &cp_fib_entry);

	/* Modify entry in DBs */
	if (internal_db_modify_fib_entry(&cp_fib_entry) == NW_DB_INTERNAL_ERROR) {
		/* Internal error */
		write_log(LOG_CRIT, "Failed to modify FIB entry (IP=%s, mask length=%d) (internal error).",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_DB_INTERNAL_ERROR;

	}
	if (add_fib_entry_to_nps(&cp_fib_entry) == false) {
		write_log(LOG_CRIT, "Failed to modify FIB entry (IP=%s, mask length=%d) in NPS.",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_DB_NPS_ERROR;
	}

	write_log(LOG_DEBUG, "FIB entry (IP=%s, mask length=%d) modified successfully.",
		  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
	return NW_DB_OK;
}



/**************************************************************************//**
 * \brief       print all interfaces statistics
 *
 * \return	NW_DB_OK - - operation succeeded
 *		NW_DB_NPS_ERROR - fail to read statistics
 */
enum nw_db_rc nw_db_print_all_interfaces_stats(void)
{
	uint32_t i;

	/* printing interface error stats */
	for (i = 0; i < USER_NW_IF_NUM; i++) {
		write_log(LOG_INFO, "Statistics of Network Interface %d", i);
		if (nw_db_print_interface_stats(USER_BASE_LOGICAL_ID+i) != NW_DB_OK) {
			return NW_DB_NPS_ERROR;
		}
	}

	/* printing host error stats */
	write_log(LOG_INFO, "Statistics of Host Interface");
	if (nw_db_print_interface_stats(USER_HOST_LOGICAL_ID) != NW_DB_OK) {
		return NW_DB_NPS_ERROR;
	}

	return NW_DB_OK;

}

/**************************************************************************//**
 * \brief       print interface statistics
 *
 * \param[in]   interface - the interface number that need to print
 *
 * \return	NW_DB_OK - - operation succeeded
 *		NW_DB_NPS_ERROR - fail to read statistics
 */
enum nw_db_rc nw_db_print_interface_stats(unsigned int interface)
{
	uint32_t error_index;
	uint64_t temp_sum;
	uint64_t interface_counters[NW_NUM_OF_IF_STATS] = {0};

	if (infra_get_posted_counters(EMEM_IF_STATS_POSTED_OFFSET + (interface * NW_NUM_OF_IF_STATS),
				      NW_NUM_OF_IF_STATS,
				      interface_counters) == false) {
		write_log(LOG_CRIT, "Failed to read error statistics counters");
		return NW_DB_NPS_ERROR;
	}

	temp_sum = 0;
	for (error_index = 0; error_index < NW_NUM_OF_IF_STATS; error_index++) {
		if (interface_counters[error_index] > 0) {
			if (nw_if_posted_stats_offsets_names[error_index] != NULL) {
				write_log(LOG_INFO, "    %s Counter: %-20lu",
					  nw_if_posted_stats_offsets_names[error_index],
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

	return NW_DB_OK;
}

