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
#include <getopt.h>
#include <signal.h>
#include <EZenv.h>
#include <EZdev.h>
#include <EZlog.h>

#include "log.h"
#include "infrastructure.h"

#include "nw_db_manager.h"
#include "alvs_db_manager.h"

#include "defs.h"
#include "version.h"

/******************************************************************************/

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
pthread_t main_thread;
pthread_t nw_db_manager_thread;
pthread_t alvs_db_manager_thread;
bool is_object_allocated[object_type_count];
bool cancel_application_flag;
int agt_enabled;
EZapiChannel_EthIFType port_type;
/******************************************************************************/

int main(int argc, char **argv)
{
	int rc;
	int option_index;

	struct option long_options[] = {
		{ "agt_enabled", no_argument, &agt_enabled, true },
		{ "port_type", required_argument, 0, 'p' },
		{0, 0, 0, 0} };

	cancel_application_flag = false;
	main_thread = pthread_self();
	open_log("ALVS_DAEMON");

	printf("Application version: %s\n", version);

	/* Defaults */
	agt_enabled = false;
	port_type = EZapiChannel_EthIFType_40GE;

	while (true) {
		rc = getopt_long(argc, argv, "", long_options, &option_index);
		if (rc == -1) {
			break;
		}

		switch (rc) {
		case 0:
			break;

		case 'p':
			if (strcmp(optarg, "10GE") == 0) {
				port_type = EZapiChannel_EthIFType_10GE;
			} else if (strcmp(optarg, "40GE") == 0) {
				port_type = EZapiChannel_EthIFType_40GE;
			} else if (strcmp(optarg, "100GE") == 0) {
				port_type = EZapiChannel_EthIFType_100GE;
			} else {
				write_log(LOG_CRIT, "Port type argument is invalid (%s), valid values are 10GE, 40GE and 100GE.\n", optarg);
				abort();
			}
			break;

		case '?':
			break;

		default:
			abort();
		}
	}


	/* listen to the SHUTDOWN signal to handle terminate signal */
	signal(SIGINT, signal_terminate_handler);
	signal(SIGTERM, signal_terminate_handler);
	signal(SIGILL, signal_terminate_handler);
	signal(SIGSEGV, signal_terminate_handler);
	signal(SIGBUS, signal_terminate_handler);

	write_log(LOG_INFO, "Starting ALVS daemon application (port type = %s,  AGT enabled = %s) ...\n",
		  port_type == EZapiChannel_EthIFType_10GE ? "10GE" : (port_type == EZapiChannel_EthIFType_40GE ? "40GE" : "100GE"),
			  agt_enabled ? "True" : "False");

	memset(is_object_allocated, 0, object_type_count*sizeof(bool));
	/************************************************/
	/* Initialize device, CP SDK host components    */
	/* and AGT debug agent interface                */
	/************************************************/
	write_log(LOG_DEBUG, "NPS init ...\n");
	if (nps_init() == false) {
		main_thread_graceful_stop();
		exit(1);
	}

	/************************************************/
	/* Bring up control plane components        */
	/************************************************/
	write_log(LOG_DEBUG, "NPS bring up...\n");
	if (nps_bringup() == false) {
		main_thread_graceful_stop();
		exit(1);
	}

	write_log(LOG_INFO, "ALVS daemon application is running...\n");

	/************************************************/
	/* Start network DB manager main thread         */
	/************************************************/
	write_log(LOG_DEBUG, "Start network DB manager thread...\n");
	rc = pthread_create(&nw_db_manager_thread, NULL,
			    (void * (*)(void *))nw_db_manager_main, &cancel_application_flag);
	if (rc != 0) {
		write_log(LOG_CRIT, "Cannot start nw_db_manager_thread: pthread_create failed for network DB manager\n");
		main_thread_graceful_stop();
		exit(1);
	}
	is_object_allocated[object_type_nw_db_manager] = true;

	/************************************************/
	/* Start ALVS DB manager main thread            */
	/************************************************/
	write_log(LOG_DEBUG, "Start ALVS DB manager thread...\n");
	rc = pthread_create(&alvs_db_manager_thread, NULL,
			    (void * (*)(void *))alvs_db_manager_main, &cancel_application_flag);
	if (rc != 0) {
		write_log(LOG_CRIT, "Cannot start alvs_db_manager_thread: pthread_create failed for ALVS DB manager\n");
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
	write_log(LOG_DEBUG, "Creating channel...\n");
	ez_ret_val = EZapiChannel_Create(0);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "nps_bringup failed: EZapiChannel_Create returned an error.\n");
		return false;
	}

	/************************************************/
	/* Created state                                */
	/************************************************/
	write_log(LOG_DEBUG, "Created state...\n");
	if (infra_created() == false) {
		write_log(LOG_CRIT, "nps_bringup failed: infra_created returned an error.\n");
		return false;
	}

	/************************************************/
	/* Power-up channel                             */
	/************************************************/
	write_log(LOG_DEBUG, "Power-up channel...\n");
	for (pup_phase = 1; pup_phase <= 4; pup_phase++) {
		ez_ret_val = EZapiChannel_PowerUp(0, pup_phase);
		if (EZrc_IS_ERROR(ez_ret_val)) {
			write_log(LOG_CRIT, "nps_bringup failed: EZapiChannel_PowerUp phase %u failed.\n", pup_phase);
			return false;
		}
	}

	/************************************************/
	/* Powered-up state                             */
	/************************************************/
	write_log(LOG_DEBUG, "Powered-up state...\n");
	if (infra_powered_up() == false) {
		write_log(LOG_CRIT, "nps_bringup failed: infra_powered_up returned an error.\n");
		return false;
	}

	/************************************************/
	/* Initialize channel                           */
	/************************************************/
	write_log(LOG_DEBUG, "Initialize channel...\n");
	ez_ret_val = EZapiChannel_Initialize(0);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "nps_bringup failed: EZapiChannel_Initialize returned an error.\n");
		return false;
	}

	/************************************************/
	/* Initialized state                            */
	/************************************************/
	write_log(LOG_DEBUG, "Initialized state...\n");
	if (infra_initialized() == false) {
		write_log(LOG_CRIT, "nps_bringup failed: infra_initialized returned an error.\n");
		return false;
	}

	/************************************************/
	/* Finalize channel                             */
	/************************************************/
	write_log(LOG_DEBUG, "Finalize channel...\n");
	ez_ret_val = EZapiChannel_Finalize(0);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "nps_bringup failed: EZapiChannel_Finalize returned an error.\n");
		return false;
	}

	/************************************************/
	/* Finalized state                              */
	/************************************************/
	write_log(LOG_DEBUG, "Finalized state...\n");
	if (infra_finalized() == false) {
		write_log(LOG_CRIT, "nps_bringup failed: infra_finalized returned an error.\n");
		return false;
	}

	/************************************************/
	/* Channel GO                                   */
	/************************************************/
	write_log(LOG_DEBUG, "Channel GO...\n");
	ez_ret_val = EZapiChannel_Go(0);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "nps_bringup failed: EZapiChannel_Go returned an error.\n");
		return false;
	}

	/************************************************/
	/* Running state:                               */
	/************************************************/
	write_log(LOG_DEBUG, "Running state...\n");
	if (infra_running() == false) {
		write_log(LOG_CRIT, "nps_bringup failed: infra_running returned an error.\n");
		return false;
	}

	return true;
}

