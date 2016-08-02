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
#include "log.h"

#include <EZapiStat.h>
#include <EZapiPrm.h>
#include "alvs_db.h"
#include "sqlite3.h"
#include "stack.h"
#include "defs.h"
#include "infrastructure.h"

/* Global pointer to the DB */
sqlite3 *alvs_db;
struct index_pool server_index_pool;
struct index_pool service_index_pool;
pthread_t server_db_aging_thread;
bool *alvs_db_cancel_application_flag_ptr;

void server_db_aging(void);

struct alvs_db_service {
	in_addr_t                  ip;
	uint16_t                   port;
	uint16_t                   protocol;
	uint32_t                   nps_index;
	uint8_t                    flags;
	enum alvs_scheduler_type   sched_alg;
	struct ezdp_sum_addr       stats_base;
	uint16_t                   sched_entries_count;
};

struct alvs_db_server {
	in_addr_t                ip;
	uint16_t                 port;
	uint16_t                 weight;
	uint8_t                  active;
	uint32_t                 nps_index;
	uint32_t                 conn_flags;
	uint8_t                 server_flags;
	enum alvs_routing_type   routing_alg;
	uint16_t                 u_thresh;
	uint16_t                 l_thresh;
	struct ezdp_sum_addr     server_stats_base;
	struct ezdp_sum_addr     service_stats_base;
	struct ezdp_sum_addr     server_on_demand_stats_base;
	struct ezdp_sum_addr     server_flags_dp_base;
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

#define ALVS_DB_FILE_NAME "alvs.db"

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
		"ip INT NOT NULL,"
		"port INT NOT NULL,"
		"protocol INT NOT NULL,"
		"nps_index INT NOT NULL,"
		"flags INT NOT NULL,"
		"sched_alg INT NOT NULL,"
		"stats_base INT NOT NULL,"
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
	 *    routing algorithm
	 *    active (boolean)
	 *    statistics base
	 */
	sql = "CREATE TABLE servers("
		"ip INT NOT NULL,"
		"port INT NOT NULL,"
		"srv_ip INT NOT NULL,"
		"srv_port INT NOT NULL,"
		"srv_protocol INT NOT NULL,"
		"nps_index INT NOT NULL,"
		"weight INT NOT NULL,"
		"conn_flags INT NOT NULL,"
		"server_flags INT NOT NULL,"
		"routing_alg INT NOT NULL,"
		"u_thresh INT NOT NULL,"
		"l_thresh INT NOT NULL,"
		"active BOOLEAN NOT NULL,"
		"server_stats_base INT NOT NULL,"
		"service_stats_base INT NOT NULL,"
		"server_on_demand_stats_base INT NOT NULL,"
		"server_flags_dp_base INT NOT NULL,"
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
			  (uint32_t)EMEM_SERVER_FLAGS_OFFSET_CP + cp_server->nps_index * sizeof(server_flags));

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
		service->nps_index = sqlite3_column_int(statement, 3);
		service->flags = sqlite3_column_int(statement, 4);
		service->sched_alg = (enum alvs_scheduler_type)sqlite3_column_int(statement, 5);
		service->stats_base.raw_data = sqlite3_column_int(statement, 6);
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
		"(ip, port, protocol, nps_index, flags, sched_alg, stats_base) "
		"VALUES (%d, %d, %d, %d, %d, %d, %d);",
		service->ip, service->port, service->protocol, service->nps_index,
		service->flags, service->sched_alg, service->stats_base.raw_data);

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
		"SET flags=%d, sched_alg=%d "
		"WHERE ip=%d AND port=%d AND protocol=%d;",
		service->flags, service->sched_alg,
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
		node->server.nps_index = sqlite3_column_int(statement, 5);
		node->server.weight = sqlite3_column_int(statement, 6);
		node->server.conn_flags = sqlite3_column_int(statement, 7);
		node->server.server_flags = sqlite3_column_int(statement, 8);
		node->server.routing_alg = (enum alvs_routing_type)sqlite3_column_int(statement, 9);
		node->server.u_thresh = sqlite3_column_int(statement, 10);
		node->server.l_thresh = sqlite3_column_int(statement, 11);
		node->server.active = sqlite3_column_int(statement, 12);
		node->server.server_stats_base.raw_data = sqlite3_column_int(statement, 13);
		node->server.service_stats_base.raw_data = sqlite3_column_int(statement, 14);
		node->server.server_on_demand_stats_base.raw_data = sqlite3_column_int(statement, 15);
		node->server.server_flags_dp_base.raw_data = sqlite3_column_int(statement, 16);
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
	server->routing_alg = (enum alvs_routing_type)sqlite3_column_int(statement, 9);
	server->u_thresh = sqlite3_column_int(statement, 10);
	server->l_thresh = sqlite3_column_int(statement, 11);
	server->active = sqlite3_column_int(statement, 12);
	server->server_stats_base.raw_data = sqlite3_column_int(statement, 13);
	server->service_stats_base.raw_data = sqlite3_column_int(statement, 14);
	server->server_on_demand_stats_base.raw_data = sqlite3_column_int(statement, 15);
	server->server_flags_dp_base.raw_data = sqlite3_column_int(statement, 16);
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
		"(ip, port, srv_ip, srv_port, srv_protocol, nps_index, weight, conn_flags, server_flags, routing_alg, u_thresh, l_thresh, active, server_stats_base, service_stats_base, server_on_demand_stats_base,server_flags_dp_base) "
		"VALUES (%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d);",
		server->ip, server->port, service->ip, service->port,
		service->protocol, server->nps_index,
		server->weight, server->conn_flags, server->server_flags,
		server->routing_alg, server->u_thresh, server->l_thresh,
		server->active, server->server_stats_base.raw_data, server->service_stats_base.raw_data,
		server->server_on_demand_stats_base.raw_data,
		server->server_flags_dp_base.raw_data);

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
		"SET weight=%d, conn_flags=%d, server_flags=%d, routing_alg=%d, "
		"u_thresh=%d, l_thresh=%d "
		"WHERE srv_ip=%d AND srv_port=%d AND srv_protocol=%d "
		"AND ip=%d AND port=%d;",
		server->weight, server->conn_flags, server->server_flags,
		server->routing_alg, server->u_thresh, server->l_thresh,
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
		"u_thresh=%d, l_thresh=%d, routing_alg=%d, conn_flags=%d "
		"WHERE srv_ip=%d AND srv_port=%d AND srv_protocol=%d "
		"AND ip=%d AND port=%d;",
		server->weight, IP_VS_DEST_F_AVAILABLE,
		server->u_thresh, server->l_thresh,
		server->routing_alg, server->conn_flags,
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
 * \param[in]   server_count  - number of servers in list
 *
 */
