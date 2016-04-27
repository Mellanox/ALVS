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
*/

#include <pthread.h>
#include <stdint.h>
#include <signal.h>
#include <EZenv.h>
#include <EZdev.h>
#include <EZagtCPMain.h>
#include <EZosTask.h>
#include <EZagtRPC.h>

#include "infrastructure.h"
#include "infrastructure_conf.h"

#include "nw_db_manager.h"
#include "alvs_db_manager.h"

#include "defs.h"

/******************************************************************************/

extern
EZbool EZdevSim_WaitForInitSocket(EZui32 uiNumberOfConnections);

static
bool init(void);

static
bool setup_chip(void);

static
bool setup_chip(void);
void main_thread_graceful_stop(void);
void signal_terminate_handler(int signum);

enum object_type {
	object_type_board,
	object_type_cp,
	object_type_agt,
	object_type_nw_db_manager,
	object_type_alvs_db_manager,
	object_type_count
};

#define DB_CONSTRUCTORS_NUM 2
bool (*db_constructors[DB_CONSTRUCTORS_NUM])(void) = {
		&nw_db_constructor,
		&alvs_db_constructor
	};

/******************************************************************************/
pthread_t nw_db_manager_thread;
pthread_t alvs_db_manager_thread;
bool is_object_allocated[object_type_count];
EZagtRPCServer host_server;
/******************************************************************************/

int main(void)
{
	int rc;
	/************************************************/
	/* Run in the background as a daemon            */
	/************************************************/
#ifdef ALVS_DAEMON
	daemon(0, 1);
#endif
	/* listen to the SHUTDOWN signal to handle terminate signal */
	signal(SIGINT, signal_terminate_handler);
	signal(SIGTERM, signal_terminate_handler);
	signal(SIGILL, signal_terminate_handler);
	signal(SIGSEGV, signal_terminate_handler);
	signal(SIGBUS, signal_terminate_handler);

	memset(is_object_allocated, 0, object_type_count*sizeof(bool));
	/************************************************/
	/* Initialize board, CP SDK host components     */
	/* and AGT debug agent interface                */
	/************************************************/
	if (init() == false) {
		main_thread_graceful_stop();
		exit(1);
	}

	/************************************************/
	/* Initializing control plane components        */
	/************************************************/
	printf("Setup chip...\n");
	if (setup_chip() == false) {
		main_thread_graceful_stop();
		exit(1);
	}

	/************************************************/
	/* Start network DB manager main thread         */
	/************************************************/
	rc = pthread_create(&nw_db_manager_thread, NULL,
			    (void * (*)(void *))nw_db_manager_main, NULL);
	if (rc != 0) {
		perror("error: pthread_create failed for network DB manager\n");
		main_thread_graceful_stop();
		exit(1);
	}
	is_object_allocated[object_type_nw_db_manager] = true;

	/************************************************/
	/* Start ALVS DB manager main thread            */
	/************************************************/
	rc = pthread_create(&alvs_db_manager_thread, NULL,
			    (void * (*)(void *))alvs_db_manager_main, NULL);
	if (rc != 0) {
		perror("error: pthread_create failed for ALVS DB manager\n");
		main_thread_graceful_stop();
		exit(1);
	}
	is_object_allocated[object_type_alvs_db_manager] = true;

	/************************************************/
	/* Wait for signal                              */
	/************************************************/
	sigset_t mask, oldmask;
	/* Set up the mask of signals to temporarily block. */
	sigemptyset(&mask);
	sigaddset(&mask, SIGTERM);
	/* Wait for a signal to arrive. */
	sigprocmask(SIG_BLOCK, &mask, &oldmask);
	sigsuspend(&oldmask);
	sigprocmask(SIG_UNBLOCK, &mask, NULL);


	main_thread_graceful_stop();

	return 0;
}

