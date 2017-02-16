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

#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>
#include <getopt.h>
#include <signal.h>
#include <EZenv.h>
#include <EZdev.h>
#include <EZlog.h>

#include "log.h"
#include "infrastructure.h"
#include "cfg.h"
#include "nw_db_manager.h"
#include "cli_manager.h"
#if defined(CONFIG_ALVS)
#	include "alvs_db_manager.h"
#	include "alvs_conf.h"
#elif defined(CONFIG_TC)
#	include "tc_api.h"
#endif


/******************************************************************************/
#define WAIT_FOR_NPS 500
#define FTP_RETRIES 50
#define DP_RUN_RETRIES 10

bool nps_init(void);
bool nps_bringup(void);
void dp_load(void);
void main_thread_graceful_stop(void);
void signal_terminate_handler(int signum);

enum object_type {
	object_type_dev,
	object_type_cp,
	object_type_env,
	object_type_agt,
	object_type_nw_db_manager,
	object_type_alvs_db_manager,
	object_type_tc_db_manager,
	object_type_dp_load_thread,
	object_type_cli_thread,

	/* must be at the end */
	object_type_count
};

/******************************************************************************/
/* Threads */
pthread_t main_thread;
pthread_t nw_db_manager_thread;
pthread_t alvs_db_manager_thread;
pthread_t tc_db_manager_thread;
pthread_t dp_load_thread;
pthread_t cli_thread;

/* General */
bool is_object_allocated[object_type_count];
bool cancel_application_flag;
extern struct nw_db_manager_ops nw_ops;
int fd = -1;
/******************************************************************************/