void alvs_db_sh_fill_buckets(struct alvs_db_server *bucket[], struct alvs_server_node *server_list, uint32_t server_count)
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
			alvs_db_sh_fill_buckets(servers_buckets, server_list, server_count);
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
 * \brief       Get number of active connections from a statistics counter
 *
 * \param[in]   server_index   - index of server
 *
 * \return      number of active connections
 */
enum alvs_db_rc alvs_db_get_num_active_conn(uint32_t server_index,
					    uint64_t *active_conn)
{
	union {
		uint64_t raw_data;
		struct {
			uint32_t value_msb;
			uint32_t value_lsb;
		};
	} value;

	EZstatus ret_val;
	EZapiStat_PostedCounter posted_counter;
	EZapiStat_PostedCounterConfig posted_counter_config;

	memset(&posted_counter_config, 0, sizeof(posted_counter_config));
	memset(&posted_counter, 0, sizeof(posted_counter));

	posted_counter_config.uiPartition = 0;
	posted_counter_config.pasCounters = &posted_counter;
	posted_counter_config.bRange = false;
	posted_counter_config.uiStartCounter = EMEM_SERVER_STATS_POSTED_OFFSET + (server_index * ALVS_NUM_OF_SERVER_STATS) + ALVS_SERVER_STATS_ACTIVE_CONN_OFFSET;
	posted_counter_config.uiNumCounters = 1;
	posted_counter_config.bDoubleOperation = false;
	ret_val = EZapiStat_Status(0, EZapiStat_StatCmd_GetPostedCounters, &posted_counter_config);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Config: EZapiStat_StatCmd_GetPostedCounters failed.");
		return ALVS_DB_INTERNAL_ERROR;
	}

	value.value_lsb = posted_counter.uiByteValue;
	value.value_msb = posted_counter.uiByteValueMSB;
	*active_conn = value.raw_data;

	return ALVS_DB_OK;
}