static
bool setup_chip(void)
{
	uint32_t ind;
	uint32_t pup_phase;
	EZstatus ez_ret_val;

	/************************************************/
	/* Create channel                               */
	/************************************************/
	printf("Creating channel...\n");
	ez_ret_val = EZapiChannel_Create(0);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		printf("setup_chip: EZapiChannel_Create failed.\n");
		return false;
	}

	/************************************************/
	/* Created state:                               */
	/* --------------                               */
	/* 1. Create interfaces mapping                 */
	/* 2. Create memory partition                   */
	/* 3. Create statistics                         */
	/* 4. Configure my MAC                          */
	/************************************************/
	printf("Created state...\n");
	if (infra_create_if_mapping() == false) {
		printf("setup_chip: infra_create_if_mapping failed.\n");
		return false;
	}
	if (infra_create_mem_partition() == false) {
		printf("setup_chip: infra_create_mem_partition failed.\n");
		return false;
	}
	if (infra_create_statistics() == false) {
		printf("setup_chip: infra_create_statistics failed.\n");
		return false;
	}
	if (infra_configure_protocol_decode() == false) {
		printf("setup_chip: infra_configure_protocol_decode failed.\n");
		return false;
	}

	/************************************************/
	/* Power-up channel                             */
	/************************************************/
	printf("Power-up channel...\n");
	for (pup_phase = 1; pup_phase <= 4; pup_phase++) {
		ez_ret_val = EZapiChannel_PowerUp(0, pup_phase);
		if (EZrc_IS_ERROR(ez_ret_val)) {
			printf("setup_chip: EZapiChannel_PowerUp phase %u failed.\n", pup_phase);
			return false;
		}
	}

	/************************************************/
	/* Powered-up state:                            */
	/* --------------                               */
	/* Do nothing                                   */
	/************************************************/
	printf("Powered-up state...\n");

	/************************************************/
	/* Initialize channel                           */
	/************************************************/
	printf("Initialize channel...\n");
	ez_ret_val = EZapiChannel_Initialize(0);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		printf("setup_chip: EZapiChannel_Initialize failed.\n");
		return false;
	}

	/************************************************/
	/* Initialized state:                           */
	/* --------------                               */
	/* 1. Create search_structures                  */
	/* 2. Load partition                            */
	/* 3. Initialize statistics                     */
	/************************************************/
	printf("Initialized state...\n");
	for (ind = 0; ind < DB_CONSTRUCTORS_NUM; ind++) {
		if (db_constructors[ind]() == false) {
			printf("setup_chip: db_constructor[%d] failed.\n", ind);
			return false;
		}
	}
	if (load_partition() == false) {
		printf("setup_chip: load_partition failed.\n");
		return false;
	}
	if (infra_initialize_statistics() == false) {
		printf("setup_chip: initialize_statistics failed.\n");
		return false;
	}

	/************************************************/
	/* Finalize channel                             */
	/************************************************/
	printf("Finalize channel...\n");
	ez_ret_val = EZapiChannel_Finalize(0);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		printf("setup_chip: EZapiChannel_Finalize failed.\n");
		return false;
	}

	/************************************************/
	/* Finalized state:                             */
	/* --------------                               */
	/* Do nothing                                   */
	/************************************************/
	printf("Finalized state...\n");

	/************************************************/
	/* Channel GO                                   */
	/************************************************/
	printf("Channel GO...\n");
	ez_ret_val = EZapiChannel_Go(0);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		printf("setup_chip: EZapiChannel_Go failed.\n");
		return false;
	}

	/************************************************/
	/* Running state:                               */
	/************************************************/
	printf("Running state...\n");

	return true;
}

static
void EZhost_RPCJSONTask(void)
{
	EZagtRPC_ServerRun(host_server);
}

