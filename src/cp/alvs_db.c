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
*  File:                infrastructure.c
*  Desc:                Implementation of infrastructure API.
*
*/

#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <byteswap.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include "log.h"

#include <EZapiStat.h>
#include <EZapiPrm.h>
#include "alvs_db.h"
#include "sqlite3.h"
#include "defs.h"
#include "infrastructure.h"
#include "application_search_defs.h"
#include "index_pool.h"


/* Global pointer to the DB */
sqlite3 *alvs_db;
struct index_pool server_index_pool;
struct index_pool service_index_pool;
pthread_t server_db_aging_thread;
bool *alvs_db_cancel_application_flag_ptr;

extern const char *alvs_error_stats_offsets_names[];

void server_db_aging(void);

#define ALVS_DB_FILE_NAME "alvs.db"

void server_db_exit_with_error(void);

struct alvs_db_service_stats {
	uint64_t connection_scheduled;
	uint64_t in_packet;
	uint64_t in_byte;
	uint64_t out_packet;
	uint64_t out_byte;
};

struct alvs_db_service {
	in_addr_t ip;
	uint16_t port;
	uint16_t protocol;
	uint32_t nps_index;
	uint32_t flags;
	enum alvs_scheduler_type sched_alg;
	struct ezdp_sum_addr stats_base;
	uint16_t sched_entries_count;
	/* used this statistics when we want to display stats, on reset we save those stats from original counters */
	struct alvs_db_service_stats service_stats;
};

struct alvs_db_server_stats {
	uint64_t connection_scheduled;
	uint64_t in_packet;
	uint64_t in_byte;
	uint64_t out_packet;
	uint64_t out_byte;
	uint64_t active_connection;
	uint64_t inactive_connection;
	uint64_t connection_total;
};

struct alvs_db_server {
	in_addr_t ip;
	uint16_t port;
	uint16_t weight;
	uint8_t active;
	uint32_t nps_index;
	uint32_t conn_flags;
	uint32_t server_flags;
	uint32_t u_thresh;
	uint32_t l_thresh;
	struct ezdp_sum_addr server_stats_base;
	struct ezdp_sum_addr service_stats_base;
	struct ezdp_sum_addr server_on_demand_stats_base;
	struct ezdp_sum_addr server_flags_dp_base;
	/* used this statistics when we want to display stats, on reset we save those stats from original counters */
	struct alvs_db_server_stats server_stats;
};

struct alvs_db_application_info {
	uint8_t		application_index;
	bool		is_master;
	bool		is_backup;
	uint8_t		m_sync_id;
	uint8_t		b_sync_id;
	in_addr_t	source_ip;
};

struct alvs_server_node {
	struct alvs_db_server server;
	struct alvs_server_node *next;
};

char *my_inet_ntoa(in_addr_t ip)
{

	struct in_addr ip_addr = {.s_addr = bswap_32(ip)};

	return inet_ntoa(ip_addr);
}

#define TABLE_ENTRY_IP				0
#define TABLE_ENTRY_PORT			1
#define TABLE_ENTRY_PRTOTOCOL			2
#define TABLE_ENTRY_NPS_INDEX			3
#define TABLE_ENTRY_NPS_FLAGS			4
#define TABLE_ENTRY_SCHED_ALG			5
#define TABLE_ENTRY_CONNECTION_SCHEDULD		6
#define TABLE_ENTRY_STATS_BASE			7
#define TABLE_ENTRY_IN_PACKET			8
#define TABLE_ENTRY_IN_BYTE			9
#define TABLE_ENTRY_OUT_PACKET			10
#define TABLE_ENTRY_OUT_BYTE			11
#define TABLE_ENTRY_SCHED_ENTRIES_COUNT		12

enum alvs_db_rc alvs_db_init(bool *cancel_application_flag)
{
	int rc;
	char *sql;
	char *zErrMsg = NULL;

	if (index_pool_init(&server_index_pool, 32768) == false) {
		write_log(LOG_CRIT, "Failed to init server index pool.");
		return ALVS_DB_INTERNAL_ERROR;
	}
	if (index_pool_init(&service_index_pool, 256) == false) {
		index_pool_destroy(&server_index_pool);
		write_log(LOG_CRIT, "Failed to init service index pool.");
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Delete existing DB file */
	(void)remove(ALVS_DB_FILE_NAME);

	/* Open the DB file */
	rc = sqlite3_open(ALVS_DB_FILE_NAME, &alvs_db);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "Can't open database: %s",
			  sqlite3_errmsg(alvs_db));
		index_pool_destroy(&server_index_pool);
		index_pool_destroy(&service_index_pool);
		return ALVS_DB_INTERNAL_ERROR;
	}

	write_log(LOG_DEBUG, "ALVS_DB: Opened database successfully");

	/* Create the services table:
	 * Fields:
	 *    ip address
	 *    port
	 *    protocol
	 *    nps_index
	 *    flags
	 *    scheduling algorithm
	 *    statistics base
	 */
	sql = "CREATE TABLE services("
		"ip INT NOT NULL,"			/* TABLE_ENTRY_IP */
		"port INT NOT NULL,"			/* TABLE_ENTRY_PORT */
		"protocol INT NOT NULL,"		/* TABLE_ENTRY_PRTOTOCOL */
		"nps_index INT NOT NULL,"		/* TABLE_ENTRY_NPS_INDEX */
		"flags INT NOT NULL,"			/* TABLE_ENTRY_NPS_FLAGS */
		"sched_alg INT NOT NULL,"		/* TABLE_ENTRY_SCHED_ALG */
		"connection_scheduled BIGINT NOT NULL,"	/* TABLE_ENTRY_CONNECTION_SCHEDULD */
		"stats_base INT NOT NULL,"		/* TABLE_ENTRY_STATS_BASE */
		"in_packet BIGINT NOT NULL,"		/* TABLE_ENTRY_IN_PACKET */
		"in_byte BIGINT NOT NULL,"		/* TABLE_ENTRY_IN_BYTE */
		"out_packet BIGINT NOT NULL,"		/* TABLE_ENTRY_OUT_PACKET */
		"out_byte BIGINT NOT NULL,"		/* TABLE_ENTRY_OUT_BYTE */
		"sched_entries_count INT NOT NULL,"	/* TABLE_ENTRY_SCHED_ENTRIES_COUNT */
		"PRIMARY KEY (ip,port,protocol));";

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		index_pool_destroy(&server_index_pool);
		index_pool_destroy(&service_index_pool);
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Create the servers table:
	 * Fields:
	 *    ip address
	 *    port
	 *    service ip
	 *    service port
	 *    service protocol
	 *    nps_index
	 *    weight
	 *    flags
	 *    active (boolean)
	 *    statistics base
	 */
	sql = "CREATE TABLE servers("
		"ip INT NOT NULL,"				/* 0 */
		"port INT NOT NULL,"				/* 1 */
		"srv_ip INT NOT NULL,"				/* 2 */
		"srv_port INT NOT NULL,"			/* 3 */
		"srv_protocol INT NOT NULL,"			/* 4 */
		"nps_index INT NOT NULL,"			/* 5 */
		"weight INT NOT NULL,"				/* 6 */
		"conn_flags INT NOT NULL,"			/* 7 */
		"server_flags INT NOT NULL,"			/* 8 */
		"u_thresh INT NOT NULL,"			/* 9 */
		"l_thresh INT NOT NULL,"			/* 10 */
		"active BOOLEAN NOT NULL,"			/* 11 */
		"server_stats_base INT NOT NULL,"		/* 12 */
		"service_stats_base INT NOT NULL,"		/* 13 */
		"server_on_demand_stats_base INT NOT NULL,"	/* 14 */
		"server_flags_dp_base INT NOT NULL,"		/* 15 */
		"connection_scheduled BIGINT NOT NULL,"		/* 16 */
		"in_packet BIGINT NOT NULL,"			/* 17 */
		"in_byte BIGINT NOT NULL,"			/* 18 */
		"out_packet BIGINT NOT NULL,"			/* 19 */
		"out_byte BIGINT NOT NULL,"			/* 20 */
		"PRIMARY KEY (ip,port,srv_ip,srv_port,srv_protocol),"
		"FOREIGN KEY (srv_ip,srv_port,srv_protocol) "
		"REFERENCES services(ip,port,protocol));";

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		index_pool_destroy(&server_index_pool);
		index_pool_destroy(&service_index_pool);
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Create the application_info table:
	 * Fields:
	 *    application_index
	 *    is_master
	 *    is_backup
	 *    m_sync_id
	 *    b_sync_id
	 */
	sql = "CREATE TABLE application_info("
		"application_index INT NOT NULL,"
		"is_master INT NOT NULL,"
		"is_backup INT NOT NULL,"
		"m_sync_id INT NOT NULL,"
		"b_sync_id INT NOT NULL,"
		"source_ip INT NOT NULL,"
		"PRIMARY KEY (application_index));";

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		index_pool_destroy(&server_index_pool);
		index_pool_destroy(&service_index_pool);
		return ALVS_DB_INTERNAL_ERROR;
	}

	alvs_db_cancel_application_flag_ptr = cancel_application_flag;

	/* open aging thread */
	rc = pthread_create(&server_db_aging_thread, NULL,
			    (void * (*)(void *))server_db_aging, NULL);
	if (rc != 0) {
		write_log(LOG_CRIT, "Cannot start server_db_aging_thread: pthread_create failed for server DB aging.");
		return ALVS_DB_FATAL_ERROR;
	}

	return ALVS_DB_OK;
}

void alvs_db_destroy(void)
{
	pthread_join(server_db_aging_thread, NULL);

	/* Close the DB file */
	sqlite3_close(alvs_db);

	/* Destroy stacks */
	index_pool_destroy(&service_index_pool);
	index_pool_destroy(&server_index_pool);
}

#define EXCLUDE_WEIGHT_ZERO 0x1
#define EXCLUDE_INACTIVE    0x2

/**************************************************************************//**
 * \brief       Get number of active servers assigned to a service
 *
 * \param[in]   service          - reference to the service
 * \param[out]  server_count     - number of active servers
 * \param[in]   flags            - any combination of the following:
 *                                    EXCLUDE_WEIGHT_ZERO
 *                                    EXCLUDE_INACTIVE
 *
 * \return      ALVS_DB_OK - service exists (info is filled with service
 *                           information if it not NULL).
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum alvs_db_rc internal_db_get_server_count(struct alvs_db_service *service,
					     uint32_t *server_count,
					     uint32_t flags)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[256];

	sprintf(sql, "SELECT COUNT (nps_index) AS server_count FROM servers "
		"WHERE srv_ip=%d AND srv_port=%d AND srv_protocol=%d",
		service->ip, service->port, service->protocol);
	if (flags & EXCLUDE_WEIGHT_ZERO) {
		strcat(sql, " AND weight>0");
	}
	if (flags & EXCLUDE_INACTIVE) {
		strcat(sql, " AND active=1");
	}
	strcat(sql, ";");

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(alvs_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(alvs_db));
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* In case of error return 0 */
	if (rc != SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(alvs_db));
		sqlite3_finalize(statement);
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* retrieve count from result,
	 * finalize SQL statement and return count
	 */
	*server_count = sqlite3_column_int(statement, 0);
	sqlite3_finalize(statement);
	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Update server_flags (OVERLOADED bit) according to the predefined algorithm
 *
 * \param[in]   server   - server received from CP.
 *
 * \return      true if operation succeeded
 */
enum alvs_db_rc alvs_clear_overloaded_flag_of_server(struct alvs_db_server *cp_server)
{

	uint32_t server_flags = 0;
	EZstatus ret_val;

	write_log(LOG_DEBUG, "raw = 0x%x ext = %d msid = %d, lsb_addr = 0x%x",
		  cp_server->server_flags_dp_base.raw_data,
		  (cp_server->server_flags_dp_base.raw_data & EZDP_SUM_ADDR_MEM_TYPE_MASK) >> EZDP_SUM_ADDR_MEM_TYPE_OFFSET,
		  (cp_server->server_flags_dp_base.raw_data & ~EZDP_SUM_ADDR_MEM_TYPE_MASK) >> EZDP_SUM_ADDR_MSID_OFFSET,
		  (uint32_t)(EMEM_SERVER_FLAGS_OFFSET_CP + cp_server->nps_index * sizeof(server_flags)));

	write_log(LOG_DEBUG, "from_msid %d to index %d", (cp_server->server_flags_dp_base.raw_data & ~EZDP_SUM_ADDR_MEM_TYPE_MASK) >> EZDP_SUM_ADDR_MSID_OFFSET,
		  infra_from_msid_to_index(1, (cp_server->server_flags_dp_base.raw_data & ~EZDP_SUM_ADDR_MEM_TYPE_MASK) >> EZDP_SUM_ADDR_MSID_OFFSET));

	ret_val =  EZapiPrm_WriteMem(0, /*uiChannelId*/
				   (((cp_server->server_flags_dp_base.raw_data & EZDP_SUM_ADDR_MEM_TYPE_MASK) >> EZDP_SUM_ADDR_MEM_TYPE_OFFSET) == EZDP_EXTERNAL_MS) ? EZapiPrm_MemId_EXT_MEM : EZapiPrm_MemId_INT_MEM, /*eMemId*/
				    infra_from_msid_to_index(1, (cp_server->server_flags_dp_base.raw_data & ~EZDP_SUM_ADDR_MEM_TYPE_MASK) >> EZDP_SUM_ADDR_MSID_OFFSET),
				    EMEM_SERVER_FLAGS_OFFSET_CP + cp_server->nps_index * sizeof(server_flags),
				    0, /* uiMSBAddress */
				    0, /* bRange */
				    0, /* uiRangeSize */
				    0, /* uiRangeStep */
				    0, /* bSingleCopy */
				    0, /* bGCICopy */
				    0, /* uiCopyIndex */
				    sizeof(server_flags),
				    (EZuc8 *)&server_flags,   /*pucData*/
				    0                /* pSpecialParams */);

	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "alvs_db_clean_alloc_memory: EZapiPrm_WriteMem failed.");
		return ALVS_DB_INTERNAL_ERROR;
	}

	return ALVS_DB_OK;

}