/**************************************************************************//**
 * \brief       Clear all statistic counter of a server
 *
 * \param[in]   server_index   - index of server
 *
 * \return      true if operation succeeded
 */
enum alvs_db_rc alvs_db_clean_server_stats(uint32_t server_index)
{
	EZstatus ret_val;
	EZapiStat_PostedCounter posted_counter;
	EZapiStat_PostedCounterConfig posted_counter_config;
	EZapiStat_LongCounterConfig long_counter_config;
	EZapiStat_LongCounter long_counter;

	memset(&long_counter_config, 0, sizeof(long_counter_config));
	memset(&long_counter, 0, sizeof(long_counter));

	long_counter_config.uiPartition = 0;
	long_counter_config.bRange = true;
	long_counter_config.uiStartCounter = EMEM_SERVER_STATS_ON_DEMAND_OFFSET + server_index * ALVS_NUM_OF_SERVERS_ON_DEMAND_STATS;
	long_counter_config.uiNumCounters = ALVS_NUM_OF_SERVERS_ON_DEMAND_STATS;
	long_counter_config.pasCounters = &long_counter;
	ret_val = EZapiStat_Config(0, EZapiStat_ConfigCmd_SetLongCounterValues, &long_counter_config);

	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Config: EZapiStat_ConfigCmd_SetLongCounters failed.");
		return ALVS_DB_INTERNAL_ERROR;
	}

	memset(&posted_counter_config, 0, sizeof(posted_counter_config));
	memset(&posted_counter, 0, sizeof(posted_counter));

	posted_counter_config.uiPartition = 0;
	posted_counter_config.pasCounters = &posted_counter;
	posted_counter_config.bRange = true;
	posted_counter_config.uiStartCounter = EMEM_SERVER_STATS_POSTED_OFFSET + server_index * ALVS_NUM_OF_SERVER_STATS;
	posted_counter_config.uiNumCounters = ALVS_NUM_OF_SERVER_STATS;
	ret_val = EZapiStat_Config(0, EZapiStat_ConfigCmd_SetPostedCounters, &posted_counter_config);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Config: EZapiStat_ConfigCmd_SetPostedCounters failed.");
		return ALVS_DB_INTERNAL_ERROR;
	}

	return ALVS_DB_OK;
}


/**************************************************************************//**
 * \brief       Clear all statistic counter of a service
 *
 * \param[in]   service_index   - index of service
 *
 * \return      true if operation succeeded
 */
enum alvs_db_rc alvs_db_clean_service_stats(uint32_t service_index)
{
	EZstatus ret_val;
	EZapiStat_PostedCounter posted_counter;
	EZapiStat_PostedCounterConfig posted_counter_config;

	memset(&posted_counter_config, 0, sizeof(posted_counter_config));
	memset(&posted_counter, 0, sizeof(posted_counter));