int main(int argc, char **argv)
{
	int rc;

	cancel_application_flag = false;
	main_thread = pthread_self();

#ifdef CONFIG_ALVS
	open_log("alvs_daemon");
#endif
#ifdef CONFIG_TC
	open_log("atc_daemon");
#endif

	fd = open("/var/lock/nps_cp.lock", O_RDWR|O_CREAT|O_EXCL, 0444);
	if (fd == -1) {
		write_log(LOG_ERR, "application locked, not posible to run app daemon. consider to delete file /var/lock/nps_cp.lock");
		exit(1);
	}

	/* listen to the SHUTDOWN signal to handle terminate signal */
	signal(SIGINT, signal_terminate_handler);
	signal(SIGTERM, signal_terminate_handler);
	signal(SIGILL, signal_terminate_handler);
	signal(SIGSEGV, signal_terminate_handler);
	signal(SIGBUS, signal_terminate_handler);

	/* Parse configuration from command line */
	system_configuration(argc, argv);
	system_cfg_print();

	memset(is_object_allocated, 0, object_type_count*sizeof(bool));
	/************************************************/
	/* Initialize device, CP SDK host components    */
	/* and AGT debug agent interface                */
	/************************************************/
	write_log(LOG_DEBUG, "NPS init ...");
	if (nps_init() == false) {
		main_thread_graceful_stop();
		exit(1);
	}

	/************************************************/
	/* Bring up control plane components        */
	/************************************************/
	write_log(LOG_DEBUG, "NPS bring up...");
	if (nps_bringup() == false) {
		main_thread_graceful_stop();
		exit(1);
	}

#ifdef CONFIG_ALVS
	write_log(LOG_INFO, "ALVS daemon application is running...");
#endif

	/************************************************/
	/* Start DP load and run thread                 */
	/************************************************/
	write_log(LOG_DEBUG, "Start DP load and run thread...");
	rc = pthread_create(&dp_load_thread, NULL,
			   (void * (*)(void *))dp_load, NULL);
	if (rc != 0) {
		write_log(LOG_CRIT, "Cannot start dp_load_thread: pthread_create failed for dp load and run");
		main_thread_graceful_stop();
		exit(1);
	}
	is_object_allocated[object_type_dp_load_thread] = true;

	/* run nw_db_manager thread only in case of remote control is disable */
	if (system_cfg_is_remote_control_en() == false) {
		/************************************************/
		/* Start network DB manager main thread         */
		/************************************************/
		write_log(LOG_DEBUG, "Start network DB manager thread...");

		rc = pthread_create(&nw_db_manager_thread, NULL,
				    (void * (*)(void *))nw_db_manager_main, &nw_ops);
		if (rc != 0) {
			write_log(LOG_CRIT, "Cannot start nw_db_manager_thread: pthread_create failed for network DB manager");
			main_thread_graceful_stop();
			exit(1);
		}
		is_object_allocated[object_type_nw_db_manager] = true;
	}

#ifdef CONFIG_ALVS
	/************************************************/
	/* Start ALVS DB manager main thread            */
	/************************************************/
	write_log(LOG_DEBUG, "Start ALVS DB manager thread...");
	rc = pthread_create(&alvs_db_manager_thread, NULL,
			    (void * (*)(void *))alvs_db_manager_main, &cancel_application_flag);
	if (rc != 0) {
		write_log(LOG_CRIT, "Cannot start alvs_db_manager_thread: pthread_create failed for ALVS DB manager");
		main_thread_graceful_stop();
		exit(1);
	}
	is_object_allocated[object_type_alvs_db_manager] = true;
#endif

	/************************************************/
	/* Start CLI main thread                        */
	/************************************************/
	write_log(LOG_DEBUG, "Start CLI thread...");
	rc = pthread_create(&cli_thread, NULL,
			    (void * (*)(void *))cli_manager_main,
			    &cancel_application_flag);
	if (rc != 0) {
		write_log(LOG_CRIT, "Cannot start cli_thread: pthread_create failed");
		main_thread_graceful_stop();
		exit(1);
	}
	is_object_allocated[object_type_cli_thread] = true;

#ifdef CONFIG_TC
	/************************************************/
	/* Start ALVS DB manager main thread            */
	/************************************************/
	write_log(LOG_DEBUG, "Start TC DB manager thread...");
	rc = pthread_create(&tc_db_manager_thread, NULL,
			    (void * (*)(void *))tc_db_manager_main, &cancel_application_flag);
	if (rc != 0) {
		write_log(LOG_CRIT, "Cannot start tc_db_manager_thread: pthread_create failed for TC DB manager");
		main_thread_graceful_stop();
		exit(1);
	}
	is_object_allocated[object_type_tc_db_manager] = true;
#endif

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

/******************************************************************************
 * \brief    Raises SIGTERM signal to main thread and exits the thread.
 *
 * \return   void
 */
void dp_load_exit_with_error(void)
{
	cancel_application_flag = true;
	kill(getpid(), SIGTERM);
	pthread_exit(NULL);
}

/************************************************************************
 * \brief      function for dp_load_thread - check connectivity to nps,
 *             upload and run the dp bin file
 * \return     void
 */
void dp_load(void)
{
	int rc, i;
	char temp[256], nps_ip[256];
	sigset_t sigs_to_block;
	/* Mask SIGTERM signal for this thread. */
	sigemptyset(&sigs_to_block);
	sigaddset(&sigs_to_block, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &sigs_to_block, NULL);


#ifdef EZ_SIM
	strcpy(nps_ip, "192.168.0.2");
#else
	strcpy(nps_ip, "nps_if");
#endif

	/* check connection to NPS */
	write_log(LOG_INFO, "Waiting for Connection to NPS");
	for (i = 0; i < WAIT_FOR_NPS && cancel_application_flag == false; i++) {
		snprintf(temp, sizeof(temp), "ping -c1 -w10 %s > /dev/null 2>&1", nps_ip);
		rc = system(temp);
		if (rc == 0) {
			write_log(LOG_INFO, "Connection to NPS Succeed");
			break;
		}
		write_log(LOG_DEBUG, "No Connection to NPS, Trying Again");
	}
	if (i == WAIT_FOR_NPS) {
		write_log(LOG_ERR, "Connection to NPS failed");
		dp_load_exit_with_error();
	}

	sleep(3);

#ifdef CONFIG_ALVS
#if EZ_SIM
#define DP_APP_NAME    "alvs_dp_sim"
#else
#define DP_APP_NAME    "alvs_dp"
#endif
#endif
#ifdef CONFIG_TC
#if EZ_SIM
#define DP_APP_NAME    "atc_dp_sim"
#else
#define DP_APP_NAME    "atc_dp"
#endif
#endif
	/* copy dp bin file to nps */
	write_log(LOG_INFO, "Copy dp bin file: %s to NPS", system_cfg_get_dp_bin_file());
	for (i = 0; i < FTP_RETRIES  && cancel_application_flag == false; i++) {
		snprintf(temp, sizeof(temp), "{ echo \"user root\"; echo \"put %s /tmp/"DP_APP_NAME"\"; echo \"quit\"; } | ftp -n %s;", system_cfg_get_dp_bin_file(), nps_ip);
		rc = system(temp);
		if (rc == 0) {
			write_log(LOG_INFO, "Copy Succeed");
			break;
		}
		write_log(LOG_INFO, "FTP Copy Failed, Trying again");
	}
	if (i == FTP_RETRIES) {
		write_log(LOG_ERR, "FTP Upload to NPS failed");
		dp_load_exit_with_error();
	}

	/* run dp bin */
	for (i = 0; i < DP_RUN_RETRIES  && cancel_application_flag == false; i++) {
		if (strcmp(system_cfg_get_run_cpus(), "not_used") == 0) {
			snprintf(temp, sizeof(temp), "{ echo \"chmod +x /tmp/"DP_APP_NAME"\"; echo \"/tmp/"DP_APP_NAME" &\"; sleep 10;} | telnet %s &", nps_ip);
		} else {
			snprintf(temp, sizeof(temp), "{ echo \"chmod +x /tmp/"DP_APP_NAME"\"; echo \"/tmp/"DP_APP_NAME" --run_cpus %s &\"; sleep 10;} | telnet %s &", system_cfg_get_run_cpus(), nps_ip);
		}
		rc = system(temp);
		if (rc == 0) {
			write_log(LOG_INFO, "DP Bin run succeed, waiting for DP load");
			break;
		}
		write_log(LOG_INFO, "DP Bin run failed, Trying again");
	}
	if (i == DP_RUN_RETRIES) {
		write_log(LOG_ERR, "DP Bin run failed");
		dp_load_exit_with_error();
	}
}

bool nps_bringup(void)
{
	uint32_t pup_phase;
	EZstatus ez_ret_val;

	/************************************************/
	/* Create channel                               */
	/************************************************/
	write_log(LOG_DEBUG, "Creating channel...");
	ez_ret_val = EZapiChannel_Create(0);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "nps_bringup failed: EZapiChannel_Create returned an error (0x%08x).", ez_ret_val);
		return false;
	}

	/************************************************/
	/* Created state                                */
	/************************************************/
	write_log(LOG_DEBUG, "Created state...");
	if (infra_created() == false) {
		write_log(LOG_CRIT, "nps_bringup failed: infra_created returned an error.");
		return false;
	}

	/************************************************/
	/* Power-up channel                             */
	/************************************************/
	write_log(LOG_DEBUG, "Power-up channel...");
	for (pup_phase = 1; pup_phase <= 4; pup_phase++) {
		ez_ret_val = EZapiChannel_PowerUp(0, pup_phase);
		if (EZrc_IS_ERROR(ez_ret_val)) {
			write_log(LOG_CRIT, "nps_bringup failed: EZapiChannel_PowerUp phase %u failed (0x%08x).", pup_phase, ez_ret_val);
			return false;
		}
	}

	/************************************************/
	/* Powered-up state                             */
	/************************************************/
	write_log(LOG_DEBUG, "Powered-up state...");
	if (infra_powered_up() == false) {
		write_log(LOG_CRIT, "nps_bringup failed: infra_powered_up returned an error.");
		return false;
	}

	/************************************************/
	/* Initialize channel                           */
	/************************************************/
	write_log(LOG_DEBUG, "Initialize channel...");
	ez_ret_val = EZapiChannel_Initialize(0);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "nps_bringup failed: EZapiChannel_Initialize returned an error (0x%08x).", ez_ret_val);
		return false;
	}

	/************************************************/
	/* Initialized state                            */
	/************************************************/
	write_log(LOG_DEBUG, "Initialized state...");
	if (infra_initialized() == false) {
		write_log(LOG_CRIT, "nps_bringup failed: infra_initialized returned an error.");
		return false;
	}

	/************************************************/
	/* Finalize channel                             */
	/************************************************/
	write_log(LOG_DEBUG, "Finalize channel...");
	ez_ret_val = EZapiChannel_Finalize(0);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "nps_bringup failed: EZapiChannel_Finalize returned an error (0x%08x).", ez_ret_val);
		return false;
	}

	/************************************************/
	/* Finalized state                              */
	/************************************************/
	write_log(LOG_DEBUG, "Finalized state...");
	if (infra_finalized() == false) {
		write_log(LOG_CRIT, "nps_bringup failed: infra_finalized returned an error.");
		return false;
	}

	/************************************************/
	/* Channel GO                                   */
	/************************************************/
	write_log(LOG_DEBUG, "Channel GO...");
	ez_ret_val = EZapiChannel_Go(0);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "nps_bringup failed: EZapiChannel_Go returned an error (0x%08x).", ez_ret_val);
		return false;
	}

	/************************************************/
	/* Running state:                               */
	/************************************************/
	write_log(LOG_DEBUG, "Running state...");
	if (infra_running() == false) {
		write_log(LOG_CRIT, "nps_bringup failed: infra_running returned an error.");
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
	write_log(LOG_DEBUG, "Initialize Env...");
	ez_ret_val = EZenv_Create();
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "nps_init: EZenv_Create failed.");
		return false;
	}
	is_object_allocated[object_type_env] = true;

	/************************************************/
	/* Initialize EZdev                             */
	/************************************************/
	write_log(LOG_DEBUG, "Init EZdev...");
	platform_params.uiChipPhase = EZdev_CHIP_PHASE_1;
