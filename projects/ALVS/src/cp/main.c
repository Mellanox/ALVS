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
#include <EZenv.h>
#include <EZdev.h>

#include "interface.h"
#include "memory.h"
#include "search.h"



/******************************************************************************/

extern EZbool
EZdevSim_WaitForInitSocket( EZui32 uiNumberOfConnections );

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

/******************************************************************************/


int main( void )
{
	/************************************************/
	/* Run in the background as a daemon            */
	/************************************************/
	daemon(0,0);

	/************************************************/
	/* Initializing CP SDK host components          */
	/************************************************/
	printf("init board...\n");
	if (init_board() == false) {
		return 1;
	}

	printf("init cp...\n");
	if (init_cp() == false) {
		return 1;
	}

	/************************************************/
	/* Initializing control plane components        */
	/************************************************/
	printf("setup chip...\n");
	if ( setup_chip() == false ) {
		return 1;
	};


	while(1){}
	delete_cp();
	delete_board();

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
	ez_ret_val = EZapiChannel_Create(0);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	/************************************************/
	/* Created state:                               */
	/* --------------                               */
	/* 2. Create interfaces mapping                 */
	/* 3. Create memory partition                   */
	/************************************************/
	alvs_create_if_mapping();
	alvs_create_mem_partition();

	/************************************************/
	/* power-up channel                             */
	/************************************************/
	for(pup_phase = 1; pup_phase <= 4; pup_phase++) {
		ez_ret_val = EZapiChannel_PowerUp(0, pup_phase);
		if (EZrc_IS_ERROR(ez_ret_val)) {
			return false;
		}
	}

	/************************************************/
	/* powered-up state:                            */
	/* --------------                               */
	/* Do nothing                                   */
	/************************************************/

	/************************************************/
	/* initialize channel                           */
	/************************************************/
	ez_ret_val = EZapiChannel_Initialize(0);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	/************************************************/
	/* initialized state:                           */
	/* --------------                               */
	/* 1. Create search_structures                  */
	/* 2. Load partition                            */
	/************************************************/
	alvs_create_all_dbs();
	alvs_load_partition();

	/************************************************/
	/* finalize channel                             */
	/************************************************/
	ez_ret_val = EZapiChannel_Finalize(0);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	/************************************************/
	/* finalized state:                             */
	/* --------------                               */
	/* Do nothing                                   */
	/************************************************/

	/************************************************/
	/* channel GO                                   */
	/************************************************/
	ez_ret_val = EZapiChannel_Go(0);
	if (EZrc_IS_ERROR(ez_ret_val)) {
		return false;
	}

	/************************************************/
	/* running state:                               */
	/* --------------                               */
	/* 1. ALVS daemon                               */
	/************************************************/


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
bool init_board( void )
{
#ifndef EZ_CPU_ARC
   EZdev_PlatformParams  platform_params;
   EZstatus              ez_ret_val;
   EZc8                  cKernelModule[16];
   FILE                  *fd;
   EZbool                bIsRealChip = FALSE;

   fd = popen("lsmod | grep nps_cp", "r");

   if ( ( fd != NULL ) )
   {
      if ( fread( cKernelModule,  1,  sizeof (cKernelModule), fd ) > 0 )
      {
         bIsRealChip = TRUE;
      }
      else
      {
         bIsRealChip = FALSE;
      }

      pclose( fd );
   }
   else
   {
      bIsRealChip = FALSE;
   }

   if ( bIsRealChip )
   {
      printf ("nps_cp module is loaded. Running on real chip.\n");
      platform_params.ePlatform = EZdev_Platform_UIO;
      /* Set chip type. */
      platform_params.uiChipPhase = EZdev_CHIP_PHASE_1;

      /* Initialize the DEV component */
      ez_ret_val = EZdev_Create( &platform_params );
      if ( EZrc_IS_ERROR( ez_ret_val ) )
      {
         printf("init_board: EZdev_Create failed.\n" );
         return false;
      }
   }
   else
   {
      printf ("nps_cp module is not loaded. Running on simulator.\n");
      /* Connect to simulator */
      /* Set platform SIM */
      platform_params.ePlatform = EZdev_Platform_SIM;
      /* Set chip type. */
      platform_params.uiChipPhase = EZdev_CHIP_PHASE_1;

      /* Initialize the DEV component */
      ez_ret_val = EZdev_Create( &platform_params );
      if ( EZrc_IS_ERROR( ez_ret_val ) )
      {
         printf("init_board: EZdev_Create failed.\n" );
         return false;
      }

      ez_ret_val = EZdevSim_WaitForInitSocket( 1 );
      if ( EZrc_IS_ERROR( ez_ret_val ) )
      {
         printf("init_board: EZdevSim_WaitForInitSocket failed.\n" );
         return false;
      }
   }
#endif

   return true;
}

static
bool delete_cp( void )
{
   EZstatus   ez_ret_val = EZok;

   ez_ret_val = EZapiCP_Delete( );
   if( EZrc_IS_ERROR( ez_ret_val ) )
   {
      printf("delete_cp: EZapiCP_Delete failed.\n" );
      return false;
   }

   ez_ret_val = EZenv_Delete();
   if ( EZrc_IS_ERROR( ez_ret_val ) )
   {
      printf("delete_cp: EZenv_Delete failed.\n" );
      return false;
   }

   return true;
}

static
bool delete_board( void )
{
   EZstatus     ez_ret_val = EZok;

   ez_ret_val = EZdev_Delete();
   if ( EZrc_IS_ERROR( ez_ret_val ) )
   {
      printf("delete_board: EZdev_Delete failed.\n" );
      return false;
   }

   return true;
}
