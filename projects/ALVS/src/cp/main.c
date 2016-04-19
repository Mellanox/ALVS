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

#include <stdint.h>
#include <signal.h>
#include <EZenv.h>
#include <EZdev.h>

#include "agt.h"
#include "infrastructure.h"
#include "search.h"

#include "nw_db_manager.h"

/******************************************************************************/

extern EZbool
EZdevSim_WaitForInitSocket(EZui32 uiNumberOfConnections);

static
bool init_cp( void );

static
bool init_board( void );

static
bool delete_cp( void );

static
bool delete_board( void );

static
bool setup_chip( void );

void signal_terminate_handler( int signum );

/******************************************************************************/
bool    is_main_process = true;
bool    is_nw_db_manager_process = false;
/******************************************************************************/

int main( void )
{
	int32_t                        cpid;
	/************************************************/
	/* Run in the background as a daemon            */
	/************************************************/
//	daemon(0,1);
	/* listen to the SHUTDOWN signal to handle terminate signal */
	signal(SIGINT,  signal_terminate_handler);
	signal(SIGTERM, signal_terminate_handler);
	signal(SIGILL,  signal_terminate_handler);
	signal(SIGSEGV, signal_terminate_handler);
	signal(SIGBUS,  signal_terminate_handler);

	/************************************************/
	/* Initializing CP SDK host components          */
	/************************************************/
	printf("Init board...\n");
	if (init_board() == false) {
		exit(1);
	}

	printf("Init cp...\n");
	if (init_cp() == false) {
		exit(1);
	}

	/************************************************/
	/* Initializing control plane components        */
	/************************************************/
	printf("Setup chip...\n");
	if (setup_chip() == false) {
		exit(1);
	}

	/************************************************/
	/* Enable AGT debug agent interface             */
	/************************************************/
	printf("Create AGT...\n");
	if (create_agt() == false) {
		exit(1);
	}


	/************************************************/
	/* Start network DB manager process             */
	/************************************************/
	cpid = fork();
	if (cpid == -1){
		perror("Error creating child process - fork fail\n");
		exit(1);
	}
	if (cpid  == 0){
		is_main_process = false;
		is_nw_db_manager_process = true;
		nw_db_manager_process();
	}

	while(true){
		sleep(0xFFFFFFFF);
	}

	return 0;
}

static
bool setup_chip( void )
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
	/* Created state:                               */
	/* --------------                               */
	/* 1. Create interfaces mapping                 */
	/* 2. Create memory partition                   */
	/* 3. Configure my MAC                          */
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
	if (infra_configure_protocol_decode() == false) {
		printf("setup_chip: infra_configure_protocol_decode failed.\n");
		return false;
	}

	/************************************************/
	/* Power-up channel                             */
	/************************************************/
	printf("Power-up channel...\n");
	for(pup_phase = 1; pup_phase <= 4; pup_phase++) {
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
	/************************************************/
	printf("Initialized state...\n");
	if (create_all_dbs() == false) {
		printf("setup_chip: create_all_dbs failed.\n");
		return false;
	}
	if (load_partition() == false) {
		printf("setup_chip: load_partition failed.\n");
		return false;
	}
	if (initialize_dbs() == false) {
		printf("setup_chip: initialize_dbs failed.\n");
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
bool init_cp( void )
{
	EZstatus ez_ret_val;

	/************************************************/
	/* Create Env                                   */
	/************************************************/
	ez_ret_val = EZenv_Create();
	if (EZrc_IS_ERROR( ez_ret_val)) {
		return false;
	}

	/************************************************/
	/* Create CP library                            */
	/************************************************/
	ez_ret_val = EZapiCP_Create( );
	if(EZrc_IS_ERROR( ez_ret_val)) {
		return false;
	}

	/************************************************/
	/* Run CP library                               */
	/************************************************/
	ez_ret_val = EZapiCP_Go();
	if(EZrc_IS_ERROR( ez_ret_val)) {
		return false;
	}

	return true;
}

static
bool init_board(void)
{
#ifndef EZ_CPU_ARC
	EZdev_PlatformParams platform_params;
	EZstatus ez_ret_val;
	EZc8 cKernelModule[16];
	FILE *fd;
	EZbool bIsRealChip = FALSE;

	fd = popen("lsmod | grep nps_cp", "r");

   if ((fd != NULL)) {
      if (fread(cKernelModule, 1, sizeof (cKernelModule), fd) > 0) {
         bIsRealChip = TRUE;
      }
      pclose( fd );
   }

   if (bIsRealChip) {
	printf ("nps_cp module is loaded. Running on real chip.\n");
	platform_params.ePlatform = EZdev_Platform_UIO;
	/* Set chip type. */
	platform_params.uiChipPhase = EZdev_CHIP_PHASE_1;

	/* Initialize the DEV component */
	ez_ret_val = EZdev_Create(&platform_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		printf("init_board: EZdev_Create failed.\n" );
		return false;
	}
   }
   else {
	printf ("nps_cp module is not loaded. Running on simulator.\n");
	/* Connect to simulator */
	/* Set platform SIM */
	platform_params.ePlatform = EZdev_Platform_SIM;
	/* Set chip type. */
	platform_params.uiChipPhase = EZdev_CHIP_PHASE_1;

	/* Initialize the DEV component */
	ez_ret_val = EZdev_Create( &platform_params);
	if (EZrc_IS_ERROR(ez_ret_val)) {
	 printf("init_board: EZdev_Create failed.\n" );
	 return false;
	}

	ez_ret_val = EZdevSim_WaitForInitSocket(1);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		printf("init_board: EZdevSim_WaitForInitSocket failed.\n");
		return false;
	}
   }
#endif

   return true;
}

static
bool delete_cp(void)
{
   EZstatus ez_ret_val;

   ez_ret_val = EZapiCP_Delete();
   if(EZrc_IS_ERROR(ez_ret_val)) {
      printf("delete_cp: EZapiCP_Delete failed.\n" );
      return false;
   }

   ez_ret_val = EZenv_Delete();
   if (EZrc_IS_ERROR( ez_ret_val)) {
      printf("delete_cp: EZenv_Delete failed.\n" );
      return false;
   }

   return true;
}

static
bool delete_board( void )
{
   EZstatus ez_ret_val;

   ez_ret_val = EZdev_Delete();
   if (EZrc_IS_ERROR(ez_ret_val)) {
      printf("delete_board: EZdev_Delete failed.\n");
      return false;
   }

   return true;
}

/************************************************************************
 * \brief      signal terminate handler
 *
 * \return     void
 */
void       signal_terminate_handler( int signum)
{
	if(is_main_process){
		printf("Received interrupt in main process %d\n", signum);
		delete_agt();
		delete_cp();
		delete_board();
		/* kill all other processes */
		killpg(0, SIGTERM);
	}

	if(is_nw_db_manager_process){
		printf("Received interrupt in nw_db_manager process %d\n", signum);

		if(signum != SIGTERM){
			/* kill all other processes */
			kill (getppid(), SIGTERM);
			sleep(0x1FFFFFFF);
		}
		else{
			nw_db_manager_delete();
		}
	}
	exit(0);
}
