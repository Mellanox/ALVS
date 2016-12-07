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
#include <arpa/inet.h>

#include "log.h"
#include "nw_api.h"
#include "nw_db.h"
#include "sqlite3.h"
#include "nw_conf.h"
#include "infrastructure_utils.h"
#include "nw_search_defs.h"
#include "cfg.h"

uint32_t fib_entry_count;

extern sqlite3 *nw_db;
extern const char *nw_if_posted_stats_offsets_names[];
extern const char *remote_if_posted_stats_offsets_names[];

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
	memset(nps_fib_key, 0, sizeof(struct nw_fib_key));
	memset(nps_fib_mask, 0, sizeof(struct nw_fib_key));

	/* Mask for TCAM entry */
	if (cp_fib_entry->mask_length == 0) {
		nps_fib_mask->dest_ip = 0;
	} else {
		nps_fib_mask->dest_ip = bswap_32(0xFFFFFFFF << (32 - cp_fib_entry->mask_length));
	}

	/* dest_ip is received in BE, so no bswap is needed */
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
	memset(nps_fib_result, 0, sizeof(struct nw_fib_result));

	/* next_hop is received in BE, so no bswap is needed */
	nps_fib_result->dest_ip = cp_fib_entry->next_hop;
	nps_fib_result->result_type = cp_fib_entry->result_type;
	nps_fib_result->is_lag = cp_fib_entry->is_lag;
	if (cp_fib_entry->is_lag == true) {
		nps_fib_result->lag_index = cp_fib_entry->output_index;
	} else {
		nps_fib_result->output_interface = cp_fib_entry->output_index;
	}

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
			tmp_fib_entry.is_lag = sqlite3_column_int(statement, 5);
			tmp_fib_entry.output_index = sqlite3_column_int(statement, 6);
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
			if (internal_db_modify_entry(FIB_ENTRIES_INTERNAL_DB, (void *)&tmp_fib_entry) == NW_DB_ERROR) {
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
			tmp_fib_entry.is_lag = sqlite3_column_int(statement, 5);
			tmp_fib_entry.output_index = sqlite3_column_int(statement, 6);

			write_log(LOG_DEBUG, "Reorder FIB table - move entry (%s:%d) to index %d.",
				nw_inet_ntoa(tmp_fib_entry.dest_ip), tmp_fib_entry.mask_length, tmp_fib_entry.nps_index);
			/* Update DBs */
			if (internal_db_modify_entry(FIB_ENTRIES_INTERNAL_DB, (void *)&tmp_fib_entry) == NW_DB_ERROR) {
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
 * \brief       Return the mapped logical id / lag group
 *
 * \param[in]   if_index      - reference to interface index
 * \param[out]  output_index  - reference to interface logical id / lag_group
 *
 * \return      NW_API_OK       - logical id / lag group retrieved successfully
 *              NW_API_FAILURE  - if index does not exists
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc get_output_index(uint8_t if_index, uint32_t *output_index)
{
	struct nw_db_nw_interface interface_data;

	/* read interface id from internal db */
	interface_data.interface_id = if_index;
	switch (internal_db_get_entry(NW_INTERFACES_INTERNAL_DB, (void *)&interface_data)) {
	case NW_DB_OK:
		break;
	case NW_DB_ERROR:
		write_log(LOG_ERR, "Failed to read interface %d from internal DB. (internal error)", if_index);
		return NW_API_DB_ERROR;
	case NW_DB_FAILURE:
		write_log(LOG_NOTICE, "Failed to read nw interface %d from internal DB. interface does not exist", if_index);
		return NW_API_FAILURE;
	}

	/* check if we are running in lag mode */
	if (system_cfg_is_lag_en() == true) {
		*output_index = interface_data.lag_group_id;
	} else {
		*output_index = interface_data.logical_id;
	}

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       Fills fib_entry fields according to route_entry
 *
 * \param[in]   fib_entry        - reference to nw fib entry
 *              cp_fib_entry     - reference to new cp fib entry
 *
 * \return      none
 */
enum nw_api_rc  set_fib_params(struct nw_api_fib_entry *fib_entry, struct nw_db_fib_entry *cp_fib_entry)
{
	if (fib_entry->route_type == RTN_UNICAST) {
		if (fib_entry->next_hop_count == 0) {
			cp_fib_entry->result_type = NW_FIB_NEIGHBOR;
			write_log(LOG_DEBUG, "FIB entry is NEIGHBOR - no next hop.");
		} else if (fib_entry->next_hop_count == 1) {
			/* Take next hop */
			cp_fib_entry->next_hop = fib_entry->next_hop.in.s_addr;
			cp_fib_entry->result_type = NW_FIB_GW;
			write_log(LOG_DEBUG, "FIB Entry is GW. next hop is %s", nw_inet_ntoa(cp_fib_entry->next_hop));
		} else {
			/* Unsupported number of hop entries - DP will handle it according to the application */
			cp_fib_entry->result_type = NW_FIB_UNSUPPORTED;
			write_log(LOG_DEBUG, "FIB entry (IP=%s, mask length=%d) has multiple hops - marked as unsupported.",
				  nw_inet_ntoa(cp_fib_entry->dest_ip), cp_fib_entry->mask_length);
		}
	} else if (fib_entry->route_type == RTN_BLACKHOLE) {
		/* Drop packet */
		cp_fib_entry->result_type = NW_FIB_DROP;
		write_log(LOG_DEBUG, "FIB Entry is marked for drop.");
	} else {
		/* Unsupported route type - DP will handle it according to the application */
		cp_fib_entry->result_type = NW_FIB_UNSUPPORTED;
		write_log(LOG_DEBUG, "Unsupported route type - DP will handle it according to the application.");
	}

	cp_fib_entry->is_lag = system_cfg_is_lag_en();
	return get_output_index(fib_entry->output_if_index, &cp_fib_entry->output_index);
}

/******************************************************************************
 * \brief       set FIB key params in CP FIB entry
 *
 * \param[in]   fib_entry   - reference to ARP entry
 *              cp_fib_entry- reference to CP ARP entry
 *
 * \return      None
 */
void set_fib_cp_key(struct nw_db_fib_entry *cp_fib_entry, struct nw_api_fib_entry *fib_entry)
{
	memset(cp_fib_entry, 0, sizeof(struct nw_db_fib_entry));
	cp_fib_entry->mask_length = fib_entry->mask_length;
	cp_fib_entry->dest_ip = fib_entry->dest.in.s_addr;
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
	enum nw_api_rc rc;

	set_fib_cp_key(&cp_fib_entry, fib_entry);
	write_log(LOG_DEBUG, "Adding FIB entry. (IP=%s, mask length=%d, IF index %d) ",
		  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length, fib_entry->output_if_index);
	switch (internal_db_get_entry(FIB_ENTRIES_INTERNAL_DB, (void *)&cp_fib_entry)) {
	case NW_DB_OK:
		/* FIB entry already exists */
		write_log(LOG_NOTICE, "Can't add FIB entry. Entry (IP=%s, mask length=%d) already exists.",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_API_FAILURE;
	case NW_DB_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find FIB entry (internal error).");
		return NW_API_DB_ERROR;
	case NW_DB_FAILURE:
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
	rc = set_fib_params(fib_entry, &cp_fib_entry);
	if (rc != NW_API_OK) {
		return rc;
	}

	/* Choose where to put FIB entry */
	rc = fib_reorder_push_entries_up(&cp_fib_entry);

	if (rc != NW_API_OK) {
		write_log(LOG_CRIT, "Failed to add FIB entry (IP=%s, mask length=%d).",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return rc;
	}
	/* Add new entry to DBs */
	if (internal_db_add_entry(FIB_ENTRIES_INTERNAL_DB, (void *)&cp_fib_entry) == NW_DB_ERROR) {
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

	write_log(LOG_DEBUG, "FIB entry Added successfully. (IP=%s, mask length=%d, nps_index=%d, result_type=%d, is_lag=%d, output_index=%d) ",
		  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length, cp_fib_entry.nps_index, cp_fib_entry.result_type, cp_fib_entry.is_lag, cp_fib_entry.output_index);

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

	set_fib_cp_key(&cp_fib_entry, fib_entry);
	write_log(LOG_DEBUG, "Deleting FIB entry. (IP=%s, mask length=%d, IF index %d) ",
		  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length, fib_entry->output_if_index);

	switch (internal_db_get_entry(FIB_ENTRIES_INTERNAL_DB, (void *)&cp_fib_entry)) {
	case NW_DB_OK:
		/* FIB entry exists */
		write_log(LOG_DEBUG, "FIB entry (IP=%s, mask length=%d, index=%d) found in internal DB",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length, cp_fib_entry.nps_index);
		break;
	case NW_DB_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find FIB entry (internal error).");
		return NW_API_DB_ERROR;
	case NW_DB_FAILURE:
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
	if (internal_db_remove_entry(FIB_ENTRIES_INTERNAL_DB, (void *)&cp_fib_entry) == NW_DB_ERROR) {
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

	write_log(LOG_DEBUG, "FIB entry deleted successfully. (IP=%s, mask length=%d, nps_index=%d, result_type=%d, is_lag=%d, output_index=%d) ",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length, cp_fib_entry.nps_index, cp_fib_entry.result_type, cp_fib_entry.is_lag, cp_fib_entry.output_index);

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
	enum nw_api_rc rc;

	set_fib_cp_key(&cp_fib_entry, fib_entry);
	write_log(LOG_DEBUG, "Modifying FIB entry. (IP=%s, mask length=%d, IF index %d) ",
		  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length, fib_entry->output_if_index);

	switch (internal_db_get_entry(FIB_ENTRIES_INTERNAL_DB, (void *)&cp_fib_entry)) {
	case NW_DB_OK:
		/* FIB entry exists */
		write_log(LOG_DEBUG, "FIB entry (IP=%s, mask length=%d, index=%d) found in internal DB",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length, cp_fib_entry.nps_index);
		break;
	case NW_DB_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find FIB entry (internal error).");
		return NW_API_DB_ERROR;
	case NW_DB_FAILURE:
		/* FIB entry doesn't exist in NW DB */
		write_log(LOG_NOTICE, "Can't modify FIB entry. Entry (IP=%s, mask length=%d) doesn't exist.",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length);
		return NW_API_FAILURE;
	default:
		return NW_API_DB_ERROR;
	}

	rc = set_fib_params(fib_entry, &cp_fib_entry);
	if (rc != NW_API_OK) {
		return rc;
	}

	/* Modify entry in DBs */
	if (internal_db_modify_entry(FIB_ENTRIES_INTERNAL_DB, (void *)&cp_fib_entry) == NW_DB_ERROR) {
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

	write_log(LOG_DEBUG, "FIB entry modified successfully. (IP=%s, mask length=%d, nps_index=%d, result_type=%d, is_lag=%d, output_index=%d) ",
			  nw_inet_ntoa(cp_fib_entry.dest_ip), cp_fib_entry.mask_length, cp_fib_entry.nps_index, cp_fib_entry.result_type, cp_fib_entry.is_lag, cp_fib_entry.output_index);

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       build ARP key for NPS according to cp_arp_entry
 *
 * \param[in]   cp_arp_entry - reference to cp arp entry
 * \param[out]  nps_arp_key  - reference to nps arp key
 *
 * \return      none
 */
void build_nps_arp_key(struct nw_db_arp_entry *cp_arp_entry, struct nw_arp_key *nps_arp_key)
{
	memset(nps_arp_key, 0, sizeof(struct nw_arp_key));

	nps_arp_key->ip = cp_arp_entry->entry_ip;
	nps_arp_key->is_lag = cp_arp_entry->is_lag;
	nps_arp_key->out_index = cp_arp_entry->output_index;
}

/**************************************************************************//**
 * \brief       build ARP result for NPS according to cp_arp_entry
 *
 * \param[in]   cp_arp_entry   - reference to cp_arp_entry
 * \param[out]  nps_arp_result - reference to nps_arp_result
 *
 * \return      none
 */
void build_nps_arp_result(struct nw_db_arp_entry *cp_arp_entry, struct nw_arp_result *nps_arp_result)
{
	memset(nps_arp_result, 0, sizeof(struct nw_arp_result));

	memcpy(nps_arp_result->dest_mac_addr.ether_addr_octet, cp_arp_entry->dest_mac_address.ether_addr_octet, ETH_ALEN);
}

/******************************************************************************
 * \brief       set ARP key params in CP ARP entry
 *
 * \param[in]   arp_entry   - reference to ARP entry
 *              cp_arp_entry- reference to CP ARP entry
 *
 * \return      NW_API_OK - ARP entry key set successfully
 *              NW_API_DB_ERROR - failed to communicate with DB
 *              NW_API_FAILURE
 */
enum nw_api_rc set_arp_cp_key(struct nw_db_arp_entry *cp_arp_entry, struct nw_api_arp_entry *arp_entry)
{
	memset(cp_arp_entry, 0, sizeof(struct nw_db_arp_entry));
	cp_arp_entry->entry_ip = arp_entry->ip_addr.in.s_addr;
	cp_arp_entry->is_lag = system_cfg_is_lag_en();
	return get_output_index(arp_entry->if_index, &cp_arp_entry->output_index);
}

/**************************************************************************//**
 * \brief       Add a ARP entry to NW DB
 *
 * \param[in]   arp_entry   - reference to ARP entry
 *
 * \return      NW_API_OK - ARP entry added successfully
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc nw_api_add_arp_entry(struct nw_api_arp_entry *arp_entry)
{
	struct nw_arp_result result;
	struct nw_arp_key key;
	struct nw_db_arp_entry cp_arp_entry;
	enum nw_api_rc rc;
	uint32_t arp_entries_count = 0;

	write_log(LOG_DEBUG, "Add neighbor to arp table. IP = %s MAC = %02x:%02x:%02x:%02x:%02x:%02x", nw_inet_ntoa(arp_entry->ip_addr.in.s_addr), arp_entry->mac_addr.ether_addr_octet[0], arp_entry->mac_addr.ether_addr_octet[1], arp_entry->mac_addr.ether_addr_octet[2], arp_entry->mac_addr.ether_addr_octet[3], arp_entry->mac_addr.ether_addr_octet[4], arp_entry->mac_addr.ether_addr_octet[5]);

	/* check if arp entry already exist on internal db */
	rc = set_arp_cp_key(&cp_arp_entry, arp_entry);
	if (rc != NW_API_OK) {
		return rc;
	}
	switch (internal_db_get_entry(ARP_ENTRIES_INTERNAL_DB, (void *)&cp_arp_entry)) {
	case NW_DB_OK:
		/* ARP entry already exists */
		write_log(LOG_NOTICE, "Can't add ARP entry. Entry (IP=%s) already exists.", nw_inet_ntoa(cp_arp_entry.entry_ip));
		return NW_API_FAILURE;
	case NW_DB_ERROR:
		/* Internal error */
		write_log(LOG_ERR,  "Can't add ARP entry (IP=%s). Received error from internal DB.", nw_inet_ntoa(cp_arp_entry.entry_ip));
		return NW_API_DB_ERROR;
	case NW_DB_FAILURE:
		/* ARP entry doesn't exist on NW DB - Can add it.*/
		break;
	default:
		return NW_API_DB_ERROR;
	}

	/* check if reached to max num of arp entries */
	if (internal_db_get_arp_entries_count(&arp_entries_count) != NW_API_OK) {
		write_log(LOG_ERR, "Error while counting ARP entries from internal DB");
		return NW_API_DB_ERROR;
	}
	if (arp_entries_count >= NW_ARP_TABLE_MAX_ENTRIES) {
		write_log(LOG_ERR, "Error, reached to the maximum number of ARP entries");
		return NW_API_FAILURE;
	}

	/* update dest_mac_addr */
	memcpy(cp_arp_entry.dest_mac_address.ether_addr_octet, arp_entry->mac_addr.ether_addr_octet, ETH_ALEN);

	/* build ARP key & result */
	build_nps_arp_key(&cp_arp_entry, &key);
	build_nps_arp_result(&cp_arp_entry, &result);

	/* add ARP entry to internal DB */
	if (internal_db_add_entry(ARP_ENTRIES_INTERNAL_DB, (void *)&cp_arp_entry) != NW_DB_OK) {
		write_log(LOG_NOTICE, "Error adding ARP entry to internal DB");
		return NW_API_DB_ERROR;
	}

	if (infra_add_entry(STRUCT_ID_NW_ARP, &key, sizeof(key), &result, sizeof(result)) == false) {
		write_log(LOG_ERR, "Cannot add entry to ARP table IP = %s MAC = %02x:%02x:%02x:%02x:%02x:%02x", nw_inet_ntoa(arp_entry->ip_addr.in.s_addr), arp_entry->mac_addr.ether_addr_octet[0], arp_entry->mac_addr.ether_addr_octet[1], arp_entry->mac_addr.ether_addr_octet[2], arp_entry->mac_addr.ether_addr_octet[3], arp_entry->mac_addr.ether_addr_octet[4], arp_entry->mac_addr.ether_addr_octet[5]);
		return NW_API_DB_ERROR;
	}

	write_log(LOG_DEBUG, "ARP entry Added successfully. (IP = %s, is_lag = %d, output_if = %d)", nw_inet_ntoa(arp_entry->ip_addr.in.s_addr), cp_arp_entry.is_lag, cp_arp_entry.output_index);

	return NW_API_OK;
}

/******************************************************************************
 * \brief       Remove ARP entry from NW DB
 *
 * \param[in]   arp_entry   - reference to ARP entry
 *
 * \return      NW_API_OK - ARP entry removed successfully
 *              NW_API_FAILURE - ARP entry does not exist
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc nw_api_remove_arp_entry(struct nw_api_arp_entry *arp_entry)
{
	struct nw_arp_key key;
	struct nw_db_arp_entry cp_arp_entry;
	enum nw_api_rc rc;

	write_log(LOG_DEBUG, "Remove neighbor from ARP table. IP = %s", nw_inet_ntoa(arp_entry->ip_addr.in.s_addr));

	/* check if ARP entry exists in internal db */
	rc = set_arp_cp_key(&cp_arp_entry, arp_entry);
	if (rc != NW_API_OK) {
		return rc;
	}
	switch (internal_db_get_entry(ARP_ENTRIES_INTERNAL_DB, (void *)&cp_arp_entry)) {
	case NW_DB_OK:
		/* ARP entry exists */
		write_log(LOG_DEBUG, "ARP entry (IP=%s) found in internal DB", nw_inet_ntoa(cp_arp_entry.entry_ip));
		break;
	case NW_DB_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Received an error trying to delete ARP entry.");
		return NW_API_DB_ERROR;
	case NW_DB_FAILURE:
		/* ARP entry doesn't exist in NW DB, no need to delete */
		write_log(LOG_DEBUG, "ARP entry (IP=%s) not found in internal DB, no need to delete.", nw_inet_ntoa(cp_arp_entry.entry_ip));
		return NW_API_FAILURE;
	default:
		return NW_API_DB_ERROR;
	}

	/* remove ARP entry from internal db */
	if (internal_db_remove_entry(ARP_ENTRIES_INTERNAL_DB, (void *)&cp_arp_entry) != NW_DB_OK) {
		write_log(LOG_ERR, "Received an error trying to delete ARP entry.");
		return NW_API_DB_ERROR;
	}

	/* remove entry from NPS table */
	build_nps_arp_key(&cp_arp_entry, &key);
	if (!infra_delete_entry(STRUCT_ID_NW_ARP, &key, sizeof(key))) {
		write_log(LOG_ERR, "Cannot remove entry from ARP table IP = %s", nw_inet_ntoa(arp_entry->ip_addr.in.s_addr));
		return NW_API_DB_ERROR;
	}

	write_log(LOG_DEBUG, "ARP entry Deleted successfully. (IP = %s, is_lag = %d, output_if = %d)", nw_inet_ntoa(arp_entry->ip_addr.in.s_addr), cp_arp_entry.is_lag, cp_arp_entry.output_index);

	return NW_API_OK;
}

/******************************************************************************
 * \brief       Modify ARP entry in NW DB
 *
 * \param[in]   arp_entry   - reference to ARP entry
 *
 * \return      NW_API_OK - ARP entry modified/added successfully
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc nw_api_modify_arp_entry(struct nw_api_arp_entry *arp_entry)
{
	struct nw_arp_result result;
	struct nw_arp_key key;
	struct nw_db_arp_entry cp_arp_entry;
	enum nw_api_rc rc;

	write_log(LOG_DEBUG, "Modify neighbor in ARP table. IP = %s", nw_inet_ntoa(arp_entry->ip_addr.in.s_addr));

	/* check if ARP entry exist in internal DB */
	rc = set_arp_cp_key(&cp_arp_entry, arp_entry);
	if (rc != NW_API_OK) {
		return rc;
	}
	switch (internal_db_get_entry(ARP_ENTRIES_INTERNAL_DB, (void *)&cp_arp_entry)) {
	case NW_DB_OK:
		/* ARP entry exists */
		write_log(LOG_DEBUG, "ARP entry (IP=%s) found on internal DB", nw_inet_ntoa(cp_arp_entry.entry_ip));
		break;
	case NW_DB_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find ARP entry (internal error).");
		return NW_API_DB_ERROR;
	case NW_DB_FAILURE:
		/* ARP entry doesn't exist in NW DB, Add entry instead. */
		write_log(LOG_DEBUG, "ARP Entry is not exist in DB, create a new entry (IP=%s)", nw_inet_ntoa(cp_arp_entry.entry_ip));
		return nw_api_add_arp_entry(arp_entry);
	default:
		return NW_API_DB_ERROR;
	}

	/* update dest_mac_addr */
	memcpy(cp_arp_entry.dest_mac_address.ether_addr_octet, arp_entry->mac_addr.ether_addr_octet, ETH_ALEN);

	/* build ARP key & result */
	build_nps_arp_key(&cp_arp_entry, &key);
	build_nps_arp_result(&cp_arp_entry, &result);

	/* modify ARP entry in internal DB */
	if (internal_db_modify_entry(ARP_ENTRIES_INTERNAL_DB, (void *)&cp_arp_entry) != NW_DB_OK) {
		write_log(LOG_NOTICE, "Error from internal DB while modifying ARP entry");
		return NW_API_DB_ERROR;
	}

	/* modify entry on DP table */
	if (infra_modify_entry(STRUCT_ID_NW_ARP, &key, sizeof(key), &result, sizeof(result)) == false) {
		write_log(LOG_NOTICE, "Error from NPS DB while modifying ARP entry");
		return NW_API_DB_ERROR;
	}

	write_log(LOG_DEBUG, "ARP entry Modified successfully. (IP = %s, is_lag = %d, output_if = %d)", nw_inet_ntoa(arp_entry->ip_addr.in.s_addr), cp_arp_entry.is_lag, cp_arp_entry.output_index);

	return NW_API_OK;
}


/**************************************************************************//**
 * \brief       create lag members array - returns a list of if logical IDs
 *
 * \param[in]   lag_group_id  - lag group id
 * \param[out]  members_count - reference to num of members in group
 * \param[out]  lag_members   - the members array
 *
 * \return      NW_API_OK - function succeed
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc get_lag_group_members_array(int lag_group_id, uint8_t *members_count, uint8_t *lag_members)
{
	int rc, member_index;
	sqlite3_stmt *statement;
	char sql[256];

	/* read from db the nw interfaces table and search which interface belong to this lag group */
	sprintf(sql, "SELECT * FROM nw_interfaces "
		"WHERE is_lag=1 AND lag_group_id=%d AND admin_state=1;",
		lag_group_id);

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
		/* copy interface logical id to the members array */
		lag_members[member_index] = sqlite3_column_int(statement, 4);
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
 * \brief    build the result and key for the DP table of the nw_interface
 *
 * \param[out]  if_result      - reference to the result struct.
 *  param[in]   interface_data - the values to store in the result struct
 *
 * \return   void
 */
void build_nps_nw_interface_result_and_key(struct nw_if_key *if_key, struct nw_if_result *if_result, struct nw_db_nw_interface *interface_data)
{
	memset(if_result, 0, sizeof(struct nw_if_result));
	memset(if_key, 0, sizeof(struct nw_if_key));
	if_key->logical_id = interface_data->logical_id;
	if_result->admin_state = interface_data->admin_state;
	if_result->app_bitmap = (struct nw_if_apps)interface_data->app_bitmap;
	if_result->direct_output_if = interface_data->direct_output_if;
	if_result->is_direct_output_lag = interface_data->is_direct_output_lag;
	memcpy(if_result->mac_address.ether_addr_octet, interface_data->mac_address.ether_addr_octet, ETH_ALEN);
	if_result->output_channel = interface_data->output_channel;
	if_result->path_type = (enum dp_path_type)interface_data->path_type;
	if_result->sft_en = interface_data->sft_en;
	if_result->stats_base = interface_data->stats_base;

}

/******************************************************************************
 * \brief    Enable LAG group and add to entry all IFs of LAG group
 *
 * \param[in]   lag_group_id   - lag group to update
 *
 * \return      NW_API_OK - DB updated successfully
 *              NW_API_FAILURE - group does not exist
 *              NW_API_DB_ERROR - database error
 */
enum nw_api_rc enable_lag_group(uint32_t lag_group_id)
{
	struct nw_lag_group_key lag_key;
	struct nw_lag_group_result lag_result;
	struct nw_db_lag_group_entry lag_group;
	enum nw_api_rc nw_api_rc;

	memset(&lag_key, 0, sizeof(lag_key));
	memset(&lag_result, 0, sizeof(lag_result));

	/* read lag group from internal db */
	lag_group.lag_group_id = lag_group_id;
	switch (internal_db_get_entry(LAG_GROUPS_INTERNAL_DB, (void *)&lag_group)) {
	case NW_DB_OK:
		break;
	case NW_DB_ERROR:
		write_log(LOG_NOTICE, "Failed to read lag group %d from internal DB. (internal error)", lag_group_id);
		return NW_API_DB_ERROR;
	case NW_DB_FAILURE:
		write_log(LOG_DEBUG, "Failed to read lag group %d from internal DB. Group does not exist", lag_group_id);
		return NW_API_FAILURE;
	}

	/* get array of IFs in LAG group */
	nw_api_rc = get_lag_group_members_array(lag_group_id, &lag_result.members_count, lag_result.lag_member);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_NOTICE, "error on internal db while creating members array");
		return nw_api_rc;
	}
	/* check that we didn't reached the max num of members */
	if (lag_result.members_count > LAG_GROUP_MAX_MEMBERS) {
		write_log(LOG_NOTICE, "Cannot add interfaces to lag group %d, too many members (%d)", lag_group_id, lag_result.members_count);
		return NW_API_FAILURE;
	}

	/* Set LAG key & admin state*/
	lag_key.lag_group_id = lag_group_id;
	lag_result.admin_state = true;

	/* Update NPS */
	if (infra_modify_entry(STRUCT_ID_NW_LAG_GROUPS, &lag_key, sizeof(lag_key), &lag_result, sizeof(lag_result)) == false) {
		write_log(LOG_CRIT, "Enable lag group %d failed", lag_group_id);
		return NW_API_DB_ERROR;
	}

	write_log(LOG_DEBUG, "Updated LAG group successfully. Group ID %d admin state %d member count %d IFs: %d, %d, %d, %d", lag_key.lag_group_id, lag_result.admin_state, lag_result.members_count, lag_result.lag_member[0], lag_result.lag_member[1], lag_result.lag_member[2], lag_result.lag_member[3]);

	return NW_API_OK;
}

/******************************************************************************
 * \brief    Disable LAG group and remove from entry all IFs of LAG group
 *
 * \param[in]   lag_group_id   - lag group to update
 *
 * \return      NW_API_OK - DB updated successfully
 *              NW_API_FAILURE - group does not exist
 *              NW_API_DB_ERROR - database error
 */
enum nw_api_rc disable_lag_group(uint32_t lag_group_id)
{
	struct nw_lag_group_key lag_key;
	struct nw_lag_group_result lag_result;
	struct nw_db_lag_group_entry lag_group;

	memset(&lag_key, 0, sizeof(lag_key));
	memset(&lag_result, 0, sizeof(lag_result));

	/* read lag group from internal db */
	lag_group.lag_group_id = lag_group_id;
	switch (internal_db_get_entry(LAG_GROUPS_INTERNAL_DB, (void *)&lag_group)) {
	case NW_DB_OK:
		break;
	case NW_DB_ERROR:
		write_log(LOG_NOTICE, "Failed to read lag group %d from internal DB. (internal error)", lag_group_id);
		return NW_API_DB_ERROR;
	case NW_DB_FAILURE:
		write_log(LOG_DEBUG, "Failed to read lag group %d from internal DB. Group does not exist", lag_group_id);
		return NW_API_FAILURE;
	}

	/* Set LAG key & admin state*/
	lag_key.lag_group_id = lag_group_id;
	lag_result.admin_state = false;
	/* Update NPS */
	if (infra_modify_entry(STRUCT_ID_NW_LAG_GROUPS, &lag_key, sizeof(lag_key), &lag_result, sizeof(lag_result)) == false) {
		write_log(LOG_CRIT, "Disable lag group %d failed", lag_group_id);
		return NW_API_DB_ERROR;
	}

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       return all interface entries from nw IF internal DB
 *
 * \param[in/out]   interface_entries   - reference to IF entries
 * \param[out]      entry_count         - reference to entry count
 *
 * \return      NW_API_OK - entries found successfully
 *              NW_API_FAILURE - IF entry does not exist or exceeds max entry count
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc get_all_interface_entries(struct nw_db_nw_interface *interface_entries, uint32_t *entry_count)
{
	int rc;
	char sql[512];
	sqlite3_stmt *statement;
	uint16_t app_bitmap_u16_casting = 0;
	uint64_t mac_address_casting = 0;

	sprintf(sql, "SELECT * FROM nw_interfaces "
		"WHERE interface_id=%d;",
		interface_entries[0].interface_id);

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

	/* Entry not found */
	if (rc == SQLITE_DONE) {
		sqlite3_finalize(statement);
		return NW_API_FAILURE;
	}

	while (rc == SQLITE_ROW) {
		if (*entry_count == LAG_GROUP_MAX_MEMBERS) {
			write_log(LOG_NOTICE, "Too many members in LAG group %d.", interface_entries[0].lag_group_id);
			sqlite3_finalize(statement);
			return NW_API_DB_ERROR;
		}
		/* retrieve NW IF entry from result */
		interface_entries[*entry_count].interface_id = sqlite3_column_int(statement, 0);
		interface_entries[*entry_count].admin_state = sqlite3_column_int(statement, 1);
		interface_entries[*entry_count].lag_group_id = sqlite3_column_int(statement, 2);
		interface_entries[*entry_count].is_lag = sqlite3_column_int(statement, 3);
		interface_entries[*entry_count].logical_id = sqlite3_column_int(statement, 4);
		interface_entries[*entry_count].path_type = sqlite3_column_int(statement, 5);
		interface_entries[*entry_count].is_direct_output_lag = sqlite3_column_int(statement, 6);
		interface_entries[*entry_count].direct_output_if = sqlite3_column_int(statement, 7);
		app_bitmap_u16_casting = sqlite3_column_int(statement, 8);
		memcpy(&interface_entries[*entry_count].app_bitmap, &app_bitmap_u16_casting, sizeof(uint16_t));
		interface_entries[*entry_count].output_channel = sqlite3_column_int(statement, 9);
		interface_entries[*entry_count].sft_en = sqlite3_column_int(statement, 10);
		interface_entries[*entry_count].stats_base = sqlite3_column_int(statement, 11);
		mac_address_casting = sqlite3_column_int64(statement, 12);
		memcpy(interface_entries[*entry_count].mac_address.ether_addr_octet, &mac_address_casting, ETH_ALEN);

		(*entry_count)++;
		rc = sqlite3_step(statement);
	}

	/* finalize SQL statement */
	sqlite3_finalize(statement);

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       Change IF params in DBs
 *
 * \param[in]   interface_data   - reference to IF entry in internal DB
 *
 * \return      NW_API_OK - IF entry modified successfully
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc update_if_params(struct nw_db_nw_interface *interface_data)
{
	struct nw_if_key if_key;
	struct nw_if_result if_result;

	/* Update entry in internal DB */
	if (internal_db_modify_entry(NW_INTERFACES_INTERNAL_DB, (void *)interface_data) != NW_DB_OK) {
		write_log(LOG_NOTICE, "Failed to modify IF %d params - received error from internal DB", interface_data->interface_id);
		return NW_API_DB_ERROR;
	}
	build_nps_nw_interface_result_and_key(&if_key, &if_result, interface_data);
	if (infra_modify_entry(STRUCT_ID_NW_INTERFACES, &if_key, sizeof(if_key), &if_result, sizeof(if_result)) == false) {
		write_log(LOG_NOTICE, "Failed to modify IF params - received error from NPS");
		return NW_API_DB_ERROR;
	}
	write_log(LOG_DEBUG, "Updated Interface successfully. Logical ID %d is lag %d LAG group %d admin state %d MAC %02X:%02X:%02X:%02X:%02X:%02X", if_key.logical_id, interface_data->is_lag, interface_data->lag_group_id, if_result.admin_state, if_result.mac_address.ether_addr_octet[0], if_result.mac_address.ether_addr_octet[1], if_result.mac_address.ether_addr_octet[2], if_result.mac_address.ether_addr_octet[3], if_result.mac_address.ether_addr_octet[4], if_result.mac_address.ether_addr_octet[5]);

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       Enable IF entry - lag mode
 *
 * \param[in]   if_entry   - reference to IF entry
 *
 * \return      NW_API_OK - IF entry enabled successfully
 *              NW_API_FAILURE - IF entry does not exist
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc enable_if_entry_lag_mode(struct nw_api_if_entry *if_entry)
{
	struct nw_db_nw_interface lag_interfaces[LAG_GROUP_MAX_MEMBERS];
	enum nw_api_rc nw_api_rc;
	uint32_t i, entry_count = 0;
	/* Find all related phyisical ports */
	lag_interfaces[0].interface_id = if_entry->if_index;
	switch (get_all_interface_entries(lag_interfaces, &entry_count)) {
	case NW_API_OK:
		break;
	case NW_API_DB_ERROR:
		write_log(LOG_ERR, "Failed to read interfaces from internal DB. (internal error)");
		return NW_API_DB_ERROR;
	case NW_API_FAILURE:
		if (entry_count == 0) {
			write_log(LOG_DEBUG, "Failed to read nw interface %d from internal DB. interface does not exist", if_entry->if_index);
			return NW_API_FAILURE;
		}
		break;
	}

	/* For each physical port update admin state in internal & NPS DBs*/
	for (i = 0; i < entry_count; i++) {
		/* check if this port is a part of lag group */
		if (lag_interfaces[i].is_lag == false) {
			write_log(LOG_NOTICE, "Interface %d not part of LAG group", lag_interfaces[i].interface_id);
			return NW_API_FAILURE;
		}
		/* change admin state value */
		lag_interfaces[i].admin_state = true;
		nw_api_rc = update_if_params(&lag_interfaces[i]);
		if (nw_api_rc != NW_API_OK) {
			return nw_api_rc;
		}
	}
	/* Update LAG group with all enabled ports */
	nw_api_rc = enable_lag_group(lag_interfaces[0].lag_group_id);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_ERR, "Error while adding port to lag group");
		return nw_api_rc;
	}

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       Disable IF entry - lag mode
 *
 * \param[in]   if_entry   - reference to IF entry
 *
 * \return      NW_API_OK - IF entry disabled successfully
 *              NW_API_FAILURE - IF entry does not exist
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc disable_if_entry_lag_mode(struct nw_api_if_entry *if_entry)
{
	struct nw_db_nw_interface lag_interfaces[LAG_GROUP_MAX_MEMBERS];
	enum nw_api_rc nw_api_rc;
	uint32_t i, entry_count = 0;
	/* Find all related phyisical ports */
	lag_interfaces[0].interface_id = if_entry->if_index;
	switch (get_all_interface_entries(lag_interfaces, &entry_count)) {
	case NW_API_OK:
		break;
	case NW_API_DB_ERROR:
		write_log(LOG_ERR, "Failed to read interfaces from internal DB. (internal error)");
		return NW_API_DB_ERROR;
	case NW_API_FAILURE:
		if (entry_count == 0) {
			write_log(LOG_DEBUG, "Failed to read nw interface %d from internal DB. interface does not exist", if_entry->if_index);
			return NW_API_FAILURE;
		}
		break;
	}

	/* Disable LAG group */
	nw_api_rc = disable_lag_group(lag_interfaces[0].lag_group_id);
	if (nw_api_rc != NW_API_OK) {
		write_log(LOG_ERR, "Error while enabling LAG group %d", lag_interfaces[0].lag_group_id);
		return nw_api_rc;
	}

	/* For each physical port update admin state in internal & NPS DBs*/
	for (i = 0; i < entry_count; i++) {
		/* check if this port is a part of lag group */
		if (lag_interfaces[i].is_lag == false) {
			write_log(LOG_NOTICE, "Interface %d not part of LAG group", lag_interfaces[i].interface_id);
			return NW_API_FAILURE;
		}
		/* change admin state value */
		lag_interfaces[i].admin_state = false;
		nw_api_rc = update_if_params(&lag_interfaces[i]);
		if (nw_api_rc != NW_API_OK) {
			return nw_api_rc;
		}
	}

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       Modify IF entry - lag mode
 *
 * \param[in]   if_entry   - reference to IF entry
 *
 * \return      NW_API_OK - IF entry modified successfully
 *              NW_API_FAILURE - IF entry does not exist
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc modify_if_entry_lag_mode(struct nw_api_if_entry *if_entry)
{
	struct nw_db_nw_interface lag_interfaces[LAG_GROUP_MAX_MEMBERS];
	enum nw_api_rc nw_api_rc;
	uint32_t i, entry_count = 0;
	/* Find all related physical ports */
	lag_interfaces[0].interface_id = if_entry->if_index;
	switch (get_all_interface_entries(lag_interfaces, &entry_count)) {
	case NW_API_OK:
		break;
	case NW_API_DB_ERROR:
		write_log(LOG_ERR, "Failed to read interfaces from internal DB. (internal error)");
		return NW_API_DB_ERROR;
	case NW_API_FAILURE:
		if (entry_count == 0) {
			write_log(LOG_DEBUG, "Failed to read nw interface %d from internal DB. interface does not exist", if_entry->if_index);
			return NW_API_FAILURE;
		}
		break;
	}

	/* For each physical port update params in internal & NPS DBs*/
	for (i = 0; i < entry_count; i++) {
		/* check if this port is a part of lag group */
		if (lag_interfaces[i].is_lag == false) {
			write_log(LOG_NOTICE, "Interface %d not part of LAG group", lag_interfaces[i].interface_id);
			return NW_API_FAILURE;
		}
		/* change IF params */
		memcpy(lag_interfaces[i].mac_address.ether_addr_octet, if_entry->mac_addr.ether_addr_octet, ETH_ALEN);
		nw_api_rc = update_if_params(&lag_interfaces[i]);
		if (nw_api_rc != NW_API_OK) {
			return nw_api_rc;
		}
	}

	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       Enable IF entry
 *
 * \param[in]   if_entry   - reference to IF entry
 *
 * \return      NW_API_OK - IF entry enabled successfully
 *              NW_API_FAILURE - IF entry does not exist or already enabled
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc nw_api_enable_if_entry(struct nw_api_if_entry *if_entry)
{
	struct nw_db_nw_interface interface_data;
	enum nw_api_rc nw_api_rc;

	/* read interface id from internal db */
	interface_data.interface_id = if_entry->if_index;
	switch (internal_db_get_entry(NW_INTERFACES_INTERNAL_DB, (void *)&interface_data)) {
	case NW_DB_OK:
		break;
	case NW_DB_ERROR:
		write_log(LOG_ERR, "Failed to read interface %d from internal DB. (internal error)", interface_data.interface_id);
		return NW_API_DB_ERROR;
	case NW_DB_FAILURE:
		write_log(LOG_DEBUG, "Failed to read nw interface %d from internal DB. interface does not exist", interface_data.interface_id);
		return NW_API_FAILURE;
	}

	if (interface_data.admin_state == true) {
		/* Already enabled. Nothing to do. */
		return NW_API_FAILURE;
	}

	/* check if we are running in lag mode */
	if (system_cfg_is_lag_en() == true) {
		nw_api_rc = enable_if_entry_lag_mode(if_entry);
		if (nw_api_rc != NW_API_OK) {
			write_log(LOG_ERR, "Error while enabling port in lag group");
			return nw_api_rc;
		}
	} else {
		/* this port is independent */
		/* change admin state value */
		interface_data.admin_state = true;
		nw_api_rc = update_if_params(&interface_data);
		if (nw_api_rc != NW_API_OK) {
			write_log(LOG_ERR, "Error while enabling port.");
			return nw_api_rc;
		}
	}
	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       Disable IF entry
 *
 * \param[in]   if_entry   - reference to IF entry
 *
 * \return      NW_API_OK - IF entry disabled successfully
 *              NW_API_FAILURE - IF entry does not exist
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc nw_api_disable_if_entry(struct nw_api_if_entry *if_entry)
{
	struct nw_db_nw_interface interface_data;
	enum nw_api_rc nw_api_rc;

	/* read interface id from internal db */
	interface_data.interface_id = if_entry->if_index;
	switch (internal_db_get_entry(NW_INTERFACES_INTERNAL_DB, (void *)&interface_data)) {
	case NW_DB_OK:
		break;
	case NW_DB_ERROR:
		write_log(LOG_ERR, "Failed to read interface %d from internal DB. (internal error)", interface_data.interface_id);
		return NW_API_DB_ERROR;
	case NW_DB_FAILURE:
		write_log(LOG_DEBUG, "Failed to read nw interface %d from internal DB. interface does not exist", interface_data.interface_id);
		return NW_API_FAILURE;
	}

	if (interface_data.admin_state == false) {
		/* Already disabled. Nothing to do. */
		return NW_API_FAILURE;
	}

	/* check if we are running in lag mode */
	if (system_cfg_is_lag_en() == true) {
		nw_api_rc = disable_if_entry_lag_mode(if_entry);
		if (nw_api_rc != NW_API_OK) {
			write_log(LOG_ERR, "Error while disabling port in lag group");
			return nw_api_rc;
		}
	} else {
		/* This port is independent */
		/* change admin state value */
		interface_data.admin_state = false;
		nw_api_rc = update_if_params(&interface_data);
		if (nw_api_rc != NW_API_OK) {
			write_log(LOG_ERR, "Error while disabling port.");
			return nw_api_rc;
		}
	}
	return NW_API_OK;
}

/**************************************************************************//**
 * \brief       Modify IF entry - For disabled IF only.
 *
 * \param[in]   if_entry   - reference to IF entry
 *
 * \return      NW_API_OK - IF entry Modified successfully
 *              NW_API_FAILURE - IF entry does not exist
 *              NW_API_DB_ERROR - failed to communicate with DB
 */
enum nw_api_rc nw_api_modify_if_entry(struct nw_api_if_entry *if_entry)
{
	struct nw_db_nw_interface interface_data;
	enum nw_api_rc nw_api_rc;

	/* read interface id from internal db */
	interface_data.interface_id = if_entry->if_index;
	switch (internal_db_get_entry(NW_INTERFACES_INTERNAL_DB, (void *)&interface_data)) {
	case NW_DB_OK:
		break;
	case NW_DB_ERROR:
		write_log(LOG_ERR, "Failed to read interface %d from internal DB. (internal error)", interface_data.interface_id);
		return NW_API_DB_ERROR;
	case NW_DB_FAILURE:
		write_log(LOG_DEBUG, "Failed to read nw interface %d from internal DB. interface does not exist", interface_data.interface_id);
		return NW_API_FAILURE;
	}
	/* check if we are running in lag mode */
	if (system_cfg_is_lag_en() == true) {
		nw_api_rc = modify_if_entry_lag_mode(if_entry);
		if (nw_api_rc != NW_API_OK) {
			write_log(LOG_ERR, "Error while modifying port in lag group");
			return nw_api_rc;
		}
	} else {
		/* This port is independent */
		/* change IF params */
		memcpy(interface_data.mac_address.ether_addr_octet, if_entry->mac_addr.ether_addr_octet, ETH_ALEN);
		nw_api_rc = update_if_params(&interface_data);
		if (nw_api_rc != NW_API_OK) {
			write_log(LOG_ERR, "Error while modifying port.");
			return nw_api_rc;
		}
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
		if (internal_db_remove_entry(ARP_ENTRIES_INTERNAL_DB, (void *)&arp_entry) != NW_DB_OK) {
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
	if (internal_db_modify_entry(NW_INTERFACES_INTERNAL_DB, (void *)&interface_data) != NW_DB_OK) {
		write_log(LOG_NOTICE, "modify interface id %d on internal DB failed. (internal error)", interface_id);
		return NW_API_DB_ERROR;
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
	if (internal_db_remove_entry(LAG_GROUPS_INTERNAL_DB, (void *)&lag_group) != NW_DB_OK) {
		write_log(LOG_ERR, "remove lag group entry was failed, failure on db or lag group id is not exist");
		return NW_API_DB_ERROR;
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
