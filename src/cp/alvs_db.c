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
#include "log.h"

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
	uint32_t                   server_count;
};

struct alvs_db_server {
	in_addr_t                ip;
	uint16_t                 port;
	uint8_t                  weight;
	uint8_t                  active;
	uint32_t                 nps_index;
	uint32_t                 conn_flags;
	uint32_t                 server_flags;
	enum alvs_routing_type   routing_alg;
	uint32_t                 u_thresh;
	uint32_t                 l_thresh;
	struct ezdp_sum_addr     stats_base;
};

#define ALVS_DB_FILE_NAME "alvs.db"

enum alvs_db_rc alvs_db_init(bool *cancel_application_flag)
{
	int rc;
	char *sql;
	char *zErrMsg = NULL;

	if (index_pool_init(&server_index_pool, 32768) == false) {
		write_log(LOG_CRIT, "Failed to init server index pool.\n");
		return ALVS_DB_INTERNAL_ERROR;
	}
	if (index_pool_init(&service_index_pool, 256) == false) {
		index_pool_destroy(&server_index_pool);
		write_log(LOG_CRIT, "Failed to init service index pool.\n");
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Delete existing DB file */
	remove(ALVS_DB_FILE_NAME);

	/* Open the DB file */

	rc = sqlite3_open(ALVS_DB_FILE_NAME, &alvs_db);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "Can't open database: %s\n",
			  sqlite3_errmsg(alvs_db));
		index_pool_destroy(&server_index_pool);
		index_pool_destroy(&service_index_pool);
		return ALVS_DB_INTERNAL_ERROR;
	}

	write_log(LOG_DEBUG, "ALVS_DB: Opened database successfully\n");

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
		write_log(LOG_CRIT, "SQL error: %s\n", zErrMsg);
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
		"stats_base INT NOT NULL,"
		"PRIMARY KEY (ip,port,srv_ip,srv_port,srv_protocol),"
		"FOREIGN KEY (srv_ip,srv_port,srv_protocol) "
		"REFERENCES services(ip,port,protocol));";

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s\n", zErrMsg);
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
		write_log(LOG_CRIT, "Cannot start server_db_aging_thread: pthread_create failed for server DB aging.\n");
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