bool nps_init(void)
{
	EZstatus ez_ret_val;
	EZdev_PlatformParams platform_params;

	/************************************************/
	/* Initialize Env                               */
	/************************************************/
	write_log(LOG_DEBUG, "Initialize Env...\n");
	ez_ret_val = EZenv_Create();
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "nps_init: EZenv_Create failed.\n");
		return false;
	}
	is_object_allocated[object_type_env] = true;

	/************************************************/
	/* Initialize EZdev                             */
	/************************************************/
	write_log(LOG_DEBUG, "Init EZdev...\n");
	platform_params.uiChipPhase = EZdev_CHIP_PHASE_1;
#ifdef EZ_SIM
	write_log(LOG_DEBUG, "Platform is simulator.\n");
	platform_params.ePlatform = EZdev_Platform_SIM;
	platform_params.uiSimNumberOfConnections = 1;
#else
	write_log(LOG_DEBUG, "Platform is NPS chip.\n");
	platform_params.ePlatform = EZdev_Platform_UIO;
#endif
	ez_ret_val = EZdev_Create(&platform_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "nps_init: EZdev_Create failed.\n");
		return false;
	}
	is_object_allocated[object_type_dev] = true;

	/************************************************/
	/* Create and run CP library                    */
	/************************************************/
	write_log(LOG_DEBUG, "Create CP library...\n");
	ez_ret_val = EZapiCP_Create();
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "nps_init: EZapiCP_Create failed.\n");
		return false;
	}
	is_object_allocated[object_type_cp] = true;

	write_log(LOG_DEBUG, "Run CP library...\n");
	ez_ret_val = EZapiCP_Go();
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "nps_init: EZapiCP_Go failed.\n");
		return false;
	}

	/************************************************/
	/* Enable AGT debug agent interface             */
	/************************************************/
	if (agt_enabled == true) {
		write_log(LOG_INFO, "Enable AGT...\n");
		if (infra_enable_agt() == false) {
			write_log(LOG_CRIT, "nps_init: infra_enable_agt failed.\n");
			return false;
		}
		is_object_allocated[object_type_agt] = true;
	}

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
		pthread_t self = pthread_self();

		if (self == main_thread) {
			main_thread_graceful_stop();
			exit(1);
		}
		if (self == nw_db_manager_thread) {
			nw_db_manager_exit_with_error();
		}
		if (self == alvs_db_manager_thread) {
			alvs_db_manager_exit_with_error();
		}

		kill(getpid(), SIGTERM);
		pthread_exit(NULL);
	} else {
		main_thread_graceful_stop();
		exit(1);
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

	write_log(LOG_INFO, "Shut down ALVS daemon.\n");
	cancel_application_flag = true;
	if (is_object_allocated[object_type_nw_db_manager]) {
		pthread_join(nw_db_manager_thread, NULL);
	}
	if (is_object_allocated[object_type_alvs_db_manager]) {
		pthread_join(alvs_db_manager_thread, NULL);
	}
	if (is_object_allocated[object_type_agt]) {
		write_log(LOG_DEBUG, "Delete AGT\n");
		infra_disable_agt();
	}
	if (is_object_allocated[object_type_cp]) {
		write_log(LOG_DEBUG, "Delete CP\n");
		ez_ret_val = EZapiCP_Delete();
		if (EZrc_IS_ERROR(ez_ret_val)) {
			write_log(LOG_ERR, "main_thread_graceful_stop: EZapiCP_Delete failed.\n");
		}
	}
	if (is_object_allocated[object_type_dev]) {
		write_log(LOG_DEBUG, "Delete dev\n");
		ez_ret_val = EZdev_Delete();
		if (EZrc_IS_ERROR(ez_ret_val)) {
			write_log(LOG_ERR, "main_thread_graceful_stop: EZdev_Delete failed.\n");
		}
	}
	if (is_object_allocated[object_type_env]) {
		write_log(LOG_DEBUG, "Delete env\n");
		ez_ret_val = EZenv_Delete();
		if (EZrc_IS_ERROR(ez_ret_val)) {
			write_log(LOG_ERR, "main_thread_graceful_stop: EZenv_Delete failed.\n");
		}
	}

	close_log();
}