/**************************************************************************//**
 * \brief       Delete all services, set all servers to inactive.
 *
 * \return      ALVS_DB_OK - all was cleared
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum alvs_db_rc internal_db_clear_all(void)
{
	int rc;
	char sql[256];
	char *zErrMsg = NULL;

	sprintf(sql, "DELETE FROM services;"
		"UPDATE servers SET active=0, server_flags=server_flags&%u;",
		~IP_VS_DEST_F_AVAILABLE);

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return ALVS_DB_INTERNAL_ERROR;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Get service information (address, port, protocol)
 *
 * \param[in/out]   service   - reference to service
 * \param[in]  full_service   - when false - check only if service exists.
 *
 * \return      ALVS_DB_OK - service exists (info is filled with service
 *                           information if it not NULL).
 *              ALVS_DB_FAILURE - service doesn't exist.
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum alvs_db_rc internal_db_get_service(struct alvs_db_service *service,
					bool full_service)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[256];

	sprintf(sql, "SELECT * FROM services "
		"WHERE ip=%d AND port=%d AND protocol=%d;",
		service->ip, service->port, service->protocol);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(alvs_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(alvs_db));
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(alvs_db));
		sqlite3_finalize(statement);
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Service not found */
	if (rc == SQLITE_DONE) {
		write_log(LOG_DEBUG,
			  "No service found with required 3-tuple.");
		sqlite3_finalize(statement);
		return ALVS_DB_FAILURE;
	}
	if (full_service) {
		/* retrieve service info from result */
		service->nps_index = sqlite3_column_int(statement, TABLE_ENTRY_NPS_INDEX);
		service->flags = sqlite3_column_int(statement, TABLE_ENTRY_NPS_FLAGS);
		service->sched_alg = (enum alvs_scheduler_type)sqlite3_column_int(statement, TABLE_ENTRY_SCHED_ALG);
		service->service_stats.connection_scheduled = sqlite3_column_int64(statement, TABLE_ENTRY_CONNECTION_SCHEDULD);
		service->stats_base.raw_data = sqlite3_column_int(statement, TABLE_ENTRY_STATS_BASE);
		service->service_stats.in_packet = sqlite3_column_int64(statement, TABLE_ENTRY_IN_PACKET);
		service->service_stats.in_byte = sqlite3_column_int64(statement, TABLE_ENTRY_IN_BYTE);
		service->service_stats.out_packet = sqlite3_column_int64(statement, TABLE_ENTRY_OUT_PACKET);
		service->service_stats.out_byte = sqlite3_column_int64(statement, TABLE_ENTRY_OUT_BYTE);
		service->sched_entries_count = sqlite3_column_int(statement, TABLE_ENTRY_SCHED_ENTRIES_COUNT);
	}

	/* finalize SQL statement */
	sqlite3_finalize(statement);

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Add a service to internal DB
 *
 * \param[in]   service   - reference to service
 * \param[in]   info      - reference to service information
 *
 * \return      ALVS_DB_OK - service exists (info is filled with service
 *                           information if it not NULL).
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum alvs_db_rc internal_db_add_service(struct alvs_db_service *service)
{
	int rc;
	char sql[256];
	char *zErrMsg = NULL;

	sprintf(sql, "INSERT INTO services "
		"(ip, port, protocol, nps_index, flags, sched_alg, connection_scheduled, stats_base, "
		"in_packet, in_byte, out_packet, out_byte, sched_entries_count) "
		"VALUES (%d, %d, %d, %d, %d, %d, %ld, %d, %ld, %ld, %ld, %ld, %d);",
		service->ip, service->port, service->protocol,
		service->nps_index, service->flags, service->sched_alg,
		service->service_stats.connection_scheduled, service->stats_base.raw_data,
		service->service_stats.in_packet, service->service_stats.in_byte, service->service_stats.out_packet,
		service->service_stats.out_byte, service->sched_entries_count);

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return ALVS_DB_INTERNAL_ERROR;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Modify a service in internal DB
 *
 * \param[in]   service   - reference to service
 * \param[in]   info      - reference to service information
 *
 * \return      ALVS_DB_OK - service exists (info is filled with service
 *                           information if it not NULL).
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum alvs_db_rc internal_db_modify_service(struct alvs_db_service *service)
{
	int rc;
	char sql[256];
	char *zErrMsg = NULL;

	sprintf(sql, "UPDATE services "
		"SET flags=%d, sched_alg=%d, sched_entries_count=%d "
		"WHERE ip=%d AND port=%d AND protocol=%d;",
		service->flags, service->sched_alg, service->sched_entries_count,
		service->ip, service->port, service->protocol);

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return ALVS_DB_INTERNAL_ERROR;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Get Service Counters (Posted Counters)
 *
 * \param[in]   server_index   - index of server
 *              [out] struct alvs_db_service_stats *service_stats
 *
 * \return      alvs_db_rc
 */
enum alvs_db_rc alvs_db_get_service_counters(uint32_t service_index, struct alvs_db_service_stats *service_stats)
{

	uint64_t service_counters[ALVS_NUM_OF_SERVICE_STATS] = {0};

	if (infra_get_posted_counters(EMEM_SERVICE_STATS_POSTED_OFFSET + (service_index * ALVS_NUM_OF_SERVICE_STATS),
				      ALVS_NUM_OF_SERVICE_STATS,
				      service_counters) == false) {
		write_log(LOG_CRIT, "Failed to read error statistics counters");
		return ALVS_DB_NPS_ERROR;
	}

	service_stats->in_packet = service_counters[ALVS_SERVICE_STATS_IN_PKTS_BYTES_OFFSET];
	service_stats->in_byte = service_counters[ALVS_SERVICE_STATS_IN_PKTS_BYTES_OFFSET + 1];
	service_stats->out_packet = service_counters[ALVS_SERVICE_STATS_OUT_PKTS_BYTES_OFFSET];
	service_stats->out_byte = service_counters[ALVS_SERVICE_STATS_OUT_PKTS_BYTES_OFFSET + 1];
	service_stats->connection_scheduled = service_counters[ALVS_SERVICE_STATS_CONN_SCHED_OFFSET];

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       read service posted counters and save counters on internal DB
 *
 * \param[in]   service   - reference to service
 *
 * \return      ALVS_DB_OK - succeed to save counters on internal DB).
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum alvs_db_rc internal_db_save_service_stats(struct alvs_db_service service)
{
	int rc;
	char sql[256];
	char *zErrMsg = NULL;
	enum alvs_db_rc get_counters_rc;

	/* read posted counter */
	get_counters_rc = alvs_db_get_service_counters(service.nps_index, &service.service_stats);
	if (get_counters_rc != ALVS_DB_OK) {
		write_log(LOG_CRIT, "Error reading service posted counters");
		return get_counters_rc;
	}

	sprintf(sql, "UPDATE services "
		"SET in_packet=%ld, in_byte=%ld, out_packet=%ld, out_byte=%ld, connection_scheduled=%ld "
		"WHERE ip=%d AND port=%d AND protocol=%d;",
		service.service_stats.in_packet, service.service_stats.in_byte, service.service_stats.out_packet,
		service.service_stats.out_byte, service.service_stats.connection_scheduled, service.ip,
		service.port, service.protocol);

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return ALVS_DB_INTERNAL_ERROR;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Delete a service from internal DB
 *
 * \param[in]   service   - reference to service
 *
 * \return      ALVS_DB_OK - service exists (info is filled with service
 *                           information if it not NULL).
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum alvs_db_rc internal_db_delete_service(struct alvs_db_service *service)
{
	int rc;
	char sql[256];
	char *zErrMsg = NULL;

	sprintf(sql, "DELETE FROM services "
		"WHERE ip=%d AND port=%d AND protocol=%d;"
		"UPDATE servers SET active=0, server_flags=server_flags&%u "
		"WHERE srv_ip=%d AND srv_port=%d AND srv_protocol=%d;",
		service->ip, service->port, service->protocol,
		~IP_VS_DEST_F_AVAILABLE,
		service->ip, service->port, service->protocol);

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return ALVS_DB_INTERNAL_ERROR;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Free server cyclic list created by get_server_list().
 *
 * \param[in]   list   - reference to cyclist list
 *
 * \return      void
 */
void alvs_free_server_list(struct alvs_server_node *list)
{
	struct alvs_server_node *node, *temp;

	if (list == NULL) {
		return;
	}

	node = list;
	do {
		temp = node;
		node = node->next;
		free(temp);
	} while (node != list);
}

/**************************************************************************//**
 * \brief       Get All active servers assigned to a service
 *
 * \param[in]   service          - reference to the service
 * \param[out]  server_info      - reference to head Cyclic list of the servers
 * \param[in]   flags            - any combination of the following:
 *                                    EXCLUDE_WEIGHT_ZERO
 *                                    EXCLUDE_INACTIVE
 *
 * \return      ALVS_DB_OK - operation succeeded. In case of no servers
 *                           server_info is updated to NULL.
 *                           The returned list must be freed using
 *                           the free_server_list function.
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum alvs_db_rc internal_db_get_server_list(struct alvs_db_service *service,
					    struct alvs_server_node **server_list,
					    uint32_t flags)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[256];
	struct alvs_server_node *node;

	sprintf(sql, "SELECT * FROM servers "
		"WHERE srv_ip=%d AND srv_port=%d AND srv_protocol=%d",
		service->ip, service->port, service->protocol);
	if (flags & EXCLUDE_WEIGHT_ZERO) {
		strcat(sql, " AND weight>0");
	}
	if (flags & EXCLUDE_INACTIVE) {
		strcat(sql, " AND active=1");
	}
	strcat(sql, ";");
	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(alvs_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(alvs_db));
		return ALVS_DB_INTERNAL_ERROR;
	}
	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	*server_list = NULL;

	/* Collect server ids */
	while (rc == SQLITE_ROW) {
		node = (struct alvs_server_node *)malloc(sizeof(struct alvs_server_node));
		node->server.ip = sqlite3_column_int (statement, 0);
		node->server.port = sqlite3_column_int (statement, 1);
		node->server.nps_index = sqlite3_column_int (statement, 5);
		node->server.weight = sqlite3_column_int (statement, 6);
		node->server.conn_flags = sqlite3_column_int (statement, 7);
		node->server.server_flags = sqlite3_column_int (statement, 8);
		node->server.u_thresh = sqlite3_column_int (statement, 9);
		node->server.l_thresh = sqlite3_column_int (statement, 10);
		node->server.active = sqlite3_column_int (statement, 11);
		node->server.server_stats_base.raw_data = sqlite3_column_int (statement, 12);
		node->server.service_stats_base.raw_data = sqlite3_column_int(statement, 13);
		node->server.server_on_demand_stats_base.raw_data = sqlite3_column_int(statement, 14);
		node->server.server_flags_dp_base.raw_data = sqlite3_column_int(statement, 15);
		node->server.server_stats.connection_scheduled = sqlite3_column_int64(statement, 16);
		node->server.server_stats.in_packet = sqlite3_column_int64(statement, 17);
		node->server.server_stats.in_byte = sqlite3_column_int64(statement, 18);
		node->server.server_stats.out_packet = sqlite3_column_int64(statement, 19);
		node->server.server_stats.out_byte = sqlite3_column_int64(statement, 20);

		if (*server_list == NULL) {
			node->next = node;
			*server_list = node;
		} else {
			node->next = (*server_list)->next;
			(*server_list)->next = node;
			(*server_list) = node;
		}

		rc = sqlite3_step(statement);
	}
	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(alvs_db));
		sqlite3_finalize(statement);
		alvs_free_server_list(*server_list);
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* finalize SQL statement */
	sqlite3_finalize(statement);

	if (*server_list != NULL) {
		(*server_list) = (*server_list)->next;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Get number of active connections from a statistics counter
 *
 * \param       [in]server_index   - index of server
 *		[out] server_stats - reference to server statistics
 *		[in] read_connection_total - indicate the function to also connection total field also (long counter)
 *
 * \return	ALVS_DB_OK - function succeed
 *		ALVS_DB_NPS_ERROR - fail to read statistics
 */
enum alvs_db_rc alvs_db_get_server_counters(uint32_t server_index,
					    struct alvs_db_server_stats *server_stats,
					    bool read_connection_total)
{

	uint64_t server_counter[ALVS_NUM_OF_SERVER_STATS] = {0};

	if (infra_get_posted_counters(EMEM_SERVER_STATS_POSTED_OFFSET + (server_index * ALVS_NUM_OF_SERVER_STATS),
				      ALVS_NUM_OF_SERVER_STATS,
				      server_counter) == false) {
		write_log(LOG_CRIT, "Failed to read error statistics counters");
		return ALVS_DB_NPS_ERROR;
	}

	server_stats->in_packet = server_counter[ALVS_SERVER_STATS_IN_PKTS_BYTES_OFFSET];
	server_stats->in_byte = server_counter[ALVS_SERVER_STATS_IN_PKTS_BYTES_OFFSET + 1];
	server_stats->out_packet = server_counter[ALVS_SERVER_STATS_OUT_PKTS_BYTES_OFFSET];
	server_stats->out_byte = server_counter[ALVS_SERVER_STATS_OUT_PKTS_BYTES_OFFSET + 1];
	server_stats->connection_scheduled = server_counter[ALVS_SERVER_STATS_CONN_SCHED_OFFSET];
	server_stats->active_connection = server_counter[ALVS_SERVER_STATS_ACTIVE_CONN_OFFSET];
	server_stats->inactive_connection = server_counter[ALVS_SERVER_STATS_INACTIVE_CONN_OFFSET];

	if (read_connection_total == true) {
		if (infra_get_long_counters(EMEM_SERVER_STATS_ON_DEMAND_OFFSET + server_index * ALVS_NUM_OF_SERVERS_ON_DEMAND_STATS,
					    1,
					    &server_stats->connection_total) == false) {
			write_log(LOG_ERR, "Fail to read server total connections statistics");
			return ALVS_DB_NPS_ERROR;
		}
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       read server posted counters and save counters on internal DB
 *
 * \param[in]   server   - reference to server
 *
 * \return      ALVS_DB_OK - succeed to save counters on internal DB).
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with DB
 *              ALVS_DB_NPS_ERROR - failed to read counters
 */
enum alvs_db_rc internal_db_save_server_stats(struct alvs_db_server *server)
{
	int rc;
	char sql[256];
	char *zErrMsg = NULL;
	enum alvs_db_rc get_counters_rc;

	/* read posted counter */
	get_counters_rc = alvs_db_get_server_counters(server->nps_index,
						      &server->server_stats,
						      false);
	if (get_counters_rc != ALVS_DB_OK) {
		write_log(LOG_CRIT, "Error reading service posted counters");
		return get_counters_rc;
	}

	sprintf(sql, "UPDATE servers "
		"SET connection_scheduled=%ld, in_packet=%ld, in_byte=%ld, out_packet=%ld, out_byte=%ld "
		"WHERE ip=%d AND port=%d AND nps_index=%d;",
		server->server_stats.connection_scheduled, server->server_stats.in_packet, server->server_stats.in_byte,
		server->server_stats.out_packet, server->server_stats.out_byte,
		server->ip,
		server->port, server->nps_index);

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return ALVS_DB_INTERNAL_ERROR;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Get server information
 *
 * \param[in]     service   - reference to service
 * \param[in/out] server    - reference to server
 *
 * \return      ALVS_DB_OK - server exists (info is filled with server
 *                           information if it not NULL).
 *              ALVS_DB_FAILURE - server doesn't exist.
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum alvs_db_rc internal_db_get_server(struct alvs_db_service *service,
				       struct alvs_db_server *server)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[256];

	sprintf(sql, "SELECT * FROM servers "
		"WHERE srv_ip=%d AND srv_port=%d AND srv_protocol=%d "
		"AND ip=%d AND port=%d;",
		service->ip, service->port, service->protocol,
		server->ip, server->port);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(alvs_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(alvs_db));
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(alvs_db));
		sqlite3_finalize(statement);
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Service not found */
	if (rc == SQLITE_DONE) {
		write_log(LOG_DEBUG, "No server found in required service "
			  "with required 2-tuple.");
		sqlite3_finalize(statement);
		return ALVS_DB_FAILURE;
	}

	/* retrieve server info from result */
	server->nps_index = sqlite3_column_int(statement, 5);
	server->weight = sqlite3_column_int(statement, 6);
	server->conn_flags = sqlite3_column_int(statement, 7);
	server->server_flags = sqlite3_column_int(statement, 8);
	server->u_thresh = sqlite3_column_int(statement, 9);
	server->l_thresh = sqlite3_column_int(statement, 10);
	server->active = sqlite3_column_int(statement, 11);
	server->server_stats_base.raw_data = sqlite3_column_int(statement, 12);
	server->service_stats_base.raw_data = sqlite3_column_int(statement, 13);
	server->server_on_demand_stats_base.raw_data = sqlite3_column_int(statement, 14);
	server->server_flags_dp_base.raw_data = sqlite3_column_int(statement, 15);

	/* finalize SQL statement */
	sqlite3_finalize(statement);

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Add a server assigned to a service to internal DB
 *
 * \param[in]   service   - reference to service
 * \param[in]   server    - reference to server
 *
 * \return      ALVS_DB_OK - server exists (info is filled with service
 *                           information if it not NULL).
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum alvs_db_rc internal_db_add_server(struct alvs_db_service *service,
				       struct alvs_db_server *server)
{
	int rc;
	char sql[512];
	char *zErrMsg = NULL;

	sprintf(sql, "INSERT INTO servers "
		"(ip, port, srv_ip, srv_port, srv_protocol, nps_index, weight, conn_flags, server_flags, "
		"u_thresh, l_thresh, active, server_stats_base, service_stats_base, server_on_demand_stats_base,"
		"server_flags_dp_base, connection_scheduled, in_packet, in_byte, out_packet, out_byte) "
		"VALUES (%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %ld, %ld, %ld, %ld, %ld);",
		server->ip, server->port, service->ip, service->port,
		service->protocol, server->nps_index, server->weight,
		server->conn_flags, server->server_flags, server->u_thresh,
		server->l_thresh, server->active,
		server->server_stats_base.raw_data,
		server->service_stats_base.raw_data,
		server->server_on_demand_stats_base.raw_data,
		server->server_flags_dp_base.raw_data,
		server->server_stats.connection_scheduled, server->server_stats.in_packet,
		server->server_stats.in_byte, server->server_stats.out_packet, server->server_stats.out_byte);

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return ALVS_DB_INTERNAL_ERROR;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Modify a server assigned to a service in internal DB
 *
 * \param[in]   service   - reference to service
 * \param[in]   server    - reference to server
 *
 * \return      ALVS_DB_OK - server exists (info is filled with service
 *                           information if it not NULL).
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum alvs_db_rc internal_db_modify_server(struct alvs_db_service *service,
					  struct alvs_db_server *server)
{
	int rc;
	char sql[256];
	char *zErrMsg = NULL;

	sprintf(sql, "UPDATE servers "
			"SET weight=%d, conn_flags=%d, server_flags=%d, "
			"u_thresh=%d, l_thresh=%d "
			"WHERE srv_ip=%d AND srv_port=%d AND srv_protocol=%d "
			"AND ip=%d AND port=%d;",
			server->weight, server->conn_flags,
			server->server_flags, server->u_thresh,
			server->l_thresh, service->ip, service->port,
			service->protocol, server->ip, server->port);

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return ALVS_DB_INTERNAL_ERROR;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Delete a server assigned to a service from internal DB
 *
 * \param[in]   service   - reference to service
 * \param[in]   server    - reference to server
 *
 * \return      ALVS_DB_OK - server deleted
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum alvs_db_rc internal_db_delete_server(struct alvs_db_service *service,
					  struct alvs_db_server *server)
{
	int rc;
	char sql[256];
	char *zErrMsg = NULL;

	sprintf(sql, "DELETE FROM servers "
		"WHERE srv_ip=%d AND srv_port=%d AND srv_protocol=%d "
		"AND ip=%d AND port=%d;",
		service->ip, service->port, service->protocol,
		server->ip, server->port);

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return ALVS_DB_INTERNAL_ERROR;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Deactivate a server assigned to a service in internal DB
 *
 * \param[in]   service   - reference to service
 * \param[in]   server    - reference to server
 *
 * \return      ALVS_DB_OK - server deactivated.
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum alvs_db_rc internal_db_deactivate_server(struct alvs_db_service *service,
					  struct alvs_db_server *server)
{
	int rc;
	char sql[256];
	char *zErrMsg = NULL;

	sprintf(sql, "UPDATE servers "
		"SET active=0, server_flags=server_flags&%u "
		"WHERE srv_ip=%d AND srv_port=%d AND srv_protocol=%d "
		"AND ip=%d AND port=%d;",
		~IP_VS_DEST_F_AVAILABLE,
		service->ip, service->port, service->protocol,
		server->ip, server->port);

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return ALVS_DB_INTERNAL_ERROR;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Activate a server assigned to a service in internal DB
 *
 * \param[in]   service   - reference to service
 * \param[in]   server    - reference to server
 *
 * \return      ALVS_DB_OK - server activated.
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum alvs_db_rc internal_db_activate_server(struct alvs_db_service *service,
					struct alvs_db_server *server)
{
	int rc;
	char sql[256];
	char *zErrMsg = NULL;

	sprintf(sql, "UPDATE servers "
		"SET weight=%d, active=1, server_flags=server_flags|%u, "
		"u_thresh=%d, l_thresh=%d, conn_flags=%d "
		"WHERE srv_ip=%d AND srv_port=%d AND srv_protocol=%d "
		"AND ip=%d AND port=%d;",
		server->weight, IP_VS_DEST_F_AVAILABLE,
		server->u_thresh, server->l_thresh,
		server->conn_flags, service->ip,
		service->port, service->protocol,
		server->ip, server->port);

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return ALVS_DB_INTERNAL_ERROR;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Add an application info to internal DB
 *
 * \param[in]   alvs_app_info    - reference to application info
 *
 * \return      ALVS_DB_OK - application info was added successfully.
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum alvs_db_rc internal_db_add_application_info(struct alvs_db_application_info *alvs_app_info)
{
	int rc;
	char sql[512];
	char *zErrMsg = NULL;

	sprintf(sql, "INSERT INTO application_info "
		"(application_index, is_master, is_backup, m_sync_id, b_sync_id, source_ip) "
		"VALUES (%d, %d, %d, %d, %d, %d);",
		alvs_app_info->application_index, alvs_app_info->is_master, alvs_app_info->is_backup,
		alvs_app_info->m_sync_id, alvs_app_info->b_sync_id, alvs_app_info->source_ip);

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return ALVS_DB_INTERNAL_ERROR;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Get an application info
 *
 * \param[out]	alvs_app_info    - reference to application info
 *
 * \return      ALVS_DB_OK	- application info was filled successfully
 *              ALVS_DB_FAILURE - application info with the given index doesn't exist.
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum alvs_db_rc internal_db_get_application_info(struct alvs_db_application_info *alvs_app_info)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[256];

	sprintf(sql, "SELECT * FROM application_info "
		"WHERE application_index=%d;", alvs_app_info->application_index);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(alvs_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(alvs_db));
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(alvs_db));
		sqlite3_finalize(statement);
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* given application was not found */
	if (rc == SQLITE_DONE) {
		write_log(LOG_DEBUG,
			  "No application found with required index.");
		sqlite3_finalize(statement);
		return ALVS_DB_FAILURE;
	}

	/* retrieve state sync alvs info from result */
	alvs_app_info->is_master = sqlite3_column_int(statement, 1);
	alvs_app_info->is_backup = sqlite3_column_int(statement, 2);
	alvs_app_info->m_sync_id = sqlite3_column_int(statement, 3);
	alvs_app_info->b_sync_id = sqlite3_column_int(statement, 4);
	alvs_app_info->source_ip = sqlite3_column_int(statement, 5);

	/* finalize SQL statement */
	sqlite3_finalize(statement);

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Modify application info
 *
 * \param[in]   alvs_app_info    - reference to application info
 *
 * \return      ALVS_DB_OK - application info DB was updated successfully
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum alvs_db_rc internal_db_modify_application_info(struct alvs_db_application_info *alvs_app_info)
{
	int rc;
	char sql[256];
	char *zErrMsg = NULL;

	sprintf(sql, "UPDATE application_info "
		"SET is_master=%d, is_backup=%d, m_sync_id=%d, "
		"b_sync_id=%d, source_ip=%d "
		"WHERE application_index=%d;",
		alvs_app_info->is_master, alvs_app_info->is_backup, alvs_app_info->m_sync_id,
		alvs_app_info->b_sync_id, alvs_app_info->source_ip, alvs_app_info->application_index);

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", zErrMsg);
		sqlite3_free(zErrMsg);
		return ALVS_DB_INTERNAL_ERROR;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Return GCD of two positive numbers
 *
 * \param[in]   num1   - server1 weight
 * \param[in]   num2   - server2 weight
 *
 * \return      GCD(num1,num2)
 */
uint16_t alvs_db_gcd(uint16_t num1, uint16_t num2)
{
	uint16_t r;

	while ((r = num1 % num2) != 0) {
		num1 = num2;
		num2 = r;
	}
	return num2;
}

/**************************************************************************//**
 * \brief       Reduce all server's weight in server_list with GCD(server_list)
 *
 * \param[in]   server_list   - list of servers with weight > 0,
 *				server_list != NULL
 * \param[in]   server_count  - number of servers in list
 *
 */
void alvs_db_reduce_weights(struct alvs_server_node *server_list, uint32_t server_count)
{
	uint16_t gcd, ind;

	gcd = server_list->server.weight;
	for (ind = 1; ind < server_count; ind++) {
		server_list = server_list->next;
		gcd = alvs_db_gcd(gcd, server_list->server.weight);
		if (gcd == 1) {
			return;
		}
	}
	write_log(LOG_DEBUG, "GCD of server_list = %d", gcd);

	write_log(LOG_DEBUG, "start divide weights of server_list with their GCD");
	for (ind = 0; ind < server_count; ind++) {
		server_list->server.weight = server_list->server.weight / gcd;
		server_list = server_list->next;
	}
}

/**************************************************************************//**
 * \brief       Get server with max weight in server_list
 *
 * \param[in]   server_list   - list of servers with weight >= 0,
 *				server_list != NULL
 * \param[in]   server_count  - number of servers in list
 *
 * Note: NULL will be returned in case of all server's weight = 0
 *
 */
struct alvs_server_node *alvs_db_get_max_weight_server(struct alvs_server_node *server_list, uint32_t server_count)
{
	struct alvs_server_node *max_server = NULL;
	uint16_t max_weight = 0;
	uint32_t ind;

	/* Iterate over all servers and find the maximum weight */
	for (ind = 0; ind < server_count; ind++) {
		if (server_list->server.weight > max_weight) {
			max_server = server_list;
			max_weight = server_list->server.weight;
		}
		server_list = server_list->next;
	}
	return max_server;
}

/**************************************************************************//**
 * \brief       Fills a bucket of 256 entries according to RR scheduling algorithm.
 *
 * \param[in]   bucket        - bucket of 256 entries to be filled with servers from server_list
 *                              bucket must be initialized with NULL
 *
 * \param[in]   server_list   - list of servers with weight > 0,
 *                              server_list != NULL
 * \param[in]   server_count  - number of servers in list
 *
 */
void alvs_db_rr_fill_buckets(struct alvs_db_server *bucket[], struct alvs_server_node *server_list, uint32_t server_count)
{
	uint32_t ind;

	/* the bucket will include server_count entries filled with servers and other entries will be NULL */
	for (ind = 0; ind < server_count && ind < ALVS_SIZE_OF_SCHED_BUCKET; ind++) {
		bucket[ind] = &(server_list->server);
		server_list = server_list->next;
	}
}

/**************************************************************************//**
 * \brief       Fills a bucket of 256 entries according to WRR scheduling algorithm.
 *
 * \param[in]   bucket        - bucket of 256 entries to be filled with servers from server_list
 *                              bucket must be initialized with NULL
 *
 * \param[in]   server_list   - list of servers with weight > 0,
 *                              server_list != NULL
 * \param[in]   server_count  - number of servers in list
 *
 */
void alvs_db_wrr_fill_buckets(struct alvs_db_server *bucket[], struct alvs_server_node *server_list, uint32_t server_count)
{
	uint32_t ind;
	struct alvs_server_node *max;

	/* The bucket will include Si*W(Si) entries, Si - server i, W(Si) - weight of server i, 0 <= i < server_count
	 * other entries will be NULL.
	 */
	for (ind = 0; ind < ALVS_SIZE_OF_SCHED_BUCKET; ind++) {
		max = alvs_db_get_max_weight_server(server_list, server_count);
		if (max == NULL) {
			break;
		}
		bucket[ind] = &(max->server);
		max->server.weight--;
	}
}

/**************************************************************************//**
 * \brief       Fills a bucket of 256 entries according to SH scheduling algorithm.
 *
 * \param[in]   bucket        - bucket of 256 entries to be filled with servers from server_list
 *
 * \param[in]   server_list   - list of servers with weight > 0,
 *                              server_list != NULL
 *
 */
void alvs_db_sh_fill_buckets(struct alvs_db_server *bucket[], struct alvs_server_node *server_list)
{
	uint32_t ind;
	uint16_t weight = 0;

	/* the bucket will include 256 entries filled with servers from server_list according to server's weight */
	for (ind = 0; ind < ALVS_SIZE_OF_SCHED_BUCKET; ind++) {
		bucket[ind] = &(server_list->server);
		weight++;
		if (weight >= server_list->server.weight) {
			weight = 0;
			server_list = server_list->next;
		}
	}
}

/**************************************************************************//**
 * \brief       Recalculate scheduling info DB in NPS for a service.
 *
 * \param[in]   service   - reference to service
 *
 * \return      ALVS_DB_OK - DB updated.
 *              ALVS_DB_NPS_ERROR - failed to update NPS DB
 *              ALVS_DB_NOT_SUPPORTED - scheduling algorithm not supported
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with internal DB
 */
enum alvs_db_rc alvs_db_recalculate_scheduling_info(struct alvs_db_service *service)
{
	uint32_t ind, server_count;
	struct alvs_db_server *server;
	struct alvs_server_node *server_list;
	struct alvs_sched_info_key sched_info_key;
	struct alvs_sched_info_result sched_info_result;
	struct alvs_db_server *servers_buckets[ALVS_SIZE_OF_SCHED_BUCKET] = {NULL};

	write_log(LOG_DEBUG, "Getting list of servers.");
	if (internal_db_get_server_list(service, &server_list, EXCLUDE_INACTIVE | EXCLUDE_WEIGHT_ZERO) != ALVS_DB_OK) {
		/* Can't retrieve server list */
		write_log(LOG_CRIT, "Can't retrieve server list! internal error.");
		return ALVS_DB_INTERNAL_ERROR;
	}

	if (internal_db_get_server_count(service, &server_count, EXCLUDE_INACTIVE | EXCLUDE_WEIGHT_ZERO) != ALVS_DB_OK) {
		/* Can't retrieve server list */
		write_log(LOG_CRIT, "Can't retrieve server count! internal error.");
		return ALVS_DB_INTERNAL_ERROR;
	}
	write_log(LOG_DEBUG, "Number of servers for the relevant service is %d.", server_count);

	/* Fill server buckets array according to algorithm */
	if (server_count > 0) {
		switch (service->sched_alg) {
		case ALVS_SOURCE_HASH_SCHEDULER:
			write_log(LOG_DEBUG, "filling bucket array according to SH scheduling algorithm.");
			alvs_db_reduce_weights(server_list, server_count);
			alvs_db_sh_fill_buckets(servers_buckets, server_list);
			break;
		case ALVS_ROUND_ROBIN_SCHEDULER:
			write_log(LOG_DEBUG, "filling bucket array according to RR scheduling algorithm.");
			alvs_db_rr_fill_buckets(servers_buckets, server_list, server_count);
			break;
		case ALVS_WEIGHTED_ROUND_ROBIN_SCHEDULER:
			write_log(LOG_DEBUG, "filling bucket array according to WRR scheduling algorithm.");
			alvs_db_reduce_weights(server_list, server_count);
			alvs_db_wrr_fill_buckets(servers_buckets, server_list, server_count);
			break;
		default:
			/* Other algorithms are currently not supported */
			alvs_free_server_list(server_list);
			write_log(LOG_NOTICE, "Algorithm not supported.");
			return ALVS_DB_NOT_SUPPORTED;
		}
	}

	/* Populate NPS scheduling info DB according to array */
	service->sched_entries_count = 0;
	for (ind = 0; ind < ALVS_SIZE_OF_SCHED_BUCKET; ind++) {
		server = servers_buckets[ind];
		sched_info_key.sched_index = bswap_16(service->nps_index * ALVS_SIZE_OF_SCHED_BUCKET + ind);
		if (server != NULL) {
			/* If server exists modify entry in DB.
			 * In case no entry, CP treats 'modify' as 'add'
			 */
			sched_info_result.server_index = bswap_32(server->nps_index);
			write_log(LOG_DEBUG, "(%d) %d --> %d", service->nps_index, ind, server->nps_index);
			if (infra_modify_entry(STRUCT_ID_ALVS_SCHED_INFO, &sched_info_key,
					       sizeof(struct alvs_sched_info_key), &sched_info_result,
					       sizeof(struct alvs_sched_info_result)) == false) {
				write_log(LOG_ERR, "Failed to modify a scheduling info entry.");
				alvs_free_server_list(server_list);
				return ALVS_DB_NPS_ERROR;
			}

			/* update number of scheduling info entries in service */
			service->sched_entries_count++;
		} else {
			/* If server doesn't exist delete entry in DB.
			 * In case no entry, CP treats 'delete' as 'nop'
			 */
			write_log(LOG_DEBUG, "(%d) %d --> EMPTY", service->nps_index, ind);
			if (infra_delete_entry(STRUCT_ID_ALVS_SCHED_INFO, &sched_info_key,
					       sizeof(struct alvs_sched_info_key)) == false) {
				write_log(LOG_ERR, "Failed to delete a scheduling info entry.");
				alvs_free_server_list(server_list);
				return ALVS_DB_NPS_ERROR;
			}
		}
	}

	/* Free the list when finished using */
	alvs_free_server_list(server_list);

	write_log(LOG_DEBUG, "Scheduling info recalculated.");
	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Get number of total connections from a statistics counter
 *
 * \param[in]   server_index   - index of server
 *       [out]  number of total connections
 *
 * \return      ALVS_DB_OK - succeed.
 *              ALVS_DB_NPS_ERROR - failed to read counters
 */
enum alvs_db_rc alvs_db_get_num_conn_total(uint32_t server_index, uint64_t *conn_total)
{
	if (infra_get_long_counters(EMEM_SERVER_STATS_ON_DEMAND_OFFSET + server_index * ALVS_NUM_OF_SERVERS_ON_DEMAND_STATS + ALVS_SERVER_STATS_CONNECTION_TOTAL_OFFSET,
				    1,
				    conn_total) == false) {
		return ALVS_DB_NPS_ERROR;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Clear all statistic counter of a server
 *
 * \param[in]   server_index   - index of server
 *
 * \return      ALVS_DB_OK - succeed.
 *              ALVS_DB_NPS_ERROR - failed to clean counters
 */
enum alvs_db_rc alvs_db_clean_server_stats(uint32_t server_index)
{

	if (infra_clear_long_counters(EMEM_SERVER_STATS_ON_DEMAND_OFFSET + server_index * ALVS_NUM_OF_SERVERS_ON_DEMAND_STATS,
				      ALVS_NUM_OF_SERVERS_ON_DEMAND_STATS) == false) {
		return ALVS_DB_NPS_ERROR;
	}

	if (infra_clear_posted_counters(EMEM_SERVER_STATS_POSTED_OFFSET + server_index * ALVS_NUM_OF_SERVER_STATS,
					ALVS_NUM_OF_SERVER_STATS) == false) {
		return ALVS_DB_NPS_ERROR;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Clear error stats
 *
 * \param[in]
 *
 * \return      ALVS_DB_OK - succeed.
 *              ALVS_DB_NPS_ERROR - failed to clean NPS DB
 */
enum alvs_db_rc alvs_db_clean_error_stats(void)
{

	if (infra_clear_posted_counters(EMEM_ALVS_ERROR_STATS_POSTED_OFFSET,
					ALVS_NUM_OF_ALVS_ERROR_STATS) == false) {
		return ALVS_DB_NPS_ERROR;
	}

	return ALVS_DB_OK;

}

/**************************************************************************//**
 * \brief       Clear interface stats
 *
 * \param[in]
 *
 * \return      ALVS_DB_OK - succeed.
 *              ALVS_DB_NPS_ERROR - failed to clean counters
 */
enum alvs_db_rc alvs_db_clean_interface_stats(void)
{
	if (infra_clear_posted_counters(EMEM_IF_STATS_POSTED_OFFSET,
					NW_NUM_OF_IF_STATS * (USER_NW_IF_NUM + 1)) == false) {/* 4 nw interface and 1 host interface */
		return ALVS_DB_NPS_ERROR;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Clear all statistic counter of a service
 *
 * \param[in]   service_index   - index of service
 *
 * \return      ALVS_DB_OK - succeed.
 *              ALVS_DB_NPS_ERROR - failed to clean counters
 */
enum alvs_db_rc alvs_db_clean_service_stats(uint32_t service_index)
{
	if (infra_clear_posted_counters(EMEM_SERVICE_STATS_POSTED_OFFSET + service_index * ALVS_NUM_OF_SERVICE_STATS,
						     ALVS_NUM_OF_SERVICE_STATS) == false) {
		return ALVS_DB_NPS_ERROR;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       translate scheduling algorithm string to type
 *
 * \param[in]   sched_name   - scheduling algorithm string
 *
 * \return      scheduling algorithm type
 */
enum alvs_scheduler_type get_sched_alg(char *sched_name)
{
	if (strcmp(sched_name, "rr") == 0) {
		return ALVS_ROUND_ROBIN_SCHEDULER;
	}
	if (strcmp(sched_name, "wrr") == 0) {
		return ALVS_WEIGHTED_ROUND_ROBIN_SCHEDULER;
	}
	if (strcmp(sched_name, "lc") == 0) {
		return ALVS_LEAST_CONNECTION_SCHEDULER;
	}
	if (strcmp(sched_name, "wlc") == 0) {
		return ALVS_WEIGHTED_LEAST_CONNECTION_SCHEDULER;
	}
	if (strcmp(sched_name, "dh") == 0) {
		return ALVS_DESTINATION_HASH_SCHEDULER;
	}
	if (strcmp(sched_name, "sh") == 0) {
		return ALVS_SOURCE_HASH_SCHEDULER;
	}
	if (strcmp(sched_name, "lblc") == 0) {
		return ALVS_LB_LEAST_CONNECTION_SCHEDULER;
	}
	if (strcmp(sched_name, "lblcr") == 0) {
		return ALVS_LB_LEAST_CONNECTION_SCHEDULER_WITH_REPLICATION;
	}
	if (strcmp(sched_name, "sed") == 0) {
		return ALVS_SHORTEST_EXPECTED_DELAY_SCHEDULER;
	}
	if (strcmp(sched_name, "nq") == 0) {
		return ALVS_NEVER_QUEUE_SCHEDULER;
	}

	return ALVS_SCHEDULER_LAST;
}

/**************************************************************************//**
 * \brief       Checks if a protocol is supported by application
 *
 * \param[in]   protocol   - received protocol
 *
 * \return      true/false
 */
bool supported_protocol(uint16_t protocol)
{
	if (protocol == IPPROTO_TCP) {
		return true;
	}
	return false;
}

/**************************************************************************//**
 * \brief       Checks if a scheduling algorithm is supported by application
 *
 * \param[in]   sched_alg   - received scheduling algorithm
 *
 * \return      true/false
 */
bool supported_sched_alg(enum alvs_scheduler_type sched_alg)
{
	if (sched_alg == ALVS_SOURCE_HASH_SCHEDULER) {
		return true;
	}
	if (sched_alg == ALVS_ROUND_ROBIN_SCHEDULER) {
		return true;
	}
	if (sched_alg == ALVS_WEIGHTED_ROUND_ROBIN_SCHEDULER) {
		return true;
	}
	return false;
}

/**************************************************************************//**
 * \brief       Checks if a routing algorithm is supported by application
 *
 * \param[in]   conn_flags   - received connection flags
 *
 * \return      true/false
 */
bool supported_routing_alg(uint32_t conn_flags)
{
	if ((conn_flags & IP_VS_CONN_F_FWD_MASK) == IP_VS_CONN_F_DROUTE) {
		return true;
	}
	return false;
}

/**************************************************************************//**
 * \brief       Build service info key for NPS table
 *
 * \param[in]   cp_service   - service received from CP.
 *
 * \return
 */
void build_nps_service_info_key(struct alvs_db_service *cp_service,
				struct alvs_service_info_key *nps_service_info_key)
{
	nps_service_info_key->service_index = cp_service->nps_index;
}

/**************************************************************************//**
 * \brief       Build service info result for NPS table
 *
 * \param[in]   cp_service   - service received from CP.
 *
 * \return
 */
void build_nps_service_info_result(struct alvs_db_service *cp_service,
				   struct alvs_service_info_result *nps_service_info_result)
{
	nps_service_info_result->sched_alg = cp_service->sched_alg;
	nps_service_info_result->sched_entries_count = bswap_16(cp_service->sched_entries_count);
	nps_service_info_result->service_flags = bswap_32(cp_service->flags);
	nps_service_info_result->service_stats_base = bswap_32(cp_service->stats_base.raw_data);
	nps_service_info_result->service_sched_ctr = bswap_32((EZDP_INTERNAL_MS << EZDP_SUM_ADDR_MEM_TYPE_OFFSET) |
		(EZDP_ALL_CLUSTER_DATA << EZDP_SUM_ADDR_MSID_OFFSET) |
		(cp_service->nps_index << EZDP_SUM_ADDR_ELEMENT_INDEX_OFFSET));
}

/**************************************************************************//**
 * \brief       Build service classification key for NPS table
 *
 * \param[in]   cp_service   - service received from CP.
 *
 * \return
 */
void build_nps_service_classification_key(struct alvs_db_service *cp_service,
					  struct alvs_service_classification_key *nps_service_classification_key)
{
	nps_service_classification_key->service_address = bswap_32(cp_service->ip);
	nps_service_classification_key->service_port = bswap_16(cp_service->port);
	nps_service_classification_key->service_protocol = bswap_16(cp_service->protocol);
}

/**************************************************************************//**
 * \brief       Build service classification result for NPS table
 *
 * \param[in]   cp_service   - service received from CP.
 *
 * \return
 */
void build_nps_service_classification_result(struct alvs_db_service *cp_service,
					     struct alvs_service_classification_result *nps_service_classification_result)
{
	nps_service_classification_result->service_index = cp_service->nps_index;
}

/**************************************************************************//**
 * \brief       Build server info key for NPS table
 *
 * \param[in]   cp_server   - server received from CP.
 * \param[out]  nps_server_info_key   - server info key.
 *
 * \return
 */
void build_nps_server_info_key(struct alvs_db_server *cp_server,
			       struct alvs_server_info_key *nps_server_info_key)
{
	nps_server_info_key->server_index = bswap_32(cp_server->nps_index);
}

/**************************************************************************//**
 * \brief       Build server info result for NPS table
 *
 * \param[in]   cp_server   - server received from CP.
 * \param[out]  nps_server_info_result   - server info key.
 *
 * \return
 */
void build_nps_server_info_result(struct alvs_db_server *cp_server,
				  struct alvs_server_info_result *nps_server_info_result)
{
	nps_server_info_result->conn_flags = bswap_32(cp_server->conn_flags);
	nps_server_info_result->server_flags = cp_server->server_flags;
	nps_server_info_result->server_ip = bswap_32(cp_server->ip);
	nps_server_info_result->server_port = bswap_16(cp_server->port);
	nps_server_info_result->server_stats_base = bswap_32(cp_server->server_stats_base.raw_data);
	nps_server_info_result->service_stats_base = bswap_32(cp_server->service_stats_base.raw_data);
	nps_server_info_result->server_on_demand_stats_base = bswap_32(cp_server->server_on_demand_stats_base.raw_data);
	nps_server_info_result->server_flags_dp_base = bswap_32(cp_server->server_flags_dp_base.raw_data);
	nps_server_info_result->u_thresh = bswap_16(cp_server->u_thresh);
	nps_server_info_result->l_thresh = bswap_16(cp_server->l_thresh);
}

/**************************************************************************//**
 * \brief       Build server classification key for NPS table
 *
 * \param[in]   cp_service  - service received from CP.
 * \param[in]   cp_server   - server received from CP.
 * \param[out]  nps_server_classification_key   - server classification key.
 *
 * \return
 */
void build_nps_server_classification_key(struct alvs_db_service *cp_service,
					 struct alvs_db_server *cp_server,
					 struct alvs_server_classification_key *nps_server_classification_key)
{
	nps_server_classification_key->virtual_ip = bswap_32(cp_service->ip);
	nps_server_classification_key->virtual_port = bswap_16(cp_service->port);
	nps_server_classification_key->server_ip = bswap_32(cp_server->ip);
	nps_server_classification_key->server_port = bswap_16(cp_server->port);
	nps_server_classification_key->protocol = bswap_16(cp_service->protocol);
}

/**************************************************************************//**
 * \brief       Build server classification result for NPS table
 *
 * \param[in]   cp_server   - server received from CP.
 * \param[out]  nps_server_classification_result   - server classification key.
 *
 * \return
 */
void build_nps_server_classification_result(struct alvs_db_server *cp_server,
					    struct alvs_server_classification_result *nps_server_classification_result)
{
	nps_server_classification_result->server_index = bswap_32(cp_server->nps_index);
}

/**************************************************************************//**
 * \brief       Build application info result for NPS table
 *
 * \param[in]   cp_daemon_info - state sync daemon info received from CP.
 * \param[out]  nps_server_classification_result   - application info result.
 *
 * \return
 */
void build_nps_application_info_result(struct alvs_db_application_info *cp_daemon_info, union application_info_result *nps_application_info_result)
{
	nps_application_info_result->alvs_app.master_bit = cp_daemon_info->is_master;
	nps_application_info_result->alvs_app.backup_bit = cp_daemon_info->is_backup;
	nps_application_info_result->alvs_app.m_sync_id = cp_daemon_info->m_sync_id;
	nps_application_info_result->alvs_app.b_sync_id = cp_daemon_info->b_sync_id;
	nps_application_info_result->alvs_app.source_ip = bswap_32(cp_daemon_info->source_ip);
}

/**************************************************************************//**
 * \brief       Build application info key for NPS table
 *
 * \param[out]   nps_application_info_key - application info key.
 * \param[in]    app_index - application index in the application info table
 *
 * \return
 */
void build_nps_application_info_key(struct application_info_key *nps_application_info_key, uint8_t app_index)
{
	nps_application_info_key->application_index = app_index;
}

/**************************************************************************//**
 * \brief       build alvs state sync info for internal application info DB according to one daemon
 *
 * \param[in]   cp_daemon_info      - reference to an object in the internal application info DB
 * \param[in]   ip_vs_daemon_info   - reference to state sync daemon info
 * \param[in]   start_ss            - true, build cp state sync daemon after start-daemon
 *                                    false, build cp state sync daemon after stop-daemon
 */
void build_cp_state_sync_daemon(struct alvs_db_application_info *cp_daemon_info, struct ip_vs_daemon_user *ip_vs_daemon_info, bool start_ss)
{
	if (ip_vs_daemon_info->state == IP_VS_STATE_MASTER) {
		if (start_ss == true) {
			cp_daemon_info->is_master = true;
			cp_daemon_info->m_sync_id = ip_vs_daemon_info->syncid;
		} else {
			cp_daemon_info->is_master = false;
			cp_daemon_info->m_sync_id = 0;
			cp_daemon_info->source_ip = 0;
		}
	} else if (ip_vs_daemon_info->state == IP_VS_STATE_BACKUP) {
		if (start_ss == true) {
			cp_daemon_info->is_backup = true;
			cp_daemon_info->b_sync_id = ip_vs_daemon_info->syncid;
		} else {
			cp_daemon_info->is_backup = false;
			cp_daemon_info->b_sync_id = 0;

		}
	}
}

/**************************************************************************//**
 * \brief       Build an object for the internal application info DB
 *		we might get two sync daemons to update one for master and the other for backup.
 *
 * \param[in]   cp_daemon_info   - reference to an object in the internal application info DB
 *				 - given cp_daemon_info should be initialized.
 * \param[in]   ip_vs_daemon_info   - pointer to array with size 2 (master & backup) for state sync daemon info
 */
void build_cp_state_sync_daemons(struct alvs_db_application_info *cp_daemon_info, struct ip_vs_daemon_user *ip_vs_daemon_info)
{
	cp_daemon_info->application_index = ALVS_APPLICATION_INFO_INDEX;
	/* no sync daemon was configured */
	if (ip_vs_daemon_info->state != IP_VS_STATE_MASTER && ip_vs_daemon_info->state != IP_VS_STATE_BACKUP) {
		cp_daemon_info->is_master = false;
		cp_daemon_info->m_sync_id = 0;
		cp_daemon_info->is_backup = false;
		cp_daemon_info->b_sync_id = 0;
		cp_daemon_info->source_ip = 0;
		return;
	}
	build_cp_state_sync_daemon(cp_daemon_info, ip_vs_daemon_info, true);
	/* continue to the second sync daemon - if exists */
	ip_vs_daemon_info++;
	build_cp_state_sync_daemon(cp_daemon_info, ip_vs_daemon_info, true);
}

/**************************************************************************//**
 * \brief       API to add new service to ALVS date bases
 *
 * \param[in]   ip_vs_service   - new service reference
 *
 * \return      ALVS_DB_OK - operation succeeded
 *              ALVS_DB_NOT_SUPPORTED - protocol or scheduling not supported
 *                                      Or reached max service count.
 *              ALVS_DB_FAILURE - service exists
 *              ALVS_DB_INTERNAL_ERROR - received an error from internal DB
 *              ALVS_DB_NPS_ERROR - failed to update NPS DB
 */
enum alvs_db_rc alvs_db_add_service(struct ip_vs_service_user *ip_vs_service)
{
	struct alvs_db_service cp_service;
	struct alvs_service_info_key nps_service_info_key;
	struct alvs_service_info_result nps_service_info_result;
	struct alvs_service_classification_key nps_service_classification_key;
	struct alvs_service_classification_result nps_service_classification_result;

	/* Check is request is supported */
	if (supported_protocol(ip_vs_service->protocol) == false) {
		write_log(LOG_NOTICE, "Protocol (%d) is not supported.", ip_vs_service->protocol);
		return ALVS_DB_NOT_SUPPORTED;
	}
	if (supported_sched_alg(get_sched_alg(ip_vs_service->sched_name)) == false) {
		write_log(LOG_NOTICE, "Scheduling algorithm (%s) is not supported.", ip_vs_service->sched_name);
		return ALVS_DB_NOT_SUPPORTED;
	}

	/* init the cp_service internal entry */
	memset(&cp_service, 0, sizeof(cp_service));

	/* Check if service already exists in internal DB */
	cp_service.ip = bswap_32(ip_vs_service->addr);
	cp_service.port = bswap_16(ip_vs_service->port);
	cp_service.protocol = ip_vs_service->protocol;
	switch (internal_db_get_service(&cp_service, false)) {
	case ALVS_DB_OK:
		/* Service already exists */
		write_log(LOG_NOTICE, "Can't add service. Service (%s:%d, protocol=%d) already exists.",
			  my_inet_ntoa(cp_service.ip), cp_service.port, cp_service.protocol);
		return ALVS_DB_FAILURE;
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find service (internal error).");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_FAILURE:
		/* Service doesn't exist */
		write_log(LOG_DEBUG, "Service (%s:%d, protocol=%d) doesn't exist",
			  my_inet_ntoa(cp_service.ip), cp_service.port, cp_service.protocol);
		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Service doesn't exist
	 * Allocate an index for the new service
	 */
	if (index_pool_alloc(&service_index_pool, &cp_service.nps_index) == false) {
		write_log(LOG_ERR, "Can't add service. Reached maximum.");
		return ALVS_DB_NOT_SUPPORTED;
	}
	write_log(LOG_DEBUG, "Allocated nps_index = %d", cp_service.nps_index);

	/* Fill information of the service */
	cp_service.sched_alg = get_sched_alg(ip_vs_service->sched_name);
	cp_service.flags = ip_vs_service->flags;
	cp_service.sched_entries_count = 0;
	cp_service.stats_base.raw_data = (EZDP_EXTERNAL_MS << EZDP_SUM_ADDR_MEM_TYPE_OFFSET) |
		(EMEM_SERVICE_STATS_POSTED_MSID << EZDP_SUM_ADDR_MSID_OFFSET) |
		((EMEM_SERVICE_STATS_POSTED_OFFSET + cp_service.nps_index * ALVS_NUM_OF_SERVICE_STATS) << EZDP_SUM_ADDR_ELEMENT_INDEX_OFFSET);

	write_log(LOG_DEBUG, "Service info: alg=%d, flags=%d",
		  cp_service.sched_alg, cp_service.flags);

	/* Clean service statistics */
	write_log(LOG_DEBUG, "Cleaning service statistics.");
	if (alvs_db_clean_service_stats(cp_service.nps_index) == ALVS_DB_INTERNAL_ERROR) {
		write_log(LOG_CRIT, "Failed to clean statistics.");
		index_pool_release(&service_index_pool, cp_service.nps_index);
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Add service info to internal DB */
	if (internal_db_add_service(&cp_service) == ALVS_DB_INTERNAL_ERROR) {
		write_log(LOG_CRIT, "Failed to add service to internal DB.");
		index_pool_release(&service_index_pool, cp_service.nps_index);
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Add service info to NPS search structure */
	build_nps_service_info_key(&cp_service,
				   &nps_service_info_key);
	build_nps_service_info_result(&cp_service,
				      &nps_service_info_result);
	if (infra_add_entry(STRUCT_ID_ALVS_SERVICE_INFO, &nps_service_info_key,
			    sizeof(struct alvs_service_info_key),
			    &nps_service_info_result,
			    sizeof(struct alvs_service_info_result)) == false) {
		write_log(LOG_CRIT, "Failed to add service info entry to NPS.");
		index_pool_release(&service_index_pool, cp_service.nps_index);
		return ALVS_DB_NPS_ERROR;
	}

	/* Add service classification to NPS search structure */
	build_nps_service_classification_key(&cp_service,
					     &nps_service_classification_key);
	build_nps_service_classification_result(&cp_service,
						&nps_service_classification_result);
	if (infra_add_entry(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION,
			    &nps_service_classification_key,
			    sizeof(struct alvs_service_classification_key),
			    &nps_service_classification_result,
			    sizeof(struct alvs_service_classification_result)) == false) {
		write_log(LOG_CRIT, "Failed to add service classification entry to NPS.");
		index_pool_release(&service_index_pool, cp_service.nps_index);
		return ALVS_DB_NPS_ERROR;
	}

	write_log(LOG_INFO, "Service (%s:%d, protocol=%d) added successfully.",
		  my_inet_ntoa(cp_service.ip), cp_service.port, cp_service.protocol);
	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       API to modify an existing service in ALVS date bases
 *
 * \param[in]   ip_vs_service   - service reference
 *
 * \return      ALVS_DB_OK - operation succeeded
 *              ALVS_DB_NOT_SUPPORTED - scheduling alg not supported
 *              ALVS_DB_FAILURE - service does not exists
 *              ALVS_DB_INTERNAL_ERROR - received an error from internal DB
 *              ALVS_DB_NPS_ERROR - failed to update NPS DB
 */
enum alvs_db_rc alvs_db_modify_service(struct ip_vs_service_user *ip_vs_service)
{
	struct alvs_db_service cp_service;
	struct alvs_service_info_key nps_service_info_key;
	struct alvs_service_info_result nps_service_info_result;
	enum alvs_scheduler_type   prev_sched_alg;

	/* Check if request is supported */
	if (supported_sched_alg(get_sched_alg(ip_vs_service->sched_name)) == false) {
		write_log(LOG_NOTICE, "Scheduling algorithm (%s) is not supported.", ip_vs_service->sched_name);
		return ALVS_DB_NOT_SUPPORTED;
	}

	/* Check if service exists in internal DB */
	cp_service.ip = bswap_32(ip_vs_service->addr);
	cp_service.port = bswap_16(ip_vs_service->port);
	cp_service.protocol = ip_vs_service->protocol;
	switch (internal_db_get_service(&cp_service, true)) {
	case ALVS_DB_FAILURE:
		/* Service doesn't exist */
		write_log(LOG_NOTICE, "Can't modify service. Service (%s:%d, protocol=%d) doesn't exist.",
		       my_inet_ntoa(cp_service.ip), cp_service.port, cp_service.protocol);
		return ALVS_DB_FAILURE;
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find service (internal error).");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_OK:
		/* Service exists */
		write_log(LOG_DEBUG, "Service (%s:%d, protocol=%d) exists",
			  my_inet_ntoa(cp_service.ip), cp_service.port, cp_service.protocol);
		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Modify information of the service */
	prev_sched_alg = cp_service.sched_alg;
	cp_service.sched_alg = get_sched_alg(ip_vs_service->sched_name);
	cp_service.flags = ip_vs_service->flags;

	write_log(LOG_DEBUG, "Service info: alg=%d, flags=%d",
		  cp_service.sched_alg, cp_service.flags);

	if (prev_sched_alg != cp_service.sched_alg) {
		/* Recalculate scheduling information */
		enum alvs_db_rc rc = alvs_db_recalculate_scheduling_info(&cp_service);

		if (rc != ALVS_DB_OK) {
			write_log(LOG_CRIT, "Failed to recalculate scheduling information.");
			return rc;
		}
	}

	/* Modify service information in internal DB */
	if (internal_db_modify_service(&cp_service) == ALVS_DB_INTERNAL_ERROR) {
		write_log(LOG_CRIT, "Failed to modify service in internal DB.");
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Modify service information in NPS search structure */
	build_nps_service_info_key(&cp_service, &nps_service_info_key);
	build_nps_service_info_result(&cp_service, &nps_service_info_result);
	if (infra_modify_entry(STRUCT_ID_ALVS_SERVICE_INFO, &nps_service_info_key,
			       sizeof(struct alvs_service_info_key), &nps_service_info_result,
			       sizeof(struct alvs_service_info_result)) == false) {
		write_log(LOG_CRIT, "Failed to modify service info entry in NPS.");
		return ALVS_DB_NPS_ERROR;
	}

	write_log(LOG_INFO, "Service (%s:%d, protocol=%d) modified successfully.",
		  my_inet_ntoa(cp_service.ip), cp_service.port, cp_service.protocol);
	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       API to delete an existing service from ALVS date bases
 *
 * \param[in]   ip_vs_service   - service reference
 *
 * \return      ALVS_DB_OK - operation succeeded
 *              ALVS_DB_FAILURE - service does not exists
 *              ALVS_DB_INTERNAL_ERROR - received an error from internal DB
 *              ALVS_DB_NPS_ERROR - failed to update NPS DB
 */
enum alvs_db_rc alvs_db_delete_service(struct ip_vs_service_user *ip_vs_service)
{
	uint32_t ind;
	uint32_t server_count;
	struct alvs_db_service cp_service;
	struct alvs_service_info_key nps_service_info_key;
	struct alvs_service_classification_key nps_service_classification_key;
	struct alvs_server_info_key nps_server_info_key;
	struct alvs_server_info_result nps_server_info_result;
	struct alvs_server_node *server_list;

	/* Check if service exists in internal DB */
	cp_service.ip = bswap_32(ip_vs_service->addr);
	cp_service.port = bswap_16(ip_vs_service->port);
	cp_service.protocol = ip_vs_service->protocol;
	switch (internal_db_get_service(&cp_service, true)) {
	case ALVS_DB_FAILURE:
		/* Service doesn't exist */
		write_log(LOG_NOTICE, "Can't delete service. Service (%s:%d, protocol=%d) doesn't exist.",
			  my_inet_ntoa(cp_service.ip), cp_service.port, cp_service.protocol);
		return ALVS_DB_FAILURE;
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find service (internal error).");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_OK:
		/* Service exists */
		write_log(LOG_DEBUG, "Service (%s:%d, protocol=%d) exists",
			  my_inet_ntoa(cp_service.ip), cp_service.port, cp_service.protocol);
		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Get servers to delete */
	internal_db_get_server_count(&cp_service, &server_count, EXCLUDE_INACTIVE);

	/* Get list of servers to delete */
	write_log(LOG_DEBUG, "Getting list of servers.");
	if (internal_db_get_server_list(&cp_service, &server_list, EXCLUDE_INACTIVE) != ALVS_DB_OK) {
		/* Can't retrieve server list */
		write_log(LOG_CRIT, "Can't retrieve server list - "
			  "internal error.");
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Delete service from internal DB and deactivate servers */
	if (internal_db_delete_service(&cp_service) == ALVS_DB_INTERNAL_ERROR) {
		write_log(LOG_CRIT, "Failed to delete service from internal DB.");
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Release service index */
	write_log(LOG_DEBUG, "Releasing nps_index %d.", cp_service.nps_index);
	index_pool_release(&service_index_pool, cp_service.nps_index);

	/* Delete service classification to NPS search structure */
	build_nps_service_classification_key(&cp_service,
					     &nps_service_classification_key);
	if (infra_delete_entry(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION,
			       &nps_service_classification_key,
			       sizeof(struct alvs_service_classification_key)) == false) {
		write_log(LOG_CRIT, "Failed to delete service classification entry.");
		return ALVS_DB_NPS_ERROR;
	}

	/* Delete service info from NPS search structure */
	build_nps_service_info_key(&cp_service, &nps_service_info_key);
	if (infra_delete_entry(STRUCT_ID_ALVS_SERVICE_INFO,
			       &nps_service_info_key,
			       sizeof(struct alvs_service_info_key)) == false) {
		write_log(LOG_CRIT, "Failed to delete service info entry.");
		return ALVS_DB_NPS_ERROR;
	}

	/* Deactivate all servers */
	for (ind = 0; ind < server_count; ind++) {
		write_log(LOG_DEBUG, "Deactivating server with nps_index %d.", server_list->server.nps_index);
		/* Deactivate server in struct */
		server_list->server.server_flags &= ~IP_VS_DEST_F_AVAILABLE;
		build_nps_server_info_key(&server_list->server, &nps_server_info_key);
		build_nps_server_info_result(&server_list->server, &nps_server_info_result);
		if (infra_modify_entry(STRUCT_ID_ALVS_SERVER_INFO,
				       &nps_server_info_key,
				       sizeof(struct alvs_server_info_key),
				       &nps_server_info_result,
				       sizeof(struct alvs_server_info_result)) == false) {
			write_log(LOG_CRIT, "Failed to modify server info entry.");
			alvs_free_server_list(server_list);
			return ALVS_DB_NPS_ERROR;
		}
	}

	alvs_free_server_list(server_list);

	/* Delete scheduling information from NPS search structure (if existed) */
	write_log(LOG_DEBUG, "Updating scheduling information.");
	enum alvs_db_rc rc = alvs_db_recalculate_scheduling_info(&cp_service);

	if (rc != ALVS_DB_OK) {
		write_log(LOG_CRIT, "Failed to delete scheduling information.");
		return rc;
	}

	write_log(LOG_INFO, "Service (%s:%d, protocol=%d) deleted successfully.",
		  my_inet_ntoa(cp_service.ip), cp_service.port, cp_service.protocol);
	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       API to retrieve service flags
 *
 * \param[in]   ip_vs_service   - service reference
 *
 * \return      ALVS_DB_OK - operation succeeded
 *              ALVS_DB_FAILURE - service does not exists
 *              ALVS_DB_INTERNAL_ERROR - received an internal error from DB
 */
enum alvs_db_rc alvs_db_get_service_flags(struct ip_vs_service_user *ip_vs_service)
{
	struct alvs_db_service cp_service;

	/* Check if service exists in internal DB */
	cp_service.ip = bswap_32(ip_vs_service->addr);
	cp_service.port = bswap_16(ip_vs_service->port);
	cp_service.protocol = ip_vs_service->protocol;
	switch (internal_db_get_service(&cp_service, true)) {
	case ALVS_DB_FAILURE:
		/* Service doesn't exist */
		write_log(LOG_DEBUG, "Can't find service (%s:%d, protocol=%d).",
			  my_inet_ntoa(cp_service.ip), cp_service.port, cp_service.protocol);
		return ALVS_DB_FAILURE;
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find service (internal error).");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_OK:
		/* Service exists */
		write_log(LOG_DEBUG, "Service (%s:%d, protocol=%d) found",
			  my_inet_ntoa(cp_service.ip), cp_service.port, cp_service.protocol);
		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Get service flags */
	ip_vs_service->flags = cp_service.flags;

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       API to add server to ALVS DBs
 *
 * \param[in]   ip_vs_service   - service reference
 * \param[in]   ip_vs_dest      - server reference
 *
 * \return      ALVS_DB_OK - operation succeeded
 *              ALVS_DB_FAILURE - server exists / service doesn't exist
 *              ALVS_DB_NOT_SUPPORTED - Routing algorithm is not supported
 *                                      Or reached max server count.
 *              ALVS_DB_INTERNAL_ERROR - received an error from internal DB
 *              ALVS_DB_NPS_ERROR - failed to update NPS DB
 */
enum alvs_db_rc alvs_db_add_server(struct ip_vs_service_user *ip_vs_service,
				   struct ip_vs_dest_user *ip_vs_dest)
{
	enum alvs_db_rc rc;
	uint32_t server_count = 0;
	struct alvs_db_service cp_service;
	struct alvs_db_server cp_server;
	struct alvs_server_info_key nps_server_info_key;
	struct alvs_server_info_result nps_server_info_result;
	struct alvs_service_info_key nps_service_info_key;
	struct alvs_service_info_result nps_service_info_result;
	struct alvs_server_classification_key nps_server_classification_key;
	struct alvs_server_classification_result nps_server_classification_result;

	/* Check if request is supported */
	if (supported_routing_alg(ip_vs_dest->conn_flags) == false) {
		write_log(LOG_NOTICE, "Routing algorithm (%d) is not supported.", ip_vs_dest->conn_flags & IP_VS_CONN_F_FWD_MASK);
		return ALVS_DB_NOT_SUPPORTED;
	}

	if (ip_vs_dest->l_threshold > ip_vs_dest->u_threshold) {
		write_log(LOG_ERR, "l_threshold %d > u_threshold %d", ip_vs_dest->l_threshold, ip_vs_dest->u_threshold);
		return ALVS_DB_NOT_SUPPORTED;
	}

	/* init the cp_server internal server entry */
	memset(&cp_server, 0, sizeof(cp_server));

	/* Check if service already exists in internal DB */
	cp_service.ip = bswap_32(ip_vs_service->addr);
	cp_service.port = bswap_16(ip_vs_service->port);
	cp_service.protocol = ip_vs_service->protocol;
	switch (internal_db_get_service(&cp_service, true)) {
	case ALVS_DB_FAILURE:
		/* Service doesn't exist */
		write_log(LOG_NOTICE, "Can't add server. Service (%s:%d, protocol=%d) doesn't exist.",
			  my_inet_ntoa(cp_service.ip), cp_service.port, cp_service.protocol);
		return ALVS_DB_FAILURE;
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find service (internal error).");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_OK:
		/* Service exists */
		write_log(LOG_DEBUG, "Service (%s:%d, protocol=%d) exists.",
			  my_inet_ntoa(cp_service.ip), cp_service.port, cp_service.protocol);
		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* check if service has maximum servers already */
	internal_db_get_server_count(&cp_service, &server_count, EXCLUDE_INACTIVE);
	if (server_count == ALVS_SIZE_OF_SCHED_BUCKET) {
		write_log(LOG_NOTICE, "Can't add server. Service (%s:%d, protocol=%d) has maximum number of active servers.",
			  my_inet_ntoa(cp_service.ip), cp_service.port, cp_service.protocol);
		return ALVS_DB_FAILURE;
	}

	cp_server.ip = bswap_32(ip_vs_dest->addr);
	cp_server.port = bswap_16(ip_vs_dest->port);
	switch (internal_db_get_server(&cp_service, &cp_server)) {
	case ALVS_DB_OK:
		if (cp_server.active == true) {
			/* Server exists. */
			write_log(LOG_NOTICE, "Can't add server. Server (%s:%d) already exists.",
				  my_inet_ntoa(cp_server.ip), cp_server.port);
			return ALVS_DB_FAILURE;
		}

		/* Server exists, but inactive.
		 * Need to move server to be active
		 */
		write_log(LOG_DEBUG, "Server (%s:%d) exists (inactive).",
			  my_inet_ntoa(cp_server.ip), cp_server.port);
		cp_server.server_flags |= IP_VS_DEST_F_AVAILABLE;
		cp_server.conn_flags = ip_vs_dest->conn_flags;
		cp_server.weight = ip_vs_dest->weight;
		cp_server.u_thresh = ip_vs_dest->u_threshold;
		cp_server.l_thresh = ip_vs_dest->l_threshold;
		if (internal_db_activate_server(&cp_service, &cp_server) == ALVS_DB_INTERNAL_ERROR) {
			write_log(LOG_CRIT, "Server activation failed..");
			return ALVS_DB_INTERNAL_ERROR;
		}

		build_nps_server_info_key(&cp_server,
					  &nps_server_info_key);
		build_nps_server_info_result(&cp_server,
					     &nps_server_info_result);
		if (infra_modify_entry(STRUCT_ID_ALVS_SERVER_INFO,
				       &nps_server_info_key,
				       sizeof(struct alvs_server_info_key),
				       &nps_server_info_result,
				       sizeof(struct alvs_server_info_result)) == false) {
			write_log(LOG_CRIT, "Failed to modify server info entry.");
			return ALVS_DB_NPS_ERROR;
		}

		break;
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_CRIT, "Can't find server (internal error).");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_FAILURE:
		/* server doesn't exist */
		write_log(LOG_DEBUG, "Server (%s:%d) doesn't exist.",
			  my_inet_ntoa(cp_server.ip), cp_server.port);
		/* Server doesn't exist.
		 * Create a new entry in server info.
		 */
		if (index_pool_alloc(&server_index_pool, &cp_server.nps_index) == false) {
			write_log(LOG_ERR, "Can't add server. Reached maximum.");
			return ALVS_DB_NOT_SUPPORTED;
		}
		write_log(LOG_DEBUG, "Allocated nps_index = %d", cp_server.nps_index);

		cp_server.conn_flags = ip_vs_dest->conn_flags;
		cp_server.server_flags = IP_VS_DEST_F_AVAILABLE;
		cp_server.weight = ip_vs_dest->weight;
		cp_server.active = true;
		cp_server.u_thresh = ip_vs_dest->u_threshold;
		cp_server.l_thresh = ip_vs_dest->l_threshold;
		cp_server.server_stats_base.raw_data = (EZDP_EXTERNAL_MS << EZDP_SUM_ADDR_MEM_TYPE_OFFSET) | (EMEM_SERVER_STATS_POSTED_MSID << EZDP_SUM_ADDR_MSID_OFFSET) | ((EMEM_SERVER_STATS_POSTED_OFFSET + cp_server.nps_index * ALVS_NUM_OF_SERVER_STATS) << EZDP_SUM_ADDR_ELEMENT_INDEX_OFFSET);
		cp_server.service_stats_base.raw_data = cp_service.stats_base.raw_data;
		cp_server.server_on_demand_stats_base.raw_data = (EZDP_EXTERNAL_MS << EZDP_SUM_ADDR_MEM_TYPE_OFFSET) |
			(EMEM_SERVER_STATS_ON_DEMAND_MSID << EZDP_SUM_ADDR_MSID_OFFSET) |
			((EMEM_SERVER_STATS_ON_DEMAND_OFFSET + cp_server.nps_index * ALVS_NUM_OF_SERVERS_ON_DEMAND_STATS) << EZDP_SUM_ADDR_ELEMENT_INDEX_OFFSET);
		cp_server.server_flags_dp_base.raw_data = (EZDP_EXTERNAL_MS << EZDP_SUM_ADDR_MEM_TYPE_OFFSET) |
			(EMEM_SERVER_FLAGS_MSID << EZDP_SUM_ADDR_MSID_OFFSET) |
			((EMEM_SERVER_FLAGS_OFFSET + cp_server.nps_index) << EZDP_SUM_ADDR_ELEMENT_INDEX_OFFSET);

		write_log(LOG_DEBUG, "Server info: conn_flags=%d, server_flags=%d, weight=%d, u_thresh=%d, l_thresh=%d.",
			  cp_server.conn_flags,
			  cp_server.server_flags, cp_server.weight,
			  cp_server.u_thresh, cp_server.l_thresh);
		write_log(LOG_DEBUG, "raw = 0x%x msid = %d, element_index = %d", cp_server.server_flags_dp_base.raw_data, cp_server.server_flags_dp_base.msid, cp_server.server_flags_dp_base.element_index);

		/* Clean service statistics */
		write_log(LOG_DEBUG, "Cleaning server statistics.");
		if (alvs_db_clean_server_stats(cp_server.nps_index) == ALVS_DB_INTERNAL_ERROR) {
			write_log(LOG_CRIT, "Failed to clean statistics.");
			index_pool_release(&server_index_pool, cp_server.nps_index);
			return ALVS_DB_INTERNAL_ERROR;
		}

		if (alvs_clear_overloaded_flag_of_server(&cp_server) == ALVS_DB_INTERNAL_ERROR) {
			write_log(LOG_CRIT, "Failed to clean allocated memory.");
			index_pool_release(&server_index_pool, cp_server.nps_index);
			return ALVS_DB_INTERNAL_ERROR;
		}

		if (internal_db_add_server(&cp_service, &cp_server) == ALVS_DB_INTERNAL_ERROR) {
			write_log(LOG_CRIT, "Failed to add server to internal DB.");
			index_pool_release(&server_index_pool, cp_server.nps_index);
			return ALVS_DB_INTERNAL_ERROR;
		}

		build_nps_server_info_key(&cp_server,
					  &nps_server_info_key);
		build_nps_server_info_result(&cp_server,
					     &nps_server_info_result);
		if (infra_modify_entry(STRUCT_ID_ALVS_SERVER_INFO,
				       &nps_server_info_key,
				       sizeof(struct alvs_server_info_key),
				       &nps_server_info_result,
				       sizeof(struct alvs_server_info_result)) == false) {
			write_log(LOG_CRIT, "Failed to add server info entry.");
			index_pool_release(&server_index_pool, cp_server.nps_index);
			return ALVS_DB_NPS_ERROR;
		}

		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	if (ip_vs_dest->weight > 0) {
		/* Recalculate scheduling information */
		rc = alvs_db_recalculate_scheduling_info(&cp_service);
		if (rc != ALVS_DB_OK) {
			write_log(LOG_ERR, "Failed to delete scheduling information.");
			return rc;
		}

		if (internal_db_modify_service(&cp_service) == ALVS_DB_INTERNAL_ERROR) {
			write_log(LOG_CRIT, "Failed to modify service in internal DB.");
			return ALVS_DB_INTERNAL_ERROR;
		}

		build_nps_service_info_key(&cp_service,
					   &nps_service_info_key);
		build_nps_service_info_result(&cp_service,
					      &nps_service_info_result);
		if (infra_modify_entry(STRUCT_ID_ALVS_SERVICE_INFO,
				       &nps_service_info_key,
				       sizeof(struct alvs_service_info_key),
				       &nps_service_info_result,
				       sizeof(struct alvs_service_info_result)) == false) {
			write_log(LOG_CRIT, "Failed to change server count in service info entry.");
			return ALVS_DB_NPS_ERROR;
		}
	}

	build_nps_server_classification_key(&cp_service,
					    &cp_server,
					    &nps_server_classification_key);
	build_nps_server_classification_result(&cp_server,
					       &nps_server_classification_result);
	if (infra_add_entry(STRUCT_ID_ALVS_SERVER_CLASSIFICATION,
			    &nps_server_classification_key,
			    sizeof(struct alvs_server_classification_key),
			    &nps_server_classification_result,
			    sizeof(struct alvs_server_classification_result)) == false) {
		write_log(LOG_CRIT, "Failed to add server classification entry.");
		return ALVS_DB_NPS_ERROR;
	}

	write_log(LOG_INFO, "Server (%s:%d) added successfully.",
		  my_inet_ntoa(cp_server.ip), cp_server.port);
	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       API to modify server in ALVS DBs
 *
 * \param[in]   ip_vs_service   - service reference
 * \param[in]   ip_vs_dest      - server reference
 *
 * \return      ALVS_DB_OK - operation succeeded
 *              ALVS_DB_FAILURE - server/service doesn't exist or inactive
 *              ALVS_DB_NOT_SUPPORTED - Routing algorithm is not supported
 *              ALVS_DB_INTERNAL_ERROR - received an error from internal DB
 *              ALVS_DB_NPS_ERROR - failed to update NPS DB
 */
enum alvs_db_rc alvs_db_modify_server(struct ip_vs_service_user *ip_vs_service,
			      struct ip_vs_dest_user *ip_vs_dest)
{
	enum alvs_db_rc rc;
	struct alvs_db_service cp_service;
	struct alvs_db_server cp_server;
	struct alvs_server_info_key nps_server_info_key;
	struct alvs_server_info_result nps_server_info_result;
	struct alvs_service_info_key nps_service_info_key;
	struct alvs_service_info_result nps_service_info_result;
	uint8_t prev_weight;

	/* Check is request is supported */
	if (supported_routing_alg(ip_vs_dest->conn_flags) == false) {
		write_log(LOG_NOTICE, "Routing algorithm (%d) is not supported.", ip_vs_dest->conn_flags & IP_VS_CONN_F_FWD_MASK);
		return ALVS_DB_NOT_SUPPORTED;
	}

	if (ip_vs_dest->l_threshold > ip_vs_dest->u_threshold) {
		write_log(LOG_ERR, "l_threshold %d > u_threshold %d", ip_vs_dest->l_threshold, ip_vs_dest->u_threshold);
		return ALVS_DB_NOT_SUPPORTED;
	}
	/* Check if service already exists in internal DB */
	cp_service.ip = bswap_32(ip_vs_service->addr);
	cp_service.port = bswap_16(ip_vs_service->port);
	cp_service.protocol = ip_vs_service->protocol;
	switch (internal_db_get_service(&cp_service, true)) {
	case ALVS_DB_FAILURE:
		/* Service doesn't exist */
		write_log(LOG_NOTICE, "Can't modify server. Service (%s:%d, protocol=%d) doesn't exist.",
			  my_inet_ntoa(cp_service.ip), cp_service.port, cp_service.protocol);
		return ALVS_DB_FAILURE;
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find service (internal error).");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_OK:
		/* Service exists */
		write_log(LOG_DEBUG, "Service (%s:%d, protocol=%d) exists",
			  my_inet_ntoa(cp_service.ip), cp_service.port, cp_service.protocol);
		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	cp_server.ip = bswap_32(ip_vs_dest->addr);
	cp_server.port = bswap_16(ip_vs_dest->port);
	switch (internal_db_get_server(&cp_service, &cp_server)) {
	case ALVS_DB_FAILURE:
		/* Server exists. */
		write_log(LOG_NOTICE, "Can't add server. Server (%s:%d) doesn't exist.",
			  my_inet_ntoa(cp_server.ip), cp_server.port);
		return ALVS_DB_FAILURE;
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_CRIT, "Can't find server (internal error).");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_OK:
		if (cp_server.active == false) {
			/* Server exists, but inactive. */
			write_log(LOG_NOTICE, "Can't modify server. Server (%s:%d) is inactive.",
				  my_inet_ntoa(cp_server.ip), cp_server.port);
			return ALVS_DB_FAILURE;
		}

		/* Server exists */
		write_log(LOG_DEBUG, "Server (%s:%d) exists (active).",
			  my_inet_ntoa(cp_server.ip), cp_server.port);
		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	prev_weight = cp_server.weight;
	cp_server.conn_flags = ip_vs_dest->conn_flags;
	cp_server.weight = ip_vs_dest->weight;

	if ((ip_vs_dest->u_threshold == 0) || (ip_vs_dest->u_threshold > cp_server.u_thresh)) {
		if (alvs_clear_overloaded_flag_of_server(&cp_server) == ALVS_DB_INTERNAL_ERROR) {
			write_log(LOG_CRIT, "Failed to alvs_clear_overloaded_flag_of_server.");
			return ALVS_DB_INTERNAL_ERROR;
		}
	}
	cp_server.u_thresh = ip_vs_dest->u_threshold;
	cp_server.l_thresh = ip_vs_dest->l_threshold;

	write_log(LOG_DEBUG, "Server info: conn_flags=%d, server_flags=%d, weight=%d, u_thresh=%d, l_thresh=%d.",
		  cp_server.conn_flags,
		  cp_server.server_flags, cp_server.weight, cp_server.u_thresh,
		  cp_server.l_thresh);

	if (internal_db_modify_server(&cp_service, &cp_server) == ALVS_DB_INTERNAL_ERROR) {
		write_log(LOG_CRIT, "Failed to modify server in internal DB.");
		return ALVS_DB_INTERNAL_ERROR;
	}

	if (cp_server.weight != prev_weight) {
		/* Recalculate scheduling information */
		rc = alvs_db_recalculate_scheduling_info(&cp_service);
		if (rc != ALVS_DB_OK) {
			write_log(LOG_CRIT, "Failed to delete scheduling information.");
			return rc;
		}

		build_nps_service_info_key(&cp_service,
					   &nps_service_info_key);
		build_nps_service_info_result(&cp_service,
					      &nps_service_info_result);
		if (infra_modify_entry(STRUCT_ID_ALVS_SERVICE_INFO,
				       &nps_service_info_key,
				       sizeof(struct alvs_service_info_key),
				       &nps_service_info_result,
				       sizeof(struct alvs_service_info_result)) == false) {
			write_log(LOG_CRIT, "Failed to change server count in service info entry.");
			return ALVS_DB_NPS_ERROR;
		}
	}

	build_nps_server_info_key(&cp_server, &nps_server_info_key);
	build_nps_server_info_result(&cp_server, &nps_server_info_result);
	if (infra_modify_entry(STRUCT_ID_ALVS_SERVER_INFO,
			       &nps_server_info_key,
			    sizeof(struct alvs_server_info_key),
			    &nps_server_info_result,
			    sizeof(struct alvs_server_info_result)) == false) {
		write_log(LOG_CRIT, "Failed to modify server info entry.");
		return ALVS_DB_NPS_ERROR;
	}

	write_log(LOG_INFO, "Server (%s:%d) modified successfully.",
		  my_inet_ntoa(cp_server.ip), cp_server.port);
	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       API to delete a server from ALVS DBs
 *
 * \param[in]   ip_vs_service   - service reference
 * \param[in]   ip_vs_dest      - server reference
 *
 * \return      ALVS_DB_OK - operation succeeded
 *              ALVS_DB_FAILURE - server/service doesn't exist or inactive
 *              ALVS_DB_INTERNAL_ERROR - received an error from internal DB
 *              ALVS_DB_NPS_ERROR - failed to update NPS DB
 */
enum alvs_db_rc alvs_db_delete_server(struct ip_vs_service_user *ip_vs_service,
			      struct ip_vs_dest_user *ip_vs_dest)
{
	enum alvs_db_rc rc;
	struct alvs_db_service cp_service;
	struct alvs_db_server cp_server;
	struct alvs_service_info_key nps_service_info_key;
	struct alvs_service_info_result nps_service_info_result;
	struct alvs_server_info_key nps_server_info_key;
	struct alvs_server_info_result nps_server_info_result;
	struct alvs_server_classification_key nps_server_classification_key;

	/* Check if service already exists in internal DB */
	cp_service.ip = bswap_32(ip_vs_service->addr);
	cp_service.port = bswap_16(ip_vs_service->port);
	cp_service.protocol = ip_vs_service->protocol;
	switch (internal_db_get_service(&cp_service, true)) {
	case ALVS_DB_FAILURE:
		/* Service doesn't exist */
		write_log(LOG_NOTICE, "Can't delete server. Service (%s:%d, protocol=%d) doesn't exist.",
			  my_inet_ntoa(cp_service.ip), cp_service.port, cp_service.protocol);
		return ALVS_DB_FAILURE;
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find service (internal error).");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_OK:
		/* Service exists */
		write_log(LOG_DEBUG, "Service (%s:%d, protocol=%d) exists.",
			  my_inet_ntoa(cp_service.ip), cp_service.port, cp_service.protocol);
		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	cp_server.ip = bswap_32(ip_vs_dest->addr);
	cp_server.port = bswap_16(ip_vs_dest->port);
	switch (internal_db_get_server(&cp_service, &cp_server)) {
	case ALVS_DB_FAILURE:
		/* Server exists. */
		write_log(LOG_NOTICE, "Can't delete server. Server (%s:%d) doesn't exist.",
			  my_inet_ntoa(cp_server.ip), cp_server.port);
		return ALVS_DB_FAILURE;
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_CRIT, "Can't find server (internal error).");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_OK:
		if (cp_server.active == false) {
			/* Server exists, but inactive. */
			write_log(LOG_NOTICE, "Can't delete server. Server (%s:%d) already inactive.",
				  my_inet_ntoa(cp_server.ip), cp_server.port);
			return ALVS_DB_FAILURE;
		}

		/* Server exists */
		write_log(LOG_DEBUG, "Server (%s:%d) exists (active).",
			  my_inet_ntoa(cp_server.ip), cp_server.port);
		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Deactivate server in struct in in internal DB */
	cp_server.server_flags &= ~IP_VS_DEST_F_AVAILABLE;
	if (internal_db_deactivate_server(&cp_service, &cp_server) == ALVS_DB_INTERNAL_ERROR) {
		write_log(LOG_CRIT, "Failed to deactivate server in internal DB.");
		return ALVS_DB_INTERNAL_ERROR;
	}

	if (cp_server.weight > 0) {
		/* Recalculate scheduling information */
		rc = alvs_db_recalculate_scheduling_info(&cp_service);
		if (rc != ALVS_DB_OK) {
			write_log(LOG_CRIT, "Failed to delete scheduling information.");
			return rc;
		}

		build_nps_service_info_key(&cp_service, &nps_service_info_key);
		build_nps_service_info_result(&cp_service, &nps_service_info_result);
		if (infra_modify_entry(STRUCT_ID_ALVS_SERVICE_INFO,
				       &nps_service_info_key,
				       sizeof(struct alvs_service_info_key),
				       &nps_service_info_result,
				       sizeof(struct alvs_service_info_result)) == false) {
			write_log(LOG_CRIT, "Failed to change server count in service info entry.");
			return ALVS_DB_NPS_ERROR;
		}
	}

	build_nps_server_info_key(&cp_server, &nps_server_info_key);
	build_nps_server_info_result(&cp_server, &nps_server_info_result);
	if (infra_modify_entry(STRUCT_ID_ALVS_SERVER_INFO,
			       &nps_server_info_key,
			    sizeof(struct alvs_server_info_key),
			    &nps_server_info_result,
			    sizeof(struct alvs_server_info_result)) == false) {
		write_log(LOG_CRIT, "Failed to modify server info entry.");
		return ALVS_DB_NPS_ERROR;
	}

	build_nps_server_classification_key(&cp_service,
					    &cp_server,
					    &nps_server_classification_key);
	if (infra_delete_entry(STRUCT_ID_ALVS_SERVER_CLASSIFICATION,
			       &nps_server_classification_key,
			       sizeof(struct alvs_server_classification_key)) == false) {
		write_log(LOG_CRIT, "Failed to delete server classification entry.");
		return ALVS_DB_NPS_ERROR;
	}

	write_log(LOG_INFO, "Server (%s:%d) deleted successfully.",
		  my_inet_ntoa(cp_server.ip), cp_server.port);
	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       check multi-cast interface argument in the ipvsadm --start-daemon command
 *		the function will log an error in case given interface is not configured
 *		, and will warn if it's configured with WLAN.
 *		currently it should be only eth0.
 *		in case of master sync daemon, this function will set the source ip, in the cp_daemon_info,
 *		of the given interface.
 *
 * \param[in]   ip_vs_daemon_info   - state sync daemon info reference
 * \param[in]   cp_daemon_info   -    reference to an object in the internal application info DB
 *
 * \return      true/false - mcast interface is configured or not.
 */
bool alvs_db_handle_mcast_if(struct ip_vs_daemon_user *ip_vs_daemon_info, struct alvs_db_application_info *cp_daemon_info)
{
	struct ifaddrs *ifaddr, *iter;
	struct sockaddr_in *sa;
	char host[NI_MAXHOST];
	bool res = false;
	int rc;

	/* check if there is any configured state sync daemon */
	if (ip_vs_daemon_info->state != IP_VS_STATE_MASTER && ip_vs_daemon_info->state != IP_VS_STATE_BACKUP) {
		return true;
	}
	write_log(LOG_DEBUG, "Start verifying mcast interface for start sync daemon.");
	if (getifaddrs(&ifaddr) == -1) {
		write_log(LOG_ERR, "getifaddrs() failed.");
		return false;
	}
	for (iter = ifaddr; iter != NULL; iter = iter->ifa_next) {
		if (iter->ifa_addr == NULL)
			continue;
		/* IP protocol family */
		if (iter->ifa_addr->sa_family == AF_INET) {
			rc = getnameinfo(iter->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
			if (rc != 0) {
				write_log(LOG_ERR, "getnameinfo() failed: %s.", gai_strerror(rc));
				freeifaddrs(ifaddr);
				return false;
			}
			if (strcmp(iter->ifa_name, ip_vs_daemon_info->mcast_ifn) == 0) {
				/* in case of master sync daemon, we need to update the source ip of the given interface */
				if (ip_vs_daemon_info->state == IP_VS_STATE_MASTER) {
					sa = (struct sockaddr_in *) iter->ifa_addr;
					cp_daemon_info->source_ip = bswap_32(sa->sin_addr.s_addr);
				}
				res = true;
			}
		}
	}

	freeifaddrs(ifaddr);

	if (res == false) {
		write_log(LOG_ERR, "Interface : <%s> is not configured.", ip_vs_daemon_info->mcast_ifn);
	}
	return res;
}

/**************************************************************************//**
 * \brief       API to init state sync daemon info in application info DBs
 *
 * \param[in]   ip_vs_daemon_info   - state sync daemon info reference
 *
 * \return      ALVS_DB_OK - operation succeeded
 *              ALVS_DB_INTERNAL_ERROR - received an error from internal DB
 *              ALVS_DB_NPS_ERROR - failed to update NPS DB
 *              ALVS_DB_FAILURE   - mcast interface is not valid
 */
enum alvs_db_rc alvs_db_init_daemon(struct ip_vs_daemon_user *ip_vs_daemon_info)
{
	struct alvs_db_application_info cp_daemon_info;
	struct application_info_key nps_ss_daemon_info_key;
	union application_info_result nps_ss_daemon_info_result;

	memset(&cp_daemon_info, 0, sizeof(cp_daemon_info));

	switch (internal_db_get_application_info(&cp_daemon_info)) {
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_CRIT, "Can't find state sync daemon info (internal error).");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_OK:
		/* state sync daemon info exists */
		write_log(LOG_DEBUG, "State sync daemon info exists. start initializing");
		build_cp_state_sync_daemons(&cp_daemon_info, ip_vs_daemon_info);

		/* Check given mcast-if if configured or not and set master's mcast-if source ip (if exists) */
		if (alvs_db_handle_mcast_if(ip_vs_daemon_info, &cp_daemon_info) == false) {
			/* given mcast_ifn is not valid*/
			write_log(LOG_NOTICE, "multi-cast interface is not configured. Can't init state sync daemon.");
			return ALVS_DB_FAILURE;
		}

		/* modify internal DB */
		if (internal_db_modify_application_info(&cp_daemon_info) == ALVS_DB_INTERNAL_ERROR) {
			write_log(LOG_CRIT, "Failed to update state sync daemon info in internal DB(internal error).");
			return ALVS_DB_INTERNAL_ERROR;
		}
		write_log(LOG_DEBUG, "ALVS state sync daemon info was modified successfully in internal DB.");
		break;
	case ALVS_DB_FAILURE:
		/* state sync daemon info does not exist */
		write_log(LOG_DEBUG, "State sync daemon info does not exist. start initializing");
		build_cp_state_sync_daemons(&cp_daemon_info, ip_vs_daemon_info);

		/* Check given mcast-if if configured or not and set master's mcast-if source ip (if exists) */
		if (alvs_db_handle_mcast_if(ip_vs_daemon_info, &cp_daemon_info) == false) {
			/* given mcast_ifn is not valid*/
			write_log(LOG_NOTICE, "multi-cast interface is not configured. Can't init state sync daemon.");
			return ALVS_DB_FAILURE;
		}

		/* add to internal DB */
		if (internal_db_add_application_info(&cp_daemon_info) == ALVS_DB_INTERNAL_ERROR) {
			write_log(LOG_CRIT, "Failed to add state sync daemon info to the internal DB(internal error).");
			return ALVS_DB_INTERNAL_ERROR;
		}
		write_log(LOG_DEBUG, "State sync daemon info was added successfully to internal DB.");
		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	write_log(LOG_DEBUG, "Initialized State sync daemon info with: is_master = %d, is_backup = %d, m_sync_id = %d, b_sync_id = %d, source_ip = %s",
		  cp_daemon_info.is_master, cp_daemon_info.is_backup, cp_daemon_info.m_sync_id, cp_daemon_info.b_sync_id, my_inet_ntoa(cp_daemon_info.source_ip));

	/* add to NPS DB */
	build_nps_application_info_key(&nps_ss_daemon_info_key, ALVS_APPLICATION_INFO_INDEX);
	build_nps_application_info_result(&cp_daemon_info, &nps_ss_daemon_info_result);
	if (infra_add_entry(STRUCT_ID_APPLICATION_INFO,
			       &nps_ss_daemon_info_key,
			    sizeof(struct application_info_key),
			    &nps_ss_daemon_info_result,
			    sizeof(union application_info_result)) == false) {
		write_log(LOG_CRIT, "Failed to add ALVS application info entry.");
		return ALVS_DB_NPS_ERROR;
	}
	write_log(LOG_DEBUG, "State sync daemon info was added successfully to NPS DB.");

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       API to start state sync daemon info
 *
 * \param[in]   ip_vs_daemon_info   - state sync daemon info reference
 *
 * \return      ALVS_DB_OK - operation succeeded
 *              ALVS_DB_FAILURE - state sync master/backup is already running and can't run another one.
 *				- mcast-ifn is not configured.
 *              ALVS_DB_INTERNAL_ERROR - received an error from internal DB
 *              ALVS_DB_NPS_ERROR - failed to update NPS DB
 */
enum alvs_db_rc alvs_db_start_daemon(struct ip_vs_daemon_user *ip_vs_daemon_info)
{
	struct alvs_db_application_info cp_daemon_info;
	struct application_info_key nps_ss_daemon_info_key;
	union application_info_result nps_ss_daemon_info_result;

	write_log(LOG_DEBUG, "Start state sync daemon info.");
	/* Get state sync daemon from internal DB */
	cp_daemon_info.application_index = ALVS_APPLICATION_INFO_INDEX;
	switch (internal_db_get_application_info(&cp_daemon_info)) {
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_CRIT, "Can't access application info internal DB (internal error).");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_FAILURE:
		write_log(LOG_CRIT, "ALVS application info does not exist. Can't start state sync daemon");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_OK:
		/* state sync daemon info exists */
		write_log(LOG_DEBUG, "State sync daemon info exists. is_master = %d, is_backup = %d, m_sync_id = %d, b_sync_id = %d, source_ip = %s",
			  cp_daemon_info.is_master, cp_daemon_info.is_backup, cp_daemon_info.m_sync_id, cp_daemon_info.b_sync_id, my_inet_ntoa(cp_daemon_info.source_ip));
		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Check if state sync daemon with given state already running */
	if (cp_daemon_info.is_master && ip_vs_daemon_info->state == IP_VS_STATE_MASTER) {
		write_log(LOG_NOTICE, "State sync master daemon (sync_id = %d) is already running. Can't start another one with sync_id = %d!", cp_daemon_info.m_sync_id, ip_vs_daemon_info->syncid);
		return ALVS_DB_FAILURE;
	}
	if (cp_daemon_info.is_backup && ip_vs_daemon_info->state == IP_VS_STATE_BACKUP) {
		write_log(LOG_NOTICE, "State sync backup daemon (sync_id = %d) is already running. Can't start another one with sync_id = %d!", cp_daemon_info.b_sync_id, ip_vs_daemon_info->syncid);
		return ALVS_DB_FAILURE;
	}

	/* Check in given mcast interface is configured or not */
	if (alvs_db_handle_mcast_if(ip_vs_daemon_info, &cp_daemon_info) == false) {
		/* given mcast_ifn is not valid*/
		write_log(LOG_NOTICE, "multi-cast interface is not configured. Can't start state sync daemon.");
		return ALVS_DB_FAILURE;
	}

	/* update cp_daemon_info */
	build_cp_state_sync_daemon(&cp_daemon_info, ip_vs_daemon_info, true);

	write_log(LOG_DEBUG, "New state sync daemon info: is_master = %d, is_backup = %d, m_sync_id = %d, b_sync_id = %d, source_ip = %s",
		  cp_daemon_info.is_master, cp_daemon_info.is_backup, cp_daemon_info.m_sync_id, cp_daemon_info.b_sync_id, my_inet_ntoa(cp_daemon_info.source_ip));

	/* modify internal DB */
	if (internal_db_modify_application_info(&cp_daemon_info) == ALVS_DB_INTERNAL_ERROR) {
		write_log(LOG_CRIT, "Failed to update state sync daemon info in internal DB(internal error).");
		return ALVS_DB_INTERNAL_ERROR;
	}
	write_log(LOG_DEBUG, "ALVS state sync daemon info was modified successfully in internal DB.");

	/* modify nps DB */
	build_nps_application_info_key(&nps_ss_daemon_info_key, ALVS_APPLICATION_INFO_INDEX);
	build_nps_application_info_result(&cp_daemon_info, &nps_ss_daemon_info_result);
	if (infra_modify_entry(STRUCT_ID_APPLICATION_INFO,
			       &nps_ss_daemon_info_key,
			    sizeof(struct application_info_key),
			    &nps_ss_daemon_info_result,
			    sizeof(union application_info_result)) == false) {
		write_log(LOG_CRIT, "Failed to modify application info entry.");
		return ALVS_DB_NPS_ERROR;
	}
	write_log(LOG_DEBUG, "ALVS state sync daemon info was modified successfully in NPS DB.");

	write_log(LOG_INFO, "ALVS state sync daemon (syncid=%d) started successfully.", ip_vs_daemon_info->syncid);

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       API to stop state sync daemon info
 *
 * \param[in]   ip_vs_daemon_info   - state sync daemon info reference
 *
 * \return      ALVS_DB_OK - operation succeeded
 *              ALVS_DB_FAILURE - state sync master/backup is not running and can't stop it.
 *              ALVS_DB_INTERNAL_ERROR - received an error from internal DB
 *              ALVS_DB_NPS_ERROR - failed to update NPS DB
 */
enum alvs_db_rc alvs_db_stop_daemon(struct ip_vs_daemon_user *ip_vs_daemon_info)
{
	struct alvs_db_application_info cp_daemon_info;
	struct application_info_key nps_ss_daemon_info_key;
	union application_info_result nps_ss_daemon_info_result;

	write_log(LOG_DEBUG, "Stop state sync daemon.");
	/* Get state sync daemon from internal DB */
	cp_daemon_info.application_index = ALVS_APPLICATION_INFO_INDEX;
	switch (internal_db_get_application_info(&cp_daemon_info)) {
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_CRIT, "Can't access application info internal DB (internal error).");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_FAILURE:
		write_log(LOG_CRIT, "ALVS application info does not exist. Can't stop state sync daemon");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_OK:
		/* state sync daemon info exists */
		write_log(LOG_DEBUG, "State sync daemon info exists. is_master = %d, is_backup = %d, m_sync_id = %d, b_sync_id = %d, source_ip = %s",
			  cp_daemon_info.is_master, cp_daemon_info.is_backup, cp_daemon_info.m_sync_id, cp_daemon_info.b_sync_id, my_inet_ntoa(cp_daemon_info.source_ip));
		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Check if state sync daemon was started */
	if (cp_daemon_info.is_master == 0 && ip_vs_daemon_info->state == IP_VS_STATE_MASTER) {
		write_log(LOG_NOTICE, "State sync master daemon is not running. Can't stop it!");
		return ALVS_DB_FAILURE;
	}
	if (cp_daemon_info.is_backup == 0 && ip_vs_daemon_info->state == IP_VS_STATE_BACKUP) {
		write_log(LOG_NOTICE, "State sync backup daemon is not running. Can't stop it!");
		return ALVS_DB_FAILURE;
	}

	/* update cp_daemon_info */
	build_cp_state_sync_daemon(&cp_daemon_info, ip_vs_daemon_info, false);

	write_log(LOG_DEBUG, "New state sync daemon info: is_master = %d, is_backup = %d, m_sync_id = %d, b_sync_id = %d, source_ip = %s",
		  cp_daemon_info.is_master, cp_daemon_info.is_backup, cp_daemon_info.m_sync_id, cp_daemon_info.b_sync_id, my_inet_ntoa(cp_daemon_info.source_ip));

	/* modify internal DB */
	if (internal_db_modify_application_info(&cp_daemon_info) == ALVS_DB_INTERNAL_ERROR) {
		write_log(LOG_ERR, "Failed to update state sync daemon info in internal DB(internal error).");
		return ALVS_DB_INTERNAL_ERROR;
	}
	write_log(LOG_DEBUG, "ALVS state sync daemon info was modified successfully in internal DB.");

	/* modify nps DB */
	build_nps_application_info_key(&nps_ss_daemon_info_key, ALVS_APPLICATION_INFO_INDEX);
	build_nps_application_info_result(&cp_daemon_info, &nps_ss_daemon_info_result);
	if (infra_modify_entry(STRUCT_ID_APPLICATION_INFO,
			       &nps_ss_daemon_info_key,
			    sizeof(struct application_info_key),
			    &nps_ss_daemon_info_result,
			    sizeof(union application_info_result)) == false) {
		write_log(LOG_CRIT, "Failed to modify application info entry.");
		return ALVS_DB_NPS_ERROR;
	}
	write_log(LOG_DEBUG, "ALVS state sync daemon info was modified successfully in NPS DB.");

	write_log(LOG_INFO, "ALVS state sync daemon stopped successfully.");

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       API to log state sync daemon info
 *
 * \return      ALVS_DB_OK - operation succeeded
 *              ALVS_DB_INTERNAL_ERROR - received an error from internal DB
 */
enum alvs_db_rc alvs_db_log_daemon(void)
{
	struct alvs_db_application_info cp_daemon_info;

	write_log(LOG_DEBUG, "Get state sync daemon.");
	/* Get state sync daemon from internal DB */
	cp_daemon_info.application_index = ALVS_APPLICATION_INFO_INDEX;
	switch (internal_db_get_application_info(&cp_daemon_info)) {
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_CRIT, "Can't access application info internal DB (internal error).");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_FAILURE:
		/* state sync daemon info does not exist */
		write_log(LOG_INFO, "No sync daemon.");
		return ALVS_DB_OK;
	case ALVS_DB_OK:
		/* state sync daemon info exists */
		if (cp_daemon_info.is_master == true)
			write_log(LOG_INFO, "Master sync daemon (syncid=%d).", cp_daemon_info.m_sync_id);
		if (cp_daemon_info.is_backup == true)
			write_log(LOG_INFO, "Backup sync daemon (syncid=%d).", cp_daemon_info.b_sync_id);
		if (cp_daemon_info.is_master == false && cp_daemon_info.is_backup == false)
			write_log(LOG_INFO, "No sync daemon.");
		return ALVS_DB_OK;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}
}



/* TODO - This structure is used only in the workaround described in alvs_db_clear().
 *        Once CP will fix the delete_all_entries bug this structure should be removed
 */
struct alvs_service_node {
	struct alvs_db_service service;
	struct alvs_service_node *next;
};


/* TODO - This function is used only in the workaround described in alvs_db_clear().
 *        Once CP will fix the delete_all_entries bug this function should be removed
 */
enum alvs_db_rc internal_db_get_service_count(uint32_t *service_count)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[256];

	sprintf(sql, "SELECT COUNT (nps_index) AS service_count FROM services;");

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(alvs_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(alvs_db));
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* In case of error return 0 */
	if (rc != SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(alvs_db));
		sqlite3_finalize(statement);
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* retrieve count from result,
	 * finalize SQL statement and return count
	 */
	*service_count = sqlite3_column_int(statement, 0);
	sqlite3_finalize(statement);
	return ALVS_DB_OK;
}

/* TODO - This function is used only in the workaround described in alvs_db_clear().
 *        Once CP will fix the delete_all_entries bug this function should be removed
 */
void alvs_free_service_list(struct alvs_service_node *list)
{
	struct alvs_service_node *node, *temp;

	if (list == NULL) {
		return;
	}

	node = list;
	do {
		temp = node;
		node = node->next;
		free(temp);
	} while (node != list);
}

/* TODO - This function is used only in the workaround described in alvs_db_clear().
 *        Once CP will fix the delete_all_entries bug this function should be removed
 */
enum alvs_db_rc internal_db_get_service_list(struct alvs_service_node **service_list)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[256];
	struct alvs_service_node *node;

	sprintf(sql, "SELECT * FROM services;");

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(alvs_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(alvs_db));
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	*service_list = NULL;

	/* Collect service ids */
	while (rc == SQLITE_ROW) {
		node = (struct alvs_service_node *)malloc(sizeof(struct alvs_service_node));
		if (node == NULL) {
			write_log(LOG_ERR, "Failed to allocate memory");
			alvs_free_service_list(*service_list);
			return ALVS_DB_FAILURE;
		}
		node->service.ip = sqlite3_column_int(statement, 0);
		node->service.port = sqlite3_column_int(statement, 1);
		node->service.protocol = sqlite3_column_int(statement, 2);
		node->service.nps_index = sqlite3_column_int(statement, 3);

		if (*service_list == NULL) {
			node->next = node;
			*service_list = node;
		} else {
			node->next = (*service_list)->next;
			(*service_list)->next = node;
			(*service_list) = node;
		}

		rc = sqlite3_step(statement);
	}

	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(alvs_db));
		sqlite3_finalize(statement);
		alvs_free_service_list(*service_list);
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* finalize SQL statement */
	sqlite3_finalize(statement);

	if (*service_list != NULL) {
		(*service_list) = (*service_list)->next;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       API to print servers statistics from the last zero command
 *	prints the diff between the internal db statistics (were updated on the zero command)
 *	and the posted counters
 *
 * \param[in]   ip_vs_service    - struct ip_vs_service_user, ipvs service
 *
 * \return      ALVS_DB_OK - operation succeeded
 *              ALVS_DB_INTERNAL_ERROR - received an error from internal DB
 */
enum alvs_db_rc alvs_db_print_servers_stats(struct ip_vs_service_user *ip_vs_service)
{
	uint32_t server_count, i;
	struct alvs_server_node *server_list; /* used for the counters on internal db (were updated on the last zero command )*/
	struct alvs_db_server server_counters;	  /* used for the poseted counters */
	enum alvs_db_rc get_counters_rc;
	struct alvs_db_service service;

	/* init the cp_service internal entry */
	memset(&service, 0, sizeof(service));
	memset(&server_counters, 0, sizeof(server_counters));

	service.ip = bswap_32(ip_vs_service->addr);
	service.port = bswap_16(ip_vs_service->port);
	service.protocol = ip_vs_service->protocol;

	/* check if service exist */
	if (internal_db_get_service(&service, false) == ALVS_DB_FAILURE) {
		write_log(LOG_NOTICE, "Server %s:%d (prot=%d) is not exist", my_inet_ntoa(service.ip), service.port, service.protocol);
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Get list of servers to delete */
	internal_db_get_server_count(&service, &server_count, 0);
	if (internal_db_get_server_list(&service, &server_list, 0) != ALVS_DB_OK) {
		/* Can't retrieve server list */
		write_log(LOG_CRIT, "Can't retrieve server list - internal error");
		return ALVS_DB_INTERNAL_ERROR;
	}

	if (server_count > 0) {
		write_log(LOG_INFO, "Statistics for Servers of Service %s:%d (protocol=%d):", my_inet_ntoa(service.ip), service.port, service.protocol);
		write_log(LOG_INFO, "                            In Pkt    In Byte   Out Pkt   Out Byte  ConSched ConTotal ActiveCon InactiveCon");
	}

	/* save all the counter of the servers that relate to this service */
	for (i = 0; i < server_count; i++) {
		get_counters_rc = alvs_db_get_server_counters(server_list->server.nps_index,
							      &server_counters.server_stats,
							      true);

		if (get_counters_rc != ALVS_DB_OK) {
			write_log(LOG_CRIT, "Error reading service posted counters");
			alvs_free_server_list(server_list);
			return get_counters_rc;
		}

		/* print statistics to syslog (print the difference between real posted counters to internal db counters values) */
		write_log(LOG_INFO,
			"Server %16s:%-3d  %-7lu | %-7lu | %-7lu | %-7lu | %-6lu | %-6lu | %-7lu | %-6lu",
			my_inet_ntoa(server_list->server.ip),
			server_list->server.port,
			server_counters.server_stats.in_packet - server_list->server.server_stats.in_packet,
			server_counters.server_stats.in_byte - server_list->server.server_stats.in_byte,
			server_counters.server_stats.out_packet - server_list->server.server_stats.out_packet,
			server_counters.server_stats.out_byte - server_list->server.server_stats.out_byte,
			server_counters.server_stats.connection_scheduled - server_list->server.server_stats.connection_scheduled,
			server_counters.server_stats.connection_total,
			server_counters.server_stats.active_connection,
			server_counters.server_stats.inactive_connection);

		/* move to the next server on this service */
		server_list = server_list->next;
	}

	alvs_free_server_list(server_list);

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       API to print service statistics
 *
 * \return      ALVS_DB_OK - operation succeeded
 *              ALVS_DB_NPS_ERROR - fail to read nps counters
 */
enum alvs_db_rc alvs_db_print_services_stats(void)
{
	int rc;
	char sql[256];
	sqlite3_stmt *statement;
	struct alvs_db_service service_internal_db_stats;
	struct alvs_db_service service_counters;

	memset(&service_counters, 0, sizeof(service_counters));

	/* Prepare SQL statement */
	sprintf(sql, "SELECT * FROM services");
	rc = sqlite3_prepare_v2(alvs_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s", sqlite3_errmsg(alvs_db));
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	if (rc == SQLITE_ROW) {
		write_log(LOG_INFO, "Statistics for Services:");
		write_log(LOG_INFO,
			"                        In Pkt       In Byte      Out Pkt      Out Byte     Con Sched");
	}

	/* Collect server ids */
	while (rc == SQLITE_ROW) {
		service_internal_db_stats.ip = sqlite3_column_int(statement, TABLE_ENTRY_IP);
		service_internal_db_stats.port = sqlite3_column_int(statement, TABLE_ENTRY_PORT);
		service_internal_db_stats.protocol = sqlite3_column_int(statement, TABLE_ENTRY_PRTOTOCOL);
		service_internal_db_stats.nps_index = sqlite3_column_int(statement, TABLE_ENTRY_NPS_INDEX);
		service_internal_db_stats.service_stats.in_packet = sqlite3_column_int64(statement, TABLE_ENTRY_IN_PACKET);
		service_internal_db_stats.service_stats.in_byte = sqlite3_column_int64(statement, TABLE_ENTRY_IN_BYTE);
		service_internal_db_stats.service_stats.out_packet = sqlite3_column_int64(statement, TABLE_ENTRY_OUT_PACKET);
		service_internal_db_stats.service_stats.out_byte = sqlite3_column_int64(statement, TABLE_ENTRY_OUT_BYTE);
		service_internal_db_stats.service_stats.connection_scheduled = sqlite3_column_int64(statement, TABLE_ENTRY_CONNECTION_SCHEDULD);

		/* read counter */
		if (alvs_db_get_service_counters(service_internal_db_stats.nps_index,
						 &service_counters.service_stats) != ALVS_DB_OK) {
			return ALVS_DB_NPS_ERROR;
		}

		write_log(LOG_INFO, "%s:%d (prot=%d) %-10lu | %-10lu | %-10lu | %-10lu | %-10lu ",
			my_inet_ntoa(service_internal_db_stats.ip),
			service_internal_db_stats.port,
			service_internal_db_stats.protocol,
			service_counters.service_stats.in_packet - service_internal_db_stats.service_stats.in_packet,
			service_counters.service_stats.in_byte - service_internal_db_stats.service_stats.in_byte,
			service_counters.service_stats.out_packet - service_internal_db_stats.service_stats.out_packet,
			service_counters.service_stats.out_byte - service_internal_db_stats.service_stats.out_byte,
			service_counters.service_stats.connection_scheduled - service_internal_db_stats.service_stats.connection_scheduled);

		/* move to the next service on internal db */
		rc = sqlite3_step(statement);

	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       API to clear all servers and services statistics
 *
 * \return      ALVS_DB_OK - operation succeeded
 *              ALVS_DB_INTERNAL_ERROR - received an error from internal DB
 *              ALVS_DB_NPS_ERROR - failed to update NPS DB
 */
enum alvs_db_rc alvs_db_clear_stats(void)
{
	int rc;
	char sql[256];
	uint32_t server_count, i;
	sqlite3_stmt *statement;
	struct alvs_db_service service;
	struct alvs_server_node *server_list;
	enum alvs_db_rc ret_rc;

	/* get all services from internal database */
	/* Prepare SQL statement */
	sprintf(sql, "SELECT * FROM services");
	rc = sqlite3_prepare_v2(alvs_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s",
			  sqlite3_errmsg(alvs_db));
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* Collect server ids */
	while (rc == SQLITE_ROW) {
		service.ip = sqlite3_column_int(statement, 0);
		service.port = sqlite3_column_int(statement, 1);
		service.protocol = sqlite3_column_int(statement, 2);
		service.nps_index = sqlite3_column_int(statement, 3);

		/*save statistics to internal database */
		if (internal_db_save_service_stats(service) != ALVS_DB_OK) {
			write_log(LOG_CRIT, "Failed to save service posted counters on internal db");
			return ALVS_DB_INTERNAL_ERROR;
		}

		/* Get list of servers to save */
		internal_db_get_server_count(&service, &server_count, 0);
		if (internal_db_get_server_list(&service, &server_list, 0) != ALVS_DB_OK) {
			/* Can't retrieve server list */
			write_log(LOG_CRIT, "Can't retrieve server list - internal error.");
			return ALVS_DB_INTERNAL_ERROR;
		}

		/* save all the counter of the servers that relate to this service */
		for (i = 0; i < server_count; i++) {

			ret_rc = internal_db_save_server_stats(&server_list->server);
			if (ret_rc != ALVS_DB_OK) {
				write_log(LOG_CRIT, "Failed to save server posted counters on internal db");
				alvs_free_server_list(server_list);
				return ret_rc;
			}
			/* move to the next server on this service */
			server_list = server_list->next;
		}

		alvs_free_server_list(server_list);

		/* move to the next service on internal db */
		rc = sqlite3_step(statement);
	}

	/* clear error stats */
	if (alvs_db_clean_error_stats() != ALVS_DB_OK) {
		return ALVS_DB_NPS_ERROR;
	}
	if (alvs_db_clean_interface_stats() != ALVS_DB_OK) {
		return ALVS_DB_NPS_ERROR;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       API to clear all servers and services from ALVS DBs
 *
 * \return      ALVS_DB_OK - operation succeeded
 *              ALVS_DB_INTERNAL_ERROR - received an error from internal DB
 *              ALVS_DB_NPS_ERROR - failed to update NPS DB
 */
enum alvs_db_rc alvs_db_clear(void)
{
	/* TODO - There is a CP bug here. delete_all_entries doesn't do anything for cached hashes
	 *        The workaround is to delete all entries in a loop.
	 *        Once CP will fix the bug we need to remove the workaround and instead use the following call:
	 *
	 * if (infra_delete_all_entries(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION) == false) {
		  write_log(LOG_CRIT, "Failed to delete all entries from service classification DB in NPS.");
		  return ALVS_DB_NPS_ERROR;
	 * }
	 */

	/* TODO - workaround starts here */
	uint32_t ind;
	uint32_t service_count;
	struct alvs_service_node *service_list;
	struct alvs_service_classification_key nps_service_classification_key;

	if (internal_db_get_service_count(&service_count) != ALVS_DB_OK) {
		/* Can't retrieve service list */
		write_log(LOG_CRIT, "Can't retrieve service list - "
			  "internal error.");
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Get list of servers to delete */
	if (internal_db_get_service_list(&service_list) != ALVS_DB_OK) {
		/* Can't retrieve service list */
		write_log(LOG_CRIT, "Can't retrieve service list - "
			  "internal error.");
		return ALVS_DB_INTERNAL_ERROR;
	}

	for (ind = 0; ind < service_count; ind++) {
		write_log(LOG_DEBUG, "Deleting service with nps_index %d.", service_list->service.nps_index);
		build_nps_service_classification_key(&service_list->service,
						     &nps_service_classification_key);
		if (infra_delete_entry(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION,
				&nps_service_classification_key,
				sizeof(struct alvs_service_classification_key)) == false) {
			write_log(LOG_CRIT, "Failed to delete service classification entry.");
			return ALVS_DB_NPS_ERROR;
		}
		service_list = service_list->next;
	}

	alvs_free_service_list(service_list);
	/* TODO - workaround ends here */


	if (infra_delete_all_entries(STRUCT_ID_ALVS_SERVICE_INFO) == false) {
		write_log(LOG_CRIT, "Failed to delete all entries from service info DB in NPS.");
		return ALVS_DB_NPS_ERROR;
	}

	if (infra_delete_all_entries(STRUCT_ID_ALVS_SCHED_INFO) == false) {
		write_log(LOG_CRIT, "Failed to delete all entries from scheduling info DB in NPS.");
		return ALVS_DB_NPS_ERROR;
	}

	if (internal_db_clear_all() == ALVS_DB_INTERNAL_ERROR) {
		write_log(LOG_CRIT, "Failed to delete all services in internal DB.");
		return ALVS_DB_INTERNAL_ERROR;
	}
	index_pool_rewind(&service_index_pool);
	write_log(LOG_DEBUG, "Internal DB cleared.");

	write_log(LOG_INFO, "ALVS DBs cleared successfully.");
	return ALVS_DB_OK;
}

/******************************************************************************
 * \brief    Raises SIGTERM signal to main thread and exits the thread.
 *           Deletes the DB manager.
 *
 * \return   void
 */
void server_db_exit_with_error(void)
{
	*alvs_db_cancel_application_flag_ptr = true;
	kill(getpid(), SIGTERM);
	pthread_exit(NULL);
}

void server_db_aging(void)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[256];
	struct alvs_db_service cp_service;
	struct alvs_db_server cp_server;
	struct alvs_server_info_key nps_server_info_key;
	sigset_t sigs_to_block;
	uint64_t value;

	sigemptyset(&sigs_to_block);
	sigaddset(&sigs_to_block, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &sigs_to_block, NULL);

	write_log(LOG_DEBUG, "Start aging thread");

	sprintf(sql, "SELECT * FROM servers "
		"WHERE active=0;");

	while (!(*alvs_db_cancel_application_flag_ptr)) {
		sleep(1);
		/* Prepare SQL statement */
		rc = sqlite3_prepare_v2(alvs_db, sql, -1, &statement, NULL);
		if (rc != SQLITE_OK) {
			write_log(LOG_CRIT, "SQL error: %s",
				  sqlite3_errmsg(alvs_db));
			server_db_exit_with_error();
		}

		/* Execute SQL statement */
		rc = sqlite3_step(statement);

		/* Collect server ids */
		while (rc == SQLITE_ROW) {
			if (alvs_db_get_num_conn_total(sqlite3_column_int(statement, 5), &value) == ALVS_DB_INTERNAL_ERROR) {
				write_log(LOG_CRIT, "Delete server failed in aging thread");
				server_db_exit_with_error();
			}
			if (value == 0) {
				cp_server.ip = sqlite3_column_int(statement, 0);
				cp_server.port = sqlite3_column_int(statement, 1);
				cp_service.ip = sqlite3_column_int(statement, 2);
				cp_service.port = sqlite3_column_int(statement, 3);
				cp_service.protocol = sqlite3_column_int(statement, 4);
				cp_server.nps_index = sqlite3_column_int(statement, 5);

				write_log(LOG_DEBUG, "Aging server %s:%d (index=%d) in service %s:%d (prot=%d)",
					  my_inet_ntoa(cp_server.ip), cp_server.port,
					  cp_server.nps_index, my_inet_ntoa(cp_service.ip),
					  cp_service.port, cp_service.protocol);

				/* Delete server from internal DB */
				if (internal_db_delete_server(&cp_service, &cp_server) == ALVS_DB_INTERNAL_ERROR) {
					write_log(LOG_CRIT, "Delete server from internal DB failed in aging thread");
					server_db_exit_with_error();
				}

				/* Delete server from NPS DB */
				build_nps_server_info_key(&cp_server, &nps_server_info_key);
				if (infra_delete_entry(STRUCT_ID_ALVS_SERVER_INFO,
						       &nps_server_info_key,
						       sizeof(struct alvs_server_info_key)) == false) {
					write_log(LOG_CRIT, "Delete server from NPS DB failed in aging thread");
					server_db_exit_with_error();
				}

				/* Release server index from index pool */
				write_log(LOG_DEBUG, "Releasing server index %d", cp_server.nps_index);
				index_pool_release(&server_index_pool, cp_server.nps_index);
			}

			rc = sqlite3_step(statement);
		}

		/* Error */
		if (rc < SQLITE_ROW) {
			write_log(LOG_CRIT, "SQL error: %s",
				  sqlite3_errmsg(alvs_db));
			sqlite3_finalize(statement);
			server_db_exit_with_error();
		}

		/* finalize SQL statement */
		sqlite3_finalize(statement);
	}
}

/**************************************************************************//**
 * \brief       print global error stats to syslog (global counters)
 *
 * \return	ALVS_DB_OK - - operation succeeded
 *		ALVS_DB_NPS_ERROR - fail to read statistics
 */
enum alvs_db_rc alvs_db_print_global_error_stats(void)
{
	uint64_t global_error_counters[ALVS_NUM_OF_ALVS_ERROR_STATS] = {0};
	uint64_t temp_sum;
	uint32_t error_index;

	/* printing general error stats */
	write_log(LOG_INFO, "Statistics of global errors");
	if (infra_get_posted_counters(EMEM_ALVS_ERROR_STATS_POSTED_OFFSET,
					ALVS_NUM_OF_ALVS_ERROR_STATS,
					global_error_counters) == false) {
		write_log(LOG_CRIT, "Failed to read error statistics counters");
		return ALVS_DB_NPS_ERROR;
	}
	temp_sum = 0;
	for (error_index = 0; error_index < ALVS_NUM_OF_ALVS_ERROR_STATS; error_index++) {
		if (global_error_counters[error_index] > 0) {
			if (alvs_error_stats_offsets_names[error_index] != NULL) {
				write_log(LOG_INFO, "    %s Counter: %-20lu",
					  alvs_error_stats_offsets_names[error_index],
					  global_error_counters[error_index]);
			} else {
				write_log(LOG_ERR, "    Problem printing statistics for error type %d",
					  error_index);
			}
		}
		temp_sum += global_error_counters[error_index];
	}
	if (temp_sum == 0) {
		write_log(LOG_INFO, "    No Errors On Global Counters");
	}

	return ALVS_DB_OK;
}

