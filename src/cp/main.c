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
*  File:                main.c
*  Desc:                Main thread configuration.
*/

#include <pthread.h>
#include <stdint.h>
#include <signal.h>
#include <EZenv.h>
#include <EZdev.h>

#include "infrastructure.h"
#include "infrastructure_conf.h"

#include "nw_db_manager.h"
#include "alvs_db_manager.h"

#include "defs.h"

/******************************************************************************/

#ifdef EZ_SIM
extern
bool EZdevSim_WaitForInitSocket(uint32_t num_of_connection);
#endif

bool nps_init(void);
bool nps_bringup(void);
void main_thread_graceful_stop(void);
void signal_terminate_handler(int signum);

enum object_type {
	object_type_dev,
	object_type_cp,
	object_type_env,
	object_type_agt,
	object_type_nw_db_manager,
	object_type_alvs_db_manager,
	object_type_count
};

/******************************************************************************/
pthread_t nw_db_manager_thread;
pthread_t alvs_db_manager_thread;
bool is_object_allocated[object_type_count];
/******************************************************************************/

int main(void)
{
	int rc;

	printf("Starting ALVS CP application...\n");

#ifndef DAEMON_DISABLE
	/************************************************/
	/* Run in the background as a daemon            */
	/************************************************/
	daemon(0, 0);
#endif
	/* listen to the SHUTDOWN signal to handle terminate signal */
	signal(SIGINT, signal_terminate_handler);
	signal(SIGTERM, signal_terminate_handler);
	signal(SIGILL, signal_terminate_handler);
	signal(SIGSEGV, signal_terminate_handler);
	signal(SIGBUS, signal_terminate_handler);

	memset(is_object_allocated, 0, object_type_count*sizeof(bool));
	/************************************************/
	/* Initialize device, CP SDK host components    */
	/* and AGT debug agent interface                */
	/************************************************/
	if (nps_init() == false) {
		main_thread_graceful_stop();
		exit(1);
	}

	/************************************************/
	/* Bring up control plane components        */
	/************************************************/
	printf("Bring up...\n");
	if (nps_bringup() == false) {
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

bool nps_bringup(void)
{
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
	/* Created state                                */
	/************************************************/
	printf("Created state...\n");
	if (infra_created() == false) {
		printf("setup_chip: infra_created failed.\n");
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
	/* Powered-up state                             */
	/************************************************/
	printf("Powered-up state...\n");
	if (infra_powered_up() == false) {
		printf("setup_chip: infra_powered_up failed.\n");
		return false;
	}

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
	/* Initialized state                            */
	/************************************************/
	printf("Initialized state...\n");
	if (infra_initialized() == false) {
		printf("setup_chip: infra_initialized failed.\n");
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
	/* Finalized state                              */
	/************************************************/
	printf("Finalized state...\n");
	if (infra_finalized() == false) {
		printf("setup_chip: infra_finalized failed.\n");
		return false;
	}

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
	if (infra_running() == false) {
		printf("setup_chip: infra_running failed.\n");
		return false;
	}

	return true;
}

bool nps_init(void)
{
	EZstatus ez_ret_val;
	EZdev_PlatformParams platform_params;

	/************************************************/
	/* Initialize EZdev                             */
	/************************************************/
	printf("Init EZdev...\n");
	platform_params.uiChipPhase = EZdev_CHIP_PHASE_1;
#ifdef EZ_SIM
	platform_params.ePlatform = EZdev_Platform_SIM;
#else
	platform_params.ePlatform = EZdev_Platform_UIO;
#endif
	ez_ret_val = EZdev_Create(&platform_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		printf("init_dev: EZdev_Create failed.\n");
		return false;
	}
	is_object_allocated[object_type_dev] = true;

#ifdef EZ_SIM
	/* Wait for simulator to connect to socket */
	ez_ret_val = EZdevSim_WaitForInitSocket(1);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		printf("init_dev: EZdevSim_WaitForInitSocket failed.\n");
		return false;
	}
#endif

	/************************************************/
	/* Initialize Env                               */
	/************************************************/
	printf("Initialize Env...\n");
	ez_ret_val = EZenv_Create();
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}
	is_object_allocated[object_type_env] = true;

	/************************************************/
	/* Create and run CP library                    */
	/************************************************/
	printf("Create CP library...\n");
	ez_ret_val = EZapiCP_Create();
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}
	is_object_allocated[object_type_cp] = true;

	printf("Run CP library...\n");
	ez_ret_val = EZapiCP_Go();
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

#ifdef AGT_ENABLED
	/************************************************/
	/* Enable AGT debug agent interface             */
	/************************************************/
	printf("Enable AGT...\n");
	if (infra_enable_agt() == false) {
		printf("infra_enable_agt: infra_enable_agt failed.\n");
		return false;
	}

	is_object_allocated[object_type_agt] = true;
#endif

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
		pthread_t         self;

		self = pthread_self();
		if (self == nw_db_manager_thread) {
			is_object_allocated[object_type_nw_db_manager] = false;
			nw_db_manager_exit_with_error();
		} else {
			if (self == alvs_db_manager_thread) {
				is_object_allocated[object_type_alvs_db_manager] = false;
				alvs_db_manager_exit_with_error();
			} else {
				raise(SIGTERM);
			}
		}
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
		infra_disable_agt();
	}
	if (is_object_allocated[object_type_cp]) {
		is_object_allocated[object_type_cp] = false;
		printf("Delete CP\n");
		ez_ret_val = EZapiCP_Delete();
		if (EZrc_IS_ERROR(ez_ret_val)) {
			printf("main_thread_graceful_stop: EZapiCP_Delete failed.\n");
		}
	}
	if (is_object_allocated[object_type_env]) {
		is_object_allocated[object_type_env] = false;
		printf("Delete env\n");
		ez_ret_val = EZenv_Delete();
		if (EZrc_IS_ERROR(ez_ret_val)) {
			printf("main_thread_graceful_stop: EZenv_Delete failed.\n");
		}
	}
	if (is_object_allocated[object_type_dev]) {
		is_object_allocated[object_type_dev] = false;
		printf("Delete dev\n");
		ez_ret_val = EZdev_Delete();
		if (EZrc_IS_ERROR(ez_ret_val)) {
			printf("main_thread_graceful_stop: EZdev_Delete failed.\n");
		}
	}
}