static
bool init(void)
{
	EZstatus ez_ret_val;
	EZtask task;
	EZdev_PlatformParams platform_params;
	uint8_t kernel_module[16];
	FILE *fd;
	bool is_real_chip = false;

	printf("Init board...\n");
	/* Determine if we are running on real chip or simulator */
	fd = popen("lsmod | grep nps_cp", "r");
	if (fd != NULL) {
		if (fread(kernel_module, 1, sizeof(kernel_module), fd) > 0) {
			is_real_chip = true;
		}
		pclose(fd);
	}

	/* Initialize EZdev */
	platform_params.uiChipPhase = EZdev_CHIP_PHASE_1;
	if (is_real_chip == true) {
		platform_params.ePlatform = EZdev_Platform_UIO;
	} else {
		platform_params.ePlatform = EZdev_Platform_SIM;
	}
	ez_ret_val = EZdev_Create(&platform_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		printf("init_board: EZdev_Create failed.\n");
		return false;
	}

	if (is_real_chip == false) {
		/* Wait for simulator to connect to socket */
		ez_ret_val = EZdevSim_WaitForInitSocket(1);
		if (EZrc_IS_ERROR(ez_ret_val)) {
			printf("init_board: EZdevSim_WaitForInitSocket failed.\n");
			return false;
		}
	}

	/************************************************/
	/* Initialize Env                               */
	/************************************************/
	printf("Initialize Env...\n");
	ez_ret_val = EZenv_Create();
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}
	is_object_allocated[object_type_board] = true;

	/************************************************/
	/* Create and run CP library                    */
	/************************************************/
	printf("Create CP library...\n");
	ez_ret_val = EZapiCP_Create();
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	printf("Run CP library...\n");
	ez_ret_val = EZapiCP_Go();
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}
	is_object_allocated[object_type_cp] = true;

	/************************************************/
	/* Enable AGT debug agent interface             */
	/************************************************/
	printf("Create AGT...\n");
	/* Create rpc server for given port */
	host_server = EZagtRPC_CreateServer(INFRA_AGT_PORT);
	if (host_server == NULL) {
		return false;
	}

	/* Register standard CP commands */
	ez_ret_val = EZagt_RegisterFunctions(host_server);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	/* Register standard CP commands */
	ez_ret_val = EZagtCPMain_RegisterFunctions(host_server);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	/* Create task for run rpc-json commands  */
	task = EZosTask_Spawn("agt", EZosTask_NORMAL_PRIORITY, 0x100000,
			      (EZosTask_Spawn_FuncPtr)EZhost_RPCJSONTask, 0);
	if (task == EZosTask_INVALID_TASK) {
		return false;
	}
	is_object_allocated[object_type_agt] = true;

	return true;
}

/************************************************************************
 * \brief      signal terminate handler
 *             Only main thread can receive SIGTERM signal
 *             and perform graceful stop.
 * \return     void
 */
void signal_terminate_handler(int signum)
{
	if (signum != SIGTERM) {
		raise(SIGTERM);
		sleep(2);
	} else {
		main_thread_graceful_stop();
		exit(0);
	}
}
/************************************************************************
 * \brief      graceful stop performed in main thread only
 *             shuts down other threads if needed
 * \return     void
 */
void main_thread_graceful_stop(void)
{
	EZstatus ez_ret_val;

	if (is_object_allocated[object_type_nw_db_manager]) {
		printf("Shut down thread nw_db_manager.\n");
		nw_db_manager_set_cancel_thread();
		is_object_allocated[object_type_nw_db_manager] = false;
	}
	if (is_object_allocated[object_type_alvs_db_manager]) {
		printf("Shut down thread alvs_db_manager.\n");
		alvs_db_manager_set_cancel_thread();
		is_object_allocated[object_type_alvs_db_manager] = false;
	}
	pthread_join(nw_db_manager_thread, NULL);
	pthread_join(alvs_db_manager_thread, NULL);

	if (is_object_allocated[object_type_agt]) {
		is_object_allocated[object_type_agt] = false;
		printf("Delete AGT\n");
		EZagtRPC_ServerStop(host_server);
		EZosTask_Delay(10);
		EZagtRPC_ServerDestroy(host_server);
	}
	if (is_object_allocated[object_type_cp]) {
		is_object_allocated[object_type_cp] = false;
		printf("Delete CP\n");
		ez_ret_val = EZapiCP_Delete();
		if (EZrc_IS_ERROR(ez_ret_val)) {
			printf("delete_cp: EZapiCP_Delete failed.\n");
		}
		ez_ret_val = EZenv_Delete();
		if (EZrc_IS_ERROR(ez_ret_val)) {
			printf("delete_cp: EZenv_Delete failed.\n");
		}
	}
	if (is_object_allocated[object_type_board]) {
		is_object_allocated[object_type_board] = false;
		printf("Delete board\n");
		ez_ret_val = EZdev_Delete();
		if (EZrc_IS_ERROR(ez_ret_val)) {
			printf("delete_board: EZdev_Delete failed.\n");
		}
	}
}