#ifdef EZ_SIM
	write_log(LOG_INFO, "Platform is simulator.");
	platform_params.ePlatform = EZdev_Platform_SIM;
	platform_params.uiSimNumberOfConnections = 1;
#else
	write_log(LOG_DEBUG, "Platform is NPS chip.");
	platform_params.ePlatform = EZdev_Platform_UIO;
#endif
	ez_ret_val = EZdev_Create(&platform_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "nps_init: EZdev_Create failed.");
		return false;
	}
	is_object_allocated[object_type_dev] = true;

	/************************************************/
	/* Create and run CP library                    */
	/************************************************/
	write_log(LOG_DEBUG, "Create CP library...");
	ez_ret_val = EZapiCP_Create();
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "nps_init: EZapiCP_Create failed.");
		return false;
	}
	is_object_allocated[object_type_cp] = true;

	write_log(LOG_DEBUG, "Run CP library...");
	ez_ret_val = EZapiCP_Go();
	if (EZrc_IS_ERROR(ez_ret_val)) {
		write_log(LOG_CRIT, "nps_init: EZapiCP_Go failed.");
		return false;
	}

	/************************************************/
	/* Enable AGT debug agent interface             */
	/************************************************/
	if (system_cfg_is_agt_en() == true) {
		write_log(LOG_INFO, "Enable AGT...");
		if (infra_enable_agt() == false) {
			write_log(LOG_CRIT, "nps_init: infra_enable_agt failed.");
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
	write_log(LOG_INFO, "Got signal! %d", signum);

	if (signum != SIGTERM) {
		pthread_t self = pthread_self();

		if (self == main_thread) {
			main_thread_graceful_stop();
			exit(1);
		}
		if (self == nw_db_manager_thread) {
			nw_db_manager_exit_with_error();
		}
#ifdef CONFIG_ALVS
		if (self == alvs_db_manager_thread) {
			alvs_db_manager_exit_with_error();
		}
#endif
#ifdef CONFIG_TC
		if (self == tc_db_manager_thread) {
			tc_db_manager_exit_with_error();
		}
#endif
		if (self == dp_load_thread) {
			dp_load_exit_with_error();
		}
		if (self == cli_thread) {
			cli_manager_exit();
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

	write_log(LOG_INFO, "Shut down ALVS daemon.");

	if (fd >= 0) {
		close(fd);
		unlink("/var/lock/nps_cp.lock");
	}

	cancel_application_flag = true;
	if (is_object_allocated[object_type_dp_load_thread]) {
		pthread_join(dp_load_thread, NULL);
	}
	if (is_object_allocated[object_type_nw_db_manager]) {
		pthread_join(nw_db_manager_thread, NULL);
	}
	if (is_object_allocated[object_type_alvs_db_manager]) {
		pthread_join(alvs_db_manager_thread, NULL);
	}
	if (is_object_allocated[object_type_cli_thread]) {
		pthread_join(cli_thread, NULL);
	}

	if (is_object_allocated[object_type_tc_db_manager]) {
		pthread_join(tc_db_manager_thread, NULL);
	}

	infra_db_destructor();

	if (is_object_allocated[object_type_agt]) {
		write_log(LOG_DEBUG, "Delete AGT");
		infra_disable_agt();
	}
	if (is_object_allocated[object_type_cp]) {
		write_log(LOG_DEBUG, "Delete CP");
		ez_ret_val = EZapiCP_Delete();
		if (EZrc_IS_ERROR(ez_ret_val)) {
			write_log(LOG_ERR, "main_thread_graceful_stop: EZapiCP_Delete failed.");
		}
	}
	if (is_object_allocated[object_type_dev]) {
		write_log(LOG_DEBUG, "Delete dev");
		ez_ret_val = EZdev_Delete();
		if (EZrc_IS_ERROR(ez_ret_val)) {
			write_log(LOG_ERR, "main_thread_graceful_stop: EZdev_Delete failed.");
		}
	}
	if (is_object_allocated[object_type_env]) {
		write_log(LOG_DEBUG, "Delete env");
		ez_ret_val = EZenv_Delete();
		if (EZrc_IS_ERROR(ez_ret_val)) {
			write_log(LOG_ERR, "main_thread_graceful_stop: EZenv_Delete failed.");
		}
	}

	close_log();
}