/**************************************************************************//**
 * \brief       Get number of active servers assigned to a service
 *
 * \param[in]   service          - reference to the service
 * \param[out]  server_count     - number of active servers
 *
 * \return      ALVS_DB_OK - service exists (info is filled with service
 *                           information if it not NULL).
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum alvs_db_rc internal_db_get_server_count(struct alvs_db_service *service,
					     uint32_t *server_count)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[256];

	sprintf(sql, "SELECT COUNT (nps_index) AS server_count FROM servers "
		"WHERE srv_ip=%d AND srv_port=%d AND srv_protocol=%d "
		"AND active=1;",
		service->ip, service->port, service->protocol);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(alvs_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s\n",
			  sqlite3_errmsg(alvs_db));
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* In case of error return 0 */
	if (rc != SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s\n",
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
 * \brief       Delete all services, set all servers to inactive.
 *
 * \return      ALVS_DB_OK - all was cleared
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum alvs_db_rc internal_db_clear_all(void)
{
	int rc;
	char *sql;
	char *zErrMsg = NULL;

	sql = "DELETE FROM services;"
	      "UPDATE servers SET active=0;";

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s\n", zErrMsg);
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
	uint32_t server_count;
	char sql[256];

	sprintf(sql, "SELECT * FROM services "
		"WHERE ip=%d AND port=%d AND protocol=%d;",
		service->ip, service->port, service->protocol);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(alvs_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s\n",
			  sqlite3_errmsg(alvs_db));
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s\n",
			  sqlite3_errmsg(alvs_db));
		sqlite3_finalize(statement);
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Service not found */
	if (rc == SQLITE_DONE) {
		write_log(LOG_DEBUG,
			  "No service found with required 3-tuple.\n");
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

	if (full_service) {
		if (internal_db_get_server_count(service, &server_count) == ALVS_DB_INTERNAL_ERROR) {
			return ALVS_DB_INTERNAL_ERROR;
		}
		service->server_count = server_count;
	}

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
		write_log(LOG_CRIT, "SQL error: %s\n", zErrMsg);
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
		write_log(LOG_CRIT, "SQL error: %s\n", zErrMsg);
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
		"UPDATE servers SET active=0 "
		"WHERE srv_ip=%d AND srv_port=%d AND srv_protocol=%d;",
		service->ip, service->port, service->protocol,
		service->ip, service->port, service->protocol);

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		return ALVS_DB_INTERNAL_ERROR;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Get All active servers assigned to a service
 *
 * \param[in]   service          - reference to the service
 * \param[out]  server_info      - array of the servers (maximum 256)
 *
 * \return      ALVS_DB_OK - operation succeeded.
 *              ALVS_DB_INTERNAL_ERROR - failed to communicate with DB
 */
enum alvs_db_rc internal_db_get_server_list(struct alvs_db_service *service,
					    struct alvs_db_server *server_info)
{
	int rc;
	sqlite3_stmt *statement;
	char sql[256];

	sprintf(sql, "SELECT * FROM servers "
		"WHERE srv_ip=%d AND srv_port=%d AND srv_protocol=%d "
		"AND active=1;",
		service->ip, service->port, service->protocol);

	/* Prepare SQL statement */
	rc = sqlite3_prepare_v2(alvs_db, sql, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s\n",
			  sqlite3_errmsg(alvs_db));
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* Collect server ids */
	while (rc == SQLITE_ROW) {
		server_info->nps_index = sqlite3_column_int(statement, 5);
		server_info->weight = sqlite3_column_int(statement, 6);
		server_info->conn_flags = sqlite3_column_int(statement, 7);
		server_info->server_flags = sqlite3_column_int(statement, 8);
		server_info->routing_alg = (enum alvs_routing_type)sqlite3_column_int(statement, 9);
		server_info->u_thresh = sqlite3_column_int(statement, 10);
		server_info->l_thresh = sqlite3_column_int(statement, 11);
		server_info->active = sqlite3_column_int(statement, 12);
		server_info->stats_base.raw_data = sqlite3_column_int(statement, 13);

		server_info++;
		rc = sqlite3_step(statement);
	}

	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s\n",
			  sqlite3_errmsg(alvs_db));
		sqlite3_finalize(statement);
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* finalize SQL statement */
	sqlite3_finalize(statement);

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
		write_log(LOG_CRIT, "SQL error: %s\n",
			  sqlite3_errmsg(alvs_db));
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Execute SQL statement */
	rc = sqlite3_step(statement);

	/* Error */
	if (rc < SQLITE_ROW) {
		write_log(LOG_CRIT, "SQL error: %s\n", sqlite3_errmsg(alvs_db));
		sqlite3_finalize(statement);
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Service not found */
	if (rc == SQLITE_DONE) {
		write_log(LOG_DEBUG, "No server found in required service "
			  "with required 2-tuple.\n");
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
	server->stats_base.raw_data = sqlite3_column_int(statement, 13);

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
	char sql[256];
	char *zErrMsg = NULL;

	sprintf(sql, "INSERT INTO servers "
		"(ip, port, srv_ip, srv_port, srv_protocol, nps_index, weight, conn_flags, server_flags, routing_alg, u_thresh, l_thresh, active, stats_base) "
		"VALUES (%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d);",
		server->ip, server->port, service->ip, service->port,
		service->protocol, server->nps_index,
		server->weight, server->conn_flags, server->server_flags,
		server->routing_alg, server->u_thresh, server->l_thresh,
		server->active, server->stats_base.raw_data);

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s\n", zErrMsg);
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
		write_log(LOG_CRIT, "SQL error: %s\n", zErrMsg);
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
		write_log(LOG_CRIT, "SQL error: %s\n", zErrMsg);
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
		"SET active=0 "
		"WHERE srv_ip=%d AND srv_port=%d AND srv_protocol=%d "
		"AND ip=%d AND port=%d;",
		service->ip, service->port, service->protocol,
		server->ip, server->port);

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s\n", zErrMsg);
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
		"SET active=1 "
		"WHERE srv_ip=%d AND srv_port=%d AND srv_protocol=%d "
		"AND ip=%d AND port=%d;",
		service->ip, service->port, service->protocol,
		server->ip, server->port);

	/* Execute SQL statement */
	rc = sqlite3_exec(alvs_db, sql, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK) {
		write_log(LOG_CRIT, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		return ALVS_DB_INTERNAL_ERROR;
	}

	return ALVS_DB_OK;
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
	uint32_t ind;
	uint32_t weight = 0;
	uint32_t server_index = 0;
	struct alvs_db_server server_list[ALVS_SIZE_OF_SCHED_BUCKET];
	struct alvs_sched_info_key sched_info_key;
	struct alvs_sched_info_result sched_info_result;

	write_log(LOG_DEBUG, "Recalculating scheduling info with %d servers.\n", service->server_count);
	if (service->server_count == 0) {
		for (ind = 0; ind < ALVS_SIZE_OF_SCHED_BUCKET; ind++) {
			sched_info_key.sched_index = bswap_16(service->nps_index * ALVS_SIZE_OF_SCHED_BUCKET + ind);
			if (infra_delete_entry(STRUCT_ID_ALVS_SCHED_INFO, &sched_info_key,
						sizeof(struct alvs_sched_info_key)) == false) {
				write_log(LOG_DEBUG, "Failed to modify a scheduling info entry.\n");
				return ALVS_DB_NPS_ERROR;
			}
		}
		return ALVS_DB_OK;
	}

	if (internal_db_get_server_list(service, server_list) != ALVS_DB_OK) {
		/* Can't retrieve server list */
		write_log(LOG_CRIT, "Can't retrieve server list - "
			  "internal error.\n");
		return ALVS_DB_INTERNAL_ERROR;
	}

	switch (service->sched_alg) {
	case ALVS_SOURCE_HASH_SCHEDULER:
		for (ind = 0; ind < ALVS_SIZE_OF_SCHED_BUCKET; ind++) {
			sched_info_key.sched_index = bswap_16(service->nps_index * ALVS_SIZE_OF_SCHED_BUCKET + ind);
			sched_info_result.server_index = bswap_32(server_list[server_index].nps_index);
			write_log(LOG_DEBUG, "%d --> %d\n", service->nps_index * ALVS_SIZE_OF_SCHED_BUCKET + ind, server_list[server_index].nps_index);
			if (infra_modify_entry(STRUCT_ID_ALVS_SCHED_INFO, &sched_info_key,
						sizeof(struct alvs_sched_info_key), &sched_info_result,
						sizeof(struct alvs_sched_info_result)) == false) {
				write_log(LOG_CRIT, "Failed to modify a scheduling info entry.\n");
				return ALVS_DB_NPS_ERROR;
			}

			weight++;
			if (weight >= server_list[server_index].weight) {
				weight = 0;
				server_index++;
				if (server_index == service->server_count) {
					server_index = 0;
				}
			}
		}
		break;
	default:
		/* Other algorithms are currently not supported */
		write_log(LOG_NOTICE, "Algorithm not supported.\n");
		return ALVS_DB_NOT_SUPPORTED;
	}

	return ALVS_DB_OK;
}

/**************************************************************************//**
 * \brief       Get number of active connections from a statistics counter
 *
 * \param[in]   server_stats_base   - address of statistics counter
 *
 * \return      currently returns only zero
 */
uint32_t get_num_active_conn(ezdp_sum_addr_t server_stats_base)
{
	return 0;
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
	nps_service_info_result->server_count = bswap_16(cp_service->server_count);
	nps_service_info_result->service_flags = bswap_32(cp_service->flags);
	nps_service_info_result->service_stats_base = bswap_32(cp_service->stats_base.raw_data);
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
	nps_service_classification_key->service_address = cp_service->ip;
	nps_service_classification_key->service_port = cp_service->port;
	nps_service_classification_key->service_protocol = cp_service->protocol;
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
	nps_server_info_result->server_flags = bswap_32(cp_server->server_flags);
	nps_server_info_result->server_ip = cp_server->ip;
	nps_server_info_result->server_port = cp_server->port;
	nps_server_info_result->server_stats_base = bswap_32(cp_server->stats_base.raw_data);
	nps_server_info_result->server_weight = bswap_16(cp_server->weight);
	nps_server_info_result->u_thresh = bswap_32(cp_server->u_thresh);
	nps_server_info_result->l_thresh = bswap_32(cp_server->l_thresh);
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
		write_log(LOG_NOTICE, "Protocol (%d) is not supported.\n", ip_vs_service->protocol);
		return ALVS_DB_NOT_SUPPORTED;
	}
	if (supported_sched_alg(get_sched_alg(ip_vs_service->sched_name)) == false) {
		write_log(LOG_NOTICE, "Scheduling algorithm (%s) is not supported.\n", ip_vs_service->sched_name);
		return ALVS_DB_NOT_SUPPORTED;
	}

	/* Check if service already exists in internal DB */
	cp_service.ip = ip_vs_service->addr;
	cp_service.port = ip_vs_service->port;
	cp_service.protocol = bswap_16(ip_vs_service->protocol);
	switch (internal_db_get_service(&cp_service, false)) {
	case ALVS_DB_OK:
		/* Service already exists */
		write_log(LOG_NOTICE, "Can't add service. Service (ip=0x%08x, port=%d, protocol=%d) already exists.\n",
		       cp_service.ip, cp_service.port, cp_service.protocol);
		return ALVS_DB_FAILURE;
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find service (internal error).\n");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_FAILURE:
		/* Service doesn't exist */
		write_log(LOG_DEBUG, "Service (ip=0x%08x, port=%d, protocol=%d) doesn't exist\n",
			  cp_service.ip, cp_service.port, cp_service.protocol);
		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Service doesn't exist
	 * Allocate an index for the new service
	 */
	if (index_pool_alloc(&service_index_pool, &cp_service.nps_index) == false) {
		write_log(LOG_ERR, "Can't add service. Reached maximum.\n");
		return ALVS_DB_NOT_SUPPORTED;
	}
	write_log(LOG_DEBUG, "Allocated nps_index = %d\n", cp_service.nps_index);

	/* Fill information of the service */
	cp_service.sched_alg = get_sched_alg(ip_vs_service->sched_name);
	cp_service.flags = ip_vs_service->flags;
	cp_service.server_count = 0;
	cp_service.stats_base.mem_type = EZDP_EXTERNAL_MS;
	cp_service.stats_base.msid = EMEM_SERVICE_STATS_POSTED_MSID;
	cp_service.stats_base.element_index = (EMEM_SERVICE_STATS_POSTED_OFFSET >> 3) + cp_service.nps_index * ALVS_NUM_OF_SERVICE_STATS;

	write_log(LOG_DEBUG, "Service info: alg=%d, flags=%d\n",
		  cp_service.sched_alg, cp_service.flags);

	/* Add service info to internal DB */
	if (internal_db_add_service(&cp_service) == ALVS_DB_INTERNAL_ERROR) {
		write_log(LOG_CRIT, "Failed to add service to internal DB.\n");
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
		write_log(LOG_CRIT, "Failed to add service info entry to NPS.\n");
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
		write_log(LOG_CRIT, "Failed to add service classification entry to NPS.\n");
		index_pool_release(&service_index_pool, cp_service.nps_index);
		return ALVS_DB_NPS_ERROR;
	}

	write_log(LOG_INFO, "Service added successfully.\n");
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
		write_log(LOG_NOTICE, "Scheduling algorithm (%s) is not supported.\n", ip_vs_service->sched_name);
		return ALVS_DB_NOT_SUPPORTED;
	}

	/* Check if service exists in internal DB */
	cp_service.ip = ip_vs_service->addr;
	cp_service.port = ip_vs_service->port;
	cp_service.protocol = bswap_16(ip_vs_service->protocol);
	switch (internal_db_get_service(&cp_service, true)) {
	case ALVS_DB_FAILURE:
		/* Service doesn't exist */
		write_log(LOG_NOTICE, "Can't modify service. Service (ip=0x%08x, port=%d, protocol=%d) doesn't exist.\n",
		       cp_service.ip, cp_service.port, cp_service.protocol);
		return ALVS_DB_FAILURE;
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find service (internal error).\n");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_OK:
		/* Service exists */
		write_log(LOG_DEBUG, "Service (ip=0x%08x, port=%d, protocol=%d) exists\n",
			  cp_service.ip, cp_service.port, cp_service.protocol);
		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Modify information of the service */
	prev_sched_alg = cp_service.sched_alg;
	cp_service.sched_alg = get_sched_alg(ip_vs_service->sched_name);
	cp_service.flags = ip_vs_service->flags;

	write_log(LOG_DEBUG, "Service info: alg=%d, flags=%d\n",
		  cp_service.sched_alg, cp_service.flags);

	/* Modify service information in internal DB */
	if (internal_db_modify_service(&cp_service) == ALVS_DB_INTERNAL_ERROR) {
		write_log(LOG_CRIT, "Failed to modify service in internal DB.\n");
		return ALVS_DB_INTERNAL_ERROR;
	}

	if (prev_sched_alg != cp_service.sched_alg && cp_service.server_count > 0) {
		/* Recalculate scheduling information */
		enum alvs_db_rc rc = alvs_db_recalculate_scheduling_info(&cp_service);

		if (rc == ALVS_DB_OK) {
			write_log(LOG_DEBUG, "Scheduling info recalculated.\n");
		} else {
			write_log(LOG_CRIT, "Failed to recalculate scheduling information.\n");
			return rc;
		}
	}

	/* Modify service information in NPS search structure */
	build_nps_service_info_key(&cp_service, &nps_service_info_key);
	build_nps_service_info_result(&cp_service, &nps_service_info_result);
	if (infra_modify_entry(STRUCT_ID_ALVS_SERVICE_INFO, &nps_service_info_key,
			       sizeof(struct alvs_service_info_key), &nps_service_info_result,
			       sizeof(struct alvs_service_info_result)) == false) {
		write_log(LOG_CRIT, "Failed to modify service info entry in NPS.\n");
		return ALVS_DB_NPS_ERROR;
	}

	write_log(LOG_INFO, "Service modified successfully.\n");
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
	struct alvs_db_service cp_service;
	struct alvs_service_info_key nps_service_info_key;
	struct alvs_service_classification_key nps_service_classification_key;

	/* Check if service exists in internal DB */
	cp_service.ip = ip_vs_service->addr;
	cp_service.port = ip_vs_service->port;
	cp_service.protocol = bswap_16(ip_vs_service->protocol);
	switch (internal_db_get_service(&cp_service, true)) {
	case ALVS_DB_FAILURE:
		/* Service doesn't exist */
		write_log(LOG_NOTICE, "Can't delete service. Service (ip=0x%08x, port=%d, protocol=%d) doesn't exist.\n",
		       cp_service.ip, cp_service.port, cp_service.protocol);
		return ALVS_DB_FAILURE;
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find service (internal error).\n");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_OK:
		/* Service exists */
		write_log(LOG_DEBUG, "Service (ip=0x%08x, port=%d, protocol=%d) exists\n",
			  cp_service.ip, cp_service.port, cp_service.protocol);
		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Delete service from internal DB */
	if (internal_db_delete_service(&cp_service) == ALVS_DB_INTERNAL_ERROR) {
		write_log(LOG_CRIT, "Failed to delete service from internal DB.\n");
		return ALVS_DB_INTERNAL_ERROR;
	}

	/* Release service index */
	write_log(LOG_DEBUG, "Releasing nps_index %d.\n", cp_service.nps_index);
	index_pool_release(&service_index_pool, cp_service.nps_index);

	/* Delete service classification to NPS search structure */
	build_nps_service_classification_key(&cp_service,
					     &nps_service_classification_key);
	if (infra_delete_entry(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION,
			       &nps_service_classification_key,
			       sizeof(struct alvs_service_classification_key)) == false) {
		write_log(LOG_CRIT, "Failed to delete service classification entry.\n");
		return ALVS_DB_NPS_ERROR;
	}

	/* Delete service info from NPS search structure */
	build_nps_service_info_key(&cp_service, &nps_service_info_key);
	if (infra_delete_entry(STRUCT_ID_ALVS_SERVICE_INFO,
			       &nps_service_info_key,
			       sizeof(struct alvs_service_info_key)) == false) {
		write_log(LOG_CRIT, "Failed to delete service info entry.\n");
		return ALVS_DB_NPS_ERROR;
	}

	if (cp_service.server_count > 0) {
		cp_service.server_count = 0;
		/* Delete scheduling information from NPS search structure (if existed) */
		write_log(LOG_DEBUG, "Deleting scheduling information.\n");
		enum alvs_db_rc rc = alvs_db_recalculate_scheduling_info(&cp_service);

		if (rc != ALVS_DB_OK) {
			write_log(LOG_CRIT, "Failed to delete scheduling information.\n");
			return rc;
		}
	}

	write_log(LOG_INFO, "Service deleted successfully.\n");
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
	cp_service.ip = ip_vs_service->addr;
	cp_service.port = ip_vs_service->port;
	cp_service.protocol = bswap_16(ip_vs_service->protocol);
	switch (internal_db_get_service(&cp_service, true)) {
	case ALVS_DB_FAILURE:
		/* Service doesn't exist */
		write_log(LOG_DEBUG, "Can't find service (ip=0x%08x, port=%d, protocol=%d).\n",
		       cp_service.ip, cp_service.port, cp_service.protocol);
		return ALVS_DB_FAILURE;
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find service (internal error).\n");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_OK:
		/* Service exists */
		write_log(LOG_DEBUG, "Service (ip=0x%08x, port=%d, protocol=%d) found\n",
			  cp_service.ip, cp_service.port, cp_service.protocol);
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
	struct alvs_db_service cp_service;
	struct alvs_db_server cp_server;
	struct alvs_server_info_key nps_server_info_key;
	struct alvs_server_info_result nps_server_info_result;
	struct alvs_service_info_key nps_service_info_key;
	struct alvs_service_info_result nps_service_info_result;

	/* Check if request is supported */
	if (supported_routing_alg(get_routing_alg(ip_vs_dest->conn_flags)) == false) {
		write_log(LOG_NOTICE, "Routing algorithm (%d) is not supported.\n", get_routing_alg(ip_vs_dest->conn_flags));
		return ALVS_DB_NOT_SUPPORTED;
	}

	/* Check if service already exists in internal DB */
	cp_service.ip = ip_vs_service->addr;
	cp_service.port = ip_vs_service->port;
	cp_service.protocol = bswap_16(ip_vs_service->protocol);
	switch (internal_db_get_service(&cp_service, true)) {
	case ALVS_DB_FAILURE:
		/* Service doesn't exist */
		write_log(LOG_NOTICE, "Can't add server. Service (ip=0x%08x, port=%d, protocol=%d) doesn't exist.\n",
		       cp_service.ip, cp_service.port, cp_service.protocol);
		return ALVS_DB_FAILURE;
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find service (internal error).\n");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_OK:
		/* Service exists */
		write_log(LOG_DEBUG, "Service (ip=0x%08x, port=%d, protocol=%d) exists.\n",
			  cp_service.ip, cp_service.port, cp_service.protocol);
		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	cp_server.ip = ip_vs_dest->addr;
	cp_server.port = ip_vs_dest->port;
	switch (internal_db_get_server(&cp_service, &cp_server)) {
	case ALVS_DB_OK:
		if (cp_server.active == true) {
			/* Server exists. */
			write_log(LOG_NOTICE, "Can't add server. Server (ip=0x%08x, port=%d) already exists.\n",
			       cp_server.ip, cp_server.port);
			return ALVS_DB_FAILURE;
		}

		/* Server exists, but inactive.
		 * Need to move server to be active
		 */
		write_log(LOG_DEBUG, "Server (ip=0x%08x, port=%d) exists (inactive).\n",
			  cp_server.ip, cp_server.port);
		if (internal_db_activate_server(&cp_service, &cp_server) == ALVS_DB_INTERNAL_ERROR) {
			write_log(LOG_CRIT, "Server activation failed..\n");
			return ALVS_DB_INTERNAL_ERROR;
		}

		break;
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_CRIT, "Can't find server (internal error).\n");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_FAILURE:
		/* server doesn't exist */
		write_log(LOG_DEBUG, "Server (ip=0x%08x, port=%d) doesn't exist.\n",
			  cp_server.ip, cp_server.port);
		/* Server doesn't exist.
		 * Create a new entry in server info.
		 */
		if (index_pool_alloc(&server_index_pool, &cp_server.nps_index) == false) {
			write_log(LOG_ERR, "Can't add server. Reached maximum.\n");
			return ALVS_DB_NOT_SUPPORTED;
		}
		write_log(LOG_DEBUG, "Allocated nps_index = %d\n", cp_server.nps_index);

		cp_server.conn_flags = ip_vs_dest->conn_flags;
		cp_server.server_flags = IP_VS_DEST_F_AVAILABLE;
		cp_server.weight = ip_vs_dest->weight;
		cp_server.active = true;
		cp_server.routing_alg = get_routing_alg(ip_vs_dest->conn_flags);
		cp_server.u_thresh = ip_vs_dest->u_threshold;
		cp_server.l_thresh = ip_vs_dest->l_threshold;
		cp_server.stats_base.mem_type = EZDP_EXTERNAL_MS;
		cp_server.stats_base.msid = EMEM_SERVER_STATS_POSTED_MSID;
		cp_server.stats_base.element_index = (EMEM_SERVER_STATS_POSTED_OFFSET >> 3) + cp_server.nps_index * ALVS_NUM_OF_SERVER_STATS;

		write_log(LOG_DEBUG, "Server info: alg=%d, conn_flags=%d, server_flags=%d, weight=%d, u_thresh=%d, l_thresh=%d.\n",
			  cp_server.routing_alg, cp_server.conn_flags,
			  cp_server.server_flags, cp_server.weight,
			  cp_server.u_thresh, cp_server.l_thresh);

		if (internal_db_add_server(&cp_service, &cp_server) == ALVS_DB_INTERNAL_ERROR) {
			write_log(LOG_CRIT, "Failed to add server to internal DB.\n");
			return ALVS_DB_INTERNAL_ERROR;
		}

		build_nps_server_info_key(&cp_server,
					  &nps_server_info_key);
		build_nps_server_info_result(&cp_server,
					     &nps_server_info_result);
		if (infra_add_entry(STRUCT_ID_ALVS_SERVER_INFO,
				    &nps_server_info_key,
				    sizeof(struct alvs_server_info_key),
				    &nps_server_info_result,
				    sizeof(struct alvs_server_info_result)) == false) {
			write_log(LOG_CRIT, "Failed to add server info entry.\n");
			return ALVS_DB_NPS_ERROR;
		}

		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	cp_service.server_count++;

	/* Recalculate scheduling information */
	rc = alvs_db_recalculate_scheduling_info(&cp_service);
	if (rc != ALVS_DB_OK) {
		write_log(LOG_ERR, "Failed to delete scheduling information.\n");
		return rc;
	}
	write_log(LOG_DEBUG, "Scheduling info recalculated.\n");

	build_nps_service_info_key(&cp_service,
				   &nps_service_info_key);
	build_nps_service_info_result(&cp_service,
				      &nps_service_info_result);
	if (infra_modify_entry(STRUCT_ID_ALVS_SERVICE_INFO,
			       &nps_service_info_key,
			       sizeof(struct alvs_service_info_key),
			       &nps_service_info_result,
			       sizeof(struct alvs_service_info_result)) == false) {
		write_log(LOG_CRIT, "Failed to change server count in service info entry.\n");
		return ALVS_DB_NPS_ERROR;
	}

	write_log(LOG_INFO, "Server added successfully.\n");
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
	uint8_t prev_weight;

	/* Check is request is supported */
	if (supported_routing_alg(get_routing_alg(ip_vs_dest->conn_flags)) == false) {
		write_log(LOG_NOTICE, "Routing algorithm (%d) is not supported.\n", get_routing_alg(ip_vs_dest->conn_flags));
		return ALVS_DB_NOT_SUPPORTED;
	}

	/* Check if service already exists in internal DB */
	cp_service.ip = ip_vs_service->addr;
	cp_service.port = ip_vs_service->port;
	cp_service.protocol = bswap_16(ip_vs_service->protocol);
	switch (internal_db_get_service(&cp_service, true)) {
	case ALVS_DB_FAILURE:
		/* Service doesn't exist */
		write_log(LOG_NOTICE, "Can't modify server. Service (ip=0x%08x, port=%d, protocol=%d) doesn't exist.\n",
		       cp_service.ip, cp_service.port, cp_service.protocol);
		return ALVS_DB_FAILURE;
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find service (internal error).\n");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_OK:
		/* Service exists */
		write_log(LOG_DEBUG, "Service (ip=0x%08x, port=%d, protocol=%d) exists\n",
			  cp_service.ip, cp_service.port, cp_service.protocol);
		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	cp_server.ip = ip_vs_dest->addr;
	cp_server.port = ip_vs_dest->port;
	switch (internal_db_get_server(&cp_service, &cp_server)) {
	case ALVS_DB_FAILURE:
		/* Server exists. */
		write_log(LOG_NOTICE, "Can't add server. Server (ip=0x%08x, port=%d) doesn't exist.\n",
		       cp_server.ip, cp_server.port);
		return ALVS_DB_FAILURE;
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_CRIT, "Can't find server (internal error).\n");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_OK:
		if (cp_server.active == false) {
			/* Server exists, but inactive. */
			write_log(LOG_NOTICE, "Can't modify server. Server (ip=0x%08x, port=%d) is inactive.\n",
			       cp_server.ip, cp_server.port);
			return ALVS_DB_FAILURE;
		}

		/* Server exists */
		write_log(LOG_DEBUG, "Server (ip=0x%08x, port=%d) exists (active).\n",
			  cp_server.ip, cp_server.port);
		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	prev_weight = cp_server.weight;
	cp_server.conn_flags = ip_vs_dest->conn_flags;
	cp_server.weight = ip_vs_dest->weight;
	cp_server.routing_alg = get_routing_alg(ip_vs_dest->conn_flags);
	cp_server.u_thresh = ip_vs_dest->u_threshold;
	cp_server.l_thresh = ip_vs_dest->l_threshold;

	write_log(LOG_DEBUG, "Server info: alg=%d, conn_flags=%d, server_flags=%d, weight=%d, u_thresh=%d, l_thresh=%d.\n",
		  cp_server.routing_alg, cp_server.conn_flags,
		  cp_server.server_flags, cp_server.weight, cp_server.u_thresh,
		  cp_server.l_thresh);

	if (internal_db_modify_server(&cp_service, &cp_server) == ALVS_DB_INTERNAL_ERROR) {
		write_log(LOG_CRIT, "Failed to modify server in internal DB.\n");
		return ALVS_DB_INTERNAL_ERROR;
	}

	if (cp_server.weight != prev_weight) {
		/* Recalculate scheduling information */
		rc = alvs_db_recalculate_scheduling_info(&cp_service);
		if (rc != ALVS_DB_OK) {
			write_log(LOG_CRIT, "Failed to delete scheduling information.\n");
			return rc;
		}
		write_log(LOG_DEBUG, "Scheduling info recalculated.\n");
	}

	build_nps_server_info_key(&cp_server, &nps_server_info_key);
	build_nps_server_info_result(&cp_server, &nps_server_info_result);
	if (infra_modify_entry(STRUCT_ID_ALVS_SERVER_INFO,
			       &nps_server_info_key,
			    sizeof(struct alvs_server_info_key),
			    &nps_server_info_result,
			    sizeof(struct alvs_server_info_result)) == false) {
		write_log(LOG_CRIT, "Failed to modify server info entry.\n");
		return ALVS_DB_NPS_ERROR;
	}

	write_log(LOG_INFO, "Server modified successfully.\n");
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

	/* Check if service already exists in internal DB */
	cp_service.ip = ip_vs_service->addr;
	cp_service.port = ip_vs_service->port;
	cp_service.protocol = bswap_16(ip_vs_service->protocol);
	switch (internal_db_get_service(&cp_service, true)) {
	case ALVS_DB_FAILURE:
		/* Service doesn't exist */
		write_log(LOG_NOTICE, "Can't delete server. Service (ip=0x%08x, port=%d, protocol=%d) doesn't exist.\n",
		       cp_service.ip, cp_service.port, cp_service.protocol);
		return ALVS_DB_FAILURE;
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_ERR, "Can't find service (internal error).\n");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_OK:
		/* Service exists */
		write_log(LOG_DEBUG, "Service (ip=0x%08x, port=%d, protocol=%d) exists.\n",
			  cp_service.ip, cp_service.port, cp_service.protocol);
		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	cp_server.ip = ip_vs_dest->addr;
	cp_server.port = ip_vs_dest->port;
	switch (internal_db_get_server(&cp_service, &cp_server)) {
	case ALVS_DB_FAILURE:
		/* Server exists. */
		write_log(LOG_NOTICE, "Can't delete server. Server (ip=0x%08x, port=%d) doesn't exist.\n",
		       cp_server.ip, cp_server.port);
		return ALVS_DB_FAILURE;
	case ALVS_DB_INTERNAL_ERROR:
		/* Internal error */
		write_log(LOG_CRIT, "Can't find server (internal error).\n");
		return ALVS_DB_INTERNAL_ERROR;
	case ALVS_DB_OK:
		if (cp_server.active == false) {
			/* Server exists, but inactive. */
			write_log(LOG_NOTICE, "Can't delete server. Server (ip=0x%08x, port=%d) already inactive.\n",
			       cp_server.ip, cp_server.port);
			return ALVS_DB_FAILURE;
		}

		/* Server exists */
		write_log(LOG_DEBUG, "Server (ip=0x%08x, port=%d) exists (active).\n",
			  cp_server.ip, cp_server.port);
		break;
	default:
		/* Can't reach here */
		return ALVS_DB_INTERNAL_ERROR;
	}

	if (internal_db_deactivate_server(&cp_service, &cp_server) == ALVS_DB_INTERNAL_ERROR) {
		write_log(LOG_CRIT, "Failed to deactivate server in internal DB.\n");
		return ALVS_DB_INTERNAL_ERROR;
	}

	cp_service.server_count--;

	/* Recalculate scheduling information */
	rc = alvs_db_recalculate_scheduling_info(&cp_service);
	if (rc != ALVS_DB_OK) {
		write_log(LOG_CRIT, "Failed to delete scheduling information.\n");
		return rc;
	}
	write_log(LOG_DEBUG, "Scheduling info recalculated.\n");

	build_nps_service_info_key(&cp_service, &nps_service_info_key);
	build_nps_service_info_result(&cp_service, &nps_service_info_result);
	if (infra_modify_entry(STRUCT_ID_ALVS_SERVICE_INFO,
			       &nps_service_info_key,
			       sizeof(struct alvs_service_info_key),
			       &nps_service_info_result,
			       sizeof(struct alvs_service_info_result)) == false) {
		write_log(LOG_CRIT, "Failed to change server count in service info entry.\n");
		return ALVS_DB_NPS_ERROR;
	}

	write_log(LOG_INFO, "Server deleted successfully.\n");
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
	if (internal_db_clear_all() == ALVS_DB_INTERNAL_ERROR) {
		write_log(LOG_CRIT, "Failed to delete all services in internal DB.\n");
		return ALVS_DB_INTERNAL_ERROR;
	}
	index_pool_rewind(&service_index_pool);
	write_log(LOG_DEBUG, "Internal DB cleared.\n");

	if (infra_delete_all_entries(STRUCT_ID_ALVS_SERVICE_CLASSIFICATION) == false) {
		write_log(LOG_CRIT, "Failed to delete all entries from service classification DB in NPS.\n");
		return ALVS_DB_NPS_ERROR;
	}

	if (infra_delete_all_entries(STRUCT_ID_ALVS_SERVICE_INFO) == false) {
		write_log(LOG_CRIT, "Failed to delete all entries from service info DB in NPS.\n");
		return ALVS_DB_NPS_ERROR;
	}

	if (infra_delete_all_entries(STRUCT_ID_ALVS_SCHED_INFO) == false) {
		write_log(LOG_CRIT, "Failed to delete all entries from scheduling info DB in NPS.\n");
		return ALVS_DB_NPS_ERROR;
	}

	write_log(LOG_INFO, "ALVS DBs cleared successfully.\n");
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

	sigemptyset(&sigs_to_block);
	sigaddset(&sigs_to_block, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &sigs_to_block, NULL);

	write_log(LOG_DEBUG, "Start aging thread\n");

	sprintf(sql, "SELECT * FROM servers "
		"WHERE active=0;");

	while (!(*alvs_db_cancel_application_flag_ptr)) {
		sleep(1);
		/* Prepare SQL statement */
		rc = sqlite3_prepare_v2(alvs_db, sql, -1, &statement, NULL);
		if (rc != SQLITE_OK) {
			write_log(LOG_CRIT, "SQL error: %s\n",
				  sqlite3_errmsg(alvs_db));
			server_db_exit_with_error();
		}

		/* Execute SQL statement */
		rc = sqlite3_step(statement);

		/* Collect server ids */
		while (rc == SQLITE_ROW) {
			if (get_num_active_conn(sqlite3_column_int(statement, 12)) == 0) {
				cp_server.ip = sqlite3_column_int(statement, 0);
				cp_server.port = sqlite3_column_int(statement, 1);
				cp_service.ip = sqlite3_column_int(statement, 2);
				cp_service.port = sqlite3_column_int(statement, 3);
				cp_service.protocol = sqlite3_column_int(statement, 4);

				if (internal_db_delete_server(&cp_service, &cp_server) == ALVS_DB_INTERNAL_ERROR) {
					write_log(LOG_CRIT, "Delete server failed in aging thread\n");
					server_db_exit_with_error();
				}
			}

			rc = sqlite3_step(statement);
		}

		/* Error */
		if (rc < SQLITE_ROW) {
			write_log(LOG_CRIT, "SQL error: %s\n",
				  sqlite3_errmsg(alvs_db));
			sqlite3_finalize(statement);
			server_db_exit_with_error();
		}

		/* finalize SQL statement */
		sqlite3_finalize(statement);
	}
}