	posted_counter_config.uiPartition = 0;
	posted_counter_config.pasCounters = &posted_counter;
	posted_counter_config.bRange = true;
	posted_counter_config.uiStartCounter = EMEM_SERVICE_STATS_POSTED_OFFSET + service_index * ALVS_NUM_OF_SERVICE_STATS;
	posted_counter_config.uiNumCounters = ALVS_NUM_OF_SERVICE_STATS;
	ret_val = EZapiStat_Config(0, EZapiStat_ConfigCmd_SetPostedCounters, &posted_counter_config);
	if (EZrc_IS_ERROR(ret_val)) {
		write_log(LOG_CRIT, "EZapiStat_Config: EZapiStat_ConfigCmd_SetPostedCounters failed.");
		return ALVS_DB_INTERNAL_ERROR;
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
 * \brief       translate connection flags to routing type
 *
 * \param[in]   conn_flags   - connection flags
 *
 * \return      routing type
 */
enum alvs_routing_type get_routing_alg(uint32_t conn_flags)
{
	switch (conn_flags & IP_VS_CONN_F_FWD_MASK) {
	case IP_VS_CONN_F_MASQ:
		return ALVS_NAT_ROUTING;
	case IP_VS_CONN_F_TUNNEL:
		return ALVS_TUNNEL_ROUTING;
	case IP_VS_CONN_F_DROUTE:
		return ALVS_DIRECT_ROUTING;
	default:
		return ALVS_ROUTING_LAST;
	}
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
 * \param[in]   routing_alg   - received routing algorithm
 *
 * \return      true/false
 */
bool supported_routing_alg(enum alvs_routing_type routing_alg)
{
	if (routing_alg == ALVS_DIRECT_ROUTING) {
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
	nps_server_info_result->routing_alg = cp_server->routing_alg;
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

	/* Modify service information in internal DB */
	if (internal_db_modify_service(&cp_service) == ALVS_DB_INTERNAL_ERROR) {
		write_log(LOG_CRIT, "Failed to modify service in internal DB.");
		return ALVS_DB_INTERNAL_ERROR;
	}

	if (prev_sched_alg != cp_service.sched_alg) {
		/* Recalculate scheduling information */
		enum alvs_db_rc rc = alvs_db_recalculate_scheduling_info(&cp_service);

		if (rc != ALVS_DB_OK) {
			write_log(LOG_CRIT, "Failed to recalculate scheduling information.");
			return rc;
		}
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
	uint32_t server_count;
	struct alvs_db_service cp_service;
	struct alvs_db_server cp_server;
	struct alvs_server_info_key nps_server_info_key;
	struct alvs_server_info_result nps_server_info_result;
	struct alvs_service_info_key nps_service_info_key;
	struct alvs_service_info_result nps_service_info_result;

	/* Check if request is supported */
	if (supported_routing_alg(get_routing_alg(ip_vs_dest->conn_flags)) == false) {
		write_log(LOG_NOTICE, "Routing algorithm (%d) is not supported.", get_routing_alg(ip_vs_dest->conn_flags));
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
		cp_server.routing_alg = get_routing_alg(ip_vs_dest->conn_flags);
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
		cp_server.routing_alg = get_routing_alg(ip_vs_dest->conn_flags);
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

		write_log(LOG_DEBUG, "Server info: alg=%d, conn_flags=%d, server_flags=%d, weight=%d, u_thresh=%d, l_thresh=%d.\n",
			  cp_server.routing_alg, cp_server.conn_flags,
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
	if (supported_routing_alg(get_routing_alg(ip_vs_dest->conn_flags)) == false) {
		write_log(LOG_NOTICE, "Routing algorithm (%d) is not supported.", get_routing_alg(ip_vs_dest->conn_flags));
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
	cp_server.routing_alg = get_routing_alg(ip_vs_dest->conn_flags);

	if ((ip_vs_dest->u_threshold == 0) || (ip_vs_dest->u_threshold > cp_server.u_thresh)) {
		if (alvs_clear_overloaded_flag_of_server(&cp_server) == ALVS_DB_INTERNAL_ERROR) {
			write_log(LOG_CRIT, "Failed to alvs_clear_overloaded_flag_of_server.");
			return ALVS_DB_INTERNAL_ERROR;
		}
	}
	cp_server.u_thresh = ip_vs_dest->u_threshold;
	cp_server.l_thresh = ip_vs_dest->l_threshold;

	write_log(LOG_DEBUG, "Server info: alg=%d, conn_flags=%d, server_flags=%d, weight=%d, u_thresh=%d, l_thresh=%d.\n",
		  cp_server.routing_alg, cp_server.conn_flags,
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

	write_log(LOG_INFO, "Server (%s:%d) deleted successfully.",
		  my_inet_ntoa(cp_server.ip), cp_server.port);
	return ALVS_DB_OK;
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
			if (alvs_db_get_num_active_conn(sqlite3_column_int(statement, 5), &value) == ALVS_DB_INTERNAL_ERROR) {
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

				write_log(LOG_DEBUG, "Aging server %s:%d (index=%d) in service %s:%d (protocol=%d)",
					  my_inet_ntoa(cp_server.ip), cp_server.port,
					  cp_server.nps_index, my_inet_ntoa(cp_service.ip),
					  cp_service.port, cp_service.protocol);

				if (internal_db_delete_server(&cp_service, &cp_server) == ALVS_DB_INTERNAL_ERROR) {
					write_log(LOG_CRIT, "Delete server failed in aging thread");
					server_db_exit_with_error();
				}

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
