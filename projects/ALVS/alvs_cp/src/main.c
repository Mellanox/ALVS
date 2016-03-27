/***************************************************************************
*
*                    Copyright by EZchip Technologies, 2015
*
*
*  Project:          NPS $(projectName)_cp
*  File:             main.c
*  Description:      Main for linux platform.
*  Notes:
*
*
****************************************************************************/

#include <stdio.h>
#include <EZapiChannel.h>
#include <EZapiStruct.h>
#include <EZapiPrm.h>
#include <EZapiCP.h>

#include <EZapiStruct.h>
#include <EZagtRPC.h>
#include <EZagtCPMain.h>
#include <EZsocket.h>
#include <EZenv.h>
#include <EZdev.h>

#include "utils.h"

/******************************************************************************/

extern EZbool           EZdevSim_WaitForInitSocket( EZui32 uiNumberOfConnections );

/******************************************************************************
* Name:     init_cp
* Desc:     Initialize all cp modules.
* Args:
* Notes:
* Return:
*/

static
status          init_cp( void );



/******************************************************************************
* Name:     init_cp
* Desc:     Initialize all board modules.
* Args:
* Notes:
* Return:
*/

static
status          init_board( void );



/******************************************************************************
* Name:     delete_cp
* Desc:     Close all cp modules.
* Args:
* Notes:
* Return:
*/

static
status          delete_cp( void );



/******************************************************************************
* Name:     delete_board
* Desc:     Close all board modules.
* Args:
* Notes:
* Return:
*/

static
status          delete_board( void );



/*******************************************************************************
* Name:     setup_chip
* Desc:     run cp configuration
* Args:
* Notes:
* Return:
*/

static
status          setup_chip( void );



/*******************************************************************************/


int       main( void )
{
   status       ret_val = SUCCESS;

   /* The $(projectName)_cp initialize all the modules needed for the cp,
    * it also initializes all the board configurations.
    * After all is initializes and the cp is running, it configures 4 10GE interface- 2 in each side.
    * After the initialization and configuration is done, a CLI menu opens and let you choose what would you like to do:
    * 0. Exit - this will delete all the configurations from the chip and will exit the app.
    * 1. Add rule
    * 2. Delete rule
    * 3. Update rule
    * 4. Show all rules
    * 5. Enable/disable AGT debug agent interface */

   /************************************************/
   /* Initializing CP SDK host components          */
   /************************************************/

   ret_val = init_board();
   if ( FAILED == ret_val )
   {
      printf("main: init_board failed.\n");
      return ret_val;
   }

   ret_val = init_cp();
   if ( FAILED == ret_val )
   {
      printf("main: init_cp failed.\n");
      return ret_val;
   }

   /************************************************/
   /* Initializing control plane components        */
   /************************************************/

   ret_val = setup_chip();
   if ( FAILED == ret_val )
   {
      printf("main: setup_chip failed.\n");
      return ret_val;
   };


   while(1){}
   delete_cp();
   delete_board();

   return ret_val;
}


/**********************************************************************/
static
status    setup_chip( void )
{
   unsigned int                    pup_phase, channel_id = 0, ind;
   EZapiChannel_IntMemSpaceParams  int_mem_space_params;
   EZapiChannel_EthIFParams        eth_if_params;
   EZapiChannel_EthRXChannelParams eth_rx_channel_params;
   EZapiChannel_PMUQueueParams     pmu_queue_params;
   EZapiStruct_StructParams        struct_params;
   EZapiStruct_HashParams          hash_params;
   EZapiStruct_HashMemMngParams    hash_mem_mng_params;
   EZchannelMap                    channel_map;
   EZstatus                        ez_ret_val = EZok;

   /* Create channel */
   ez_ret_val = EZapiChannel_Create( channel_id );
   if ( EZrc_IS_ERROR ( ez_ret_val ) )
   {
      printf("setup_chip: EZapiChannel_Create failed.\n" );
      return FAILED;
   }

   /* Configure internal memory space parameters */
   memset( &int_mem_space_params, 0, sizeof( int_mem_space_params ) );

   int_mem_space_params.uiIndex = 2;

   ez_ret_val = EZapiChannel_Status( channel_id, EZapiChannel_StatCmd_GetIntMemSpaceParams, &int_mem_space_params );
   if ( EZrc_IS_ERROR ( ez_ret_val ) )
   {
      printf("setup_chip: EZapiChannel_StatCmd_GetIntMemSpaceParams failed.\n" );
      return FAILED;
   }

   int_mem_space_params.bEnable = TRUE;
   int_mem_space_params.eType   = EZapiChannel_IntMemSpaceType_1_CLUSTER_DATA;
   int_mem_space_params.uiSize  = 128;

   ez_ret_val = EZapiChannel_Config( channel_id, EZapiChannel_ConfigCmd_SetIntMemSpaceParams, &int_mem_space_params );
   if ( EZrc_IS_ERROR ( ez_ret_val ) )
   {
      printf("setup_chip: EZapiChannel_ConfigCmd_SetIntMemSpaceParams failed.\n" );
      return FAILED;
   }

   memset( &int_mem_space_params, 0, sizeof( int_mem_space_params ) );

   int_mem_space_params.uiIndex = 3;

   ez_ret_val = EZapiChannel_Status( channel_id, EZapiChannel_StatCmd_GetIntMemSpaceParams, &int_mem_space_params );
   if ( EZrc_IS_ERROR ( ez_ret_val ) )
   {
      printf("setup_chip: EZapiChannel_StatCmd_GetIntMemSpaceParams failed.\n" );
      return FAILED;
   }

   int_mem_space_params.bEnable = TRUE;
   int_mem_space_params.eType   = EZapiChannel_IntMemSpaceType_1_CLUSTER_CODE;
   int_mem_space_params.uiSize  = 64;

   ez_ret_val = EZapiChannel_Config( channel_id, EZapiChannel_ConfigCmd_SetIntMemSpaceParams, &int_mem_space_params );
   if ( EZrc_IS_ERROR ( ez_ret_val ) )
   {
      printf("setup_chip: EZapiChannel_ConfigCmd_SetIntMemSpaceParams failed.\n" );
      return FAILED;
   }

   memset( &int_mem_space_params, 0, sizeof( int_mem_space_params ) );

   int_mem_space_params.uiIndex = 4;

   ez_ret_val = EZapiChannel_Status( channel_id, EZapiChannel_StatCmd_GetIntMemSpaceParams, &int_mem_space_params );
   if ( EZrc_IS_ERROR ( ez_ret_val ) )
   {
      printf("setup_chip: EZapiChannel_StatCmd_GetIntMemSpaceParams failed.\n" );
      return FAILED;
   }

   int_mem_space_params.bEnable = TRUE;
   int_mem_space_params.eType   = EZapiChannel_IntMemSpaceType_16_CLUSTER_SEARCH;
   int_mem_space_params.uiSize  = 512;

   ez_ret_val = EZapiChannel_Config( 0, EZapiChannel_ConfigCmd_SetIntMemSpaceParams, &int_mem_space_params );
   if ( EZrc_IS_ERROR ( ez_ret_val ) )
   {
      printf("setup_chip: EZapiChannel_ConfigCmd_SetIntMemSpaceParams failed.\n" );
      return FAILED;
   }

   memset( &interfaces_mapping, 0, ( sizeof( interfaces_mapping ) ) );

   interfaces_mapping[ 0 ].if_side            = 0;
   interfaces_mapping[ 0 ].if_engine          = 0;
   interfaces_mapping[ 0 ].if_type            = EZapiChannel_EthIFType_10GE;
   interfaces_mapping[ 0 ].if_number          = 0;
   interfaces_mapping[ 0 ].tx_base_oq_channel = 0;

   interfaces_mapping[ 1 ].if_side            = 1;
   interfaces_mapping[ 1 ].if_engine          = 0;
   interfaces_mapping[ 1 ].if_type            = EZapiChannel_EthIFType_10GE;
   interfaces_mapping[ 1 ].if_number          = 0;
   interfaces_mapping[ 1 ].tx_base_oq_channel = 0;

   /* Configure the interfaces */
   for( ind = 0; ind < NUMBER_OF_PORTS; ind++ )
   {
      eth_if_params.uiSide     = interfaces_mapping[ ind ].if_side;
      eth_if_params.uiIFEngine = interfaces_mapping[ ind ].if_engine;
      eth_if_params.eEthIFType = interfaces_mapping[ ind ].if_type;
      eth_if_params.uiIFNumber = interfaces_mapping[ ind ].if_number;

      ez_ret_val = EZapiChannel_Status( channel_id, EZapiChannel_StatCmd_GetEthIFParams, &eth_if_params );
      if ( EZrc_IS_ERROR ( ez_ret_val ) )
      {
         printf("setup_chip: EZapiChannel_StatCmd_GetEthIFParams failed.\n" );
         return FAILED;
      }

      eth_if_params.bRXEnable  = TRUE;
      eth_if_params.bTXEnable  = TRUE;

      ez_ret_val = EZapiChannel_Config( channel_id, EZapiChannel_ConfigCmd_SetEthIFParams, &eth_if_params );
      if ( EZrc_IS_ERROR ( ez_ret_val ) )
      {
         printf("setup_chip: EZapiChannel_ConfigCmd_SetEthIFParams failed.\n" );
         return FAILED;
      }
   }

   /* Configure ethernet RX parameters for the interfaces */
   for( ind = 0; ind < NUMBER_OF_PORTS; ind++ )
   {
      eth_rx_channel_params.uiSide      = interfaces_mapping[ ind ].if_side;
      eth_rx_channel_params.uiIFEngine  = interfaces_mapping[ ind ].if_engine;
      eth_rx_channel_params.eEthIFType  = interfaces_mapping[ ind ].if_type;
      eth_rx_channel_params.uiIFNumber  = interfaces_mapping[ ind ].if_number;
      eth_rx_channel_params.uiRXChannel = 0;

      ez_ret_val = EZapiChannel_Status( channel_id, EZapiChannel_StatCmd_GetEthRXChannelParams, &eth_rx_channel_params );
      if ( EZrc_IS_ERROR ( ez_ret_val ) )
      {
         printf("setup_chip: EZapiChannel_StatCmd_GetEthRXChannelParams failed.\n" );
         return FAILED;
      }

      eth_rx_channel_params.uiELBDSize  = 32;
      eth_rx_channel_params.uiLogicalID = ind;

      ez_ret_val = EZapiChannel_Config( channel_id, EZapiChannel_ConfigCmd_SetEthRXChannelParams, &eth_rx_channel_params );
      if ( EZrc_IS_ERROR ( ez_ret_val ) )
      {
         printf("setup_chip: EZapiChannel_ConfigCmd_SetEthRXChannelParams failed.\n" );
         return FAILED;
      }
   }

   /* Configure PMU queue parameters for both side */
   for( ind = 0; ind < 2; ind++ )
   {
      pmu_queue_params.uiSide      = ind;
      pmu_queue_params.uiQueue     = 0;

      ez_ret_val = EZapiChannel_Status( channel_id, EZapiChannel_StatCmd_GetPMUQueueParams, &pmu_queue_params );
      if ( EZrc_IS_ERROR ( ez_ret_val ) )
      {
         printf("setup_chip: EZapiChannel_StatCmd_GetPMUQueueParams failed.\n" );
         return FAILED;
      }

      pmu_queue_params.uiScheduler = 0;

      ez_ret_val = EZapiChannel_Config( channel_id, EZapiChannel_ConfigCmd_SetPMUQueueParams, &pmu_queue_params );
      if ( EZrc_IS_ERROR ( ez_ret_val ) )
      {
         printf("setup_chip: EZapiChannel_ConfigCmd_SetPMUQueueParams failed.\n" );
         return FAILED;
      }
   }

   /* Call power-up to power-up the channel */
   for( pup_phase = 1 ; pup_phase <= 4 ; pup_phase++ )
   {
      ez_ret_val = EZapiChannel_PowerUp( channel_id, pup_phase );
      if ( EZrc_IS_ERROR ( ez_ret_val ) )
      {
         printf("setup_chip: EZapiChannel_PowerUp phase %u failed.\n", pup_phase );
         return FAILED;
      }
   }

   /* Call initialize to initialize the channel */
   ez_ret_val = EZapiChannel_Initialize( channel_id );
   if ( EZrc_IS_ERROR ( ez_ret_val ) )
   {
      printf("setup_chip: EZapiChannel_Initialize failed.\n" );
      return FAILED;
   }

   /* Create HASH structure to hold the entries */
   memset( &struct_params, 0, sizeof( struct_params ) );
   memset( &channel_map, 0, sizeof( channel_map ) );

   ez_ret_val = EZapiStruct_Status( 0, EZapiStruct_StatCmd_GetStructParams, &struct_params );
   if ( EZrc_IS_ERROR ( ez_ret_val ) )
   {
      printf("setup_chip: EZapiStruct_StatCmd_GetStructParams failed.\n" );
      return FAILED;
   }

   channel_map.bSingleDest         = FALSE;
   channel_map.uDest.aucChanMap[0] = 0x01;
   struct_params.bEnable           = TRUE;
   struct_params.eStructType       = EZapiStruct_StructType_HASH;
   struct_params.eStructMemoryArea = EZapiStruct_MemoryArea_INTERNAL;
   struct_params.uiKeySize         = 3;
   struct_params.uiResultSize      = 8;
   struct_params.uiMaxEntries      = 4096;
   struct_params.sChannelMap       = channel_map;

   ez_ret_val = EZapiStruct_Config( 0, EZapiStruct_ConfigCmd_SetStructParams, &struct_params );
   if ( EZrc_IS_ERROR ( ez_ret_val ) )
   {
      printf("setup_chip: EZapiStruct_ConfigCmd_SetStructParams failed\n." );
      return FAILED;
   }

   /* Configure the HASH parameters */
   memset( &hash_params, 0, sizeof( hash_params ) );

   ez_ret_val = EZapiStruct_Status( 0, EZapiStruct_StatCmd_GetHashParams, &hash_params );
   if ( EZrc_IS_ERROR ( ez_ret_val ) )
   {
      printf("setup_chip: EZapiStruct_StatCmd_GetHashParams failed.\n" );
      return FAILED;
   }

   hash_params.eMultiChannelDataMode = EZapiStruct_MultiChannelDataMode_IDENTICAL;
   hash_params.bSingleCycle          = TRUE;

   ez_ret_val = EZapiStruct_Config( 0, EZapiStruct_ConfigCmd_SetHashParams, &hash_params );
   if ( EZrc_IS_ERROR ( ez_ret_val ) )
   {
      printf("setup_chip: EZapiStruct_ConfigCmd_SetHashParams failed.\n" );
      return FAILED;
   }

   /* Configure the HASH memory management */
   memset( &hash_mem_mng_params, 0, sizeof( hash_mem_mng_params ) );

   ez_ret_val = EZapiStruct_Status( 0, EZapiStruct_StatCmd_GetHashMemMngParams, &hash_mem_mng_params );
   if ( EZrc_IS_ERROR ( ez_ret_val ) )
   {
      printf("setup_chip: EZapiStruct_StatCmd_GetHashMemMngParams failed.\n" );
      return FAILED;
   }

   hash_mem_mng_params.uiHashSize = 8;
   hash_mem_mng_params.eSigPageMemoryArea = EZapiStruct_MemoryArea_INTERNAL;
   hash_mem_mng_params.uiMainTableSpaceIndex = 4;
   hash_mem_mng_params.uiSigSpaceIndex = 4;
   hash_mem_mng_params.uiResSpaceIndex = 4;

   ez_ret_val = EZapiStruct_Config( 0, EZapiStruct_ConfigCmd_SetHashMemMngParams, &hash_mem_mng_params );
   if ( EZrc_IS_ERROR ( ez_ret_val ) )
   {
      printf("setup_chip: EZapiStruct_ConfigCmd_SetHashMemMngParams failed.\n" );
      return FAILED;
   }

   /* Load partition */
   ez_ret_val = EZapiStruct_PartitionConfig( 0, EZapiStruct_PartitionConfigCmd_LoadPartition, NULL );
   if ( EZrc_IS_ERROR ( ez_ret_val ) )
   {
      printf("setup_chip: EZapiStruct_PartitionConfigCmd_LoadPartition failed.\n" );
      return FAILED;
   }

   /* Call finalize to finalize the channel */
   ez_ret_val = EZapiChannel_Finalize( channel_id );
   if ( EZrc_IS_ERROR ( ez_ret_val ) )
   {
      printf("setup_chip: EZapiChannel_Finalize failed.\n" );
      return FAILED;
   }

   /* Call go to start the channel to run */
   ez_ret_val = EZapiChannel_Go( channel_id );
   if ( EZrc_IS_ERROR ( ez_ret_val ) )
   {
      printf("setup_chip: EZapiChannel_Go failed.\n" );
      return FAILED;
   }

   return SUCCESS;
}



/*******************************************************************************/

static
status    init_cp( void )
{
   EZstatus               ez_ret_val = EZok;
   status                 ret_val = SUCCESS;

   /* EZenv creation */
   ez_ret_val = EZenv_Create();
   if ( EZrc_IS_ERROR( ez_ret_val ) )
   {
      printf("init_cp: EZenv_Create failed.\n" );
      return FAILED;
   }

   /* EZcp creation */

   /* Create the CP library. */
   ez_ret_val = EZapiCP_Create( );
   if( EZrc_IS_ERROR( ez_ret_val ) )
   {
      printf("init_cp: EZapiCP_Create failed.\n" );
      return FAILED;
   }

   /* Start the CP library operation. */
   ez_ret_val = EZapiCP_Go();
   if( EZrc_IS_ERROR( ez_ret_val ) )
   {
      printf("init_cp: EZapiCP_Go failed.\n" );
      return FAILED;
   }

   return ret_val;
}



/*******************************************************************************/

static
status    init_board( void )
{
   EZdev_PlatformParams  platform_params;
   EZstatus              ez_ret_val = EZok;
   status                ret_val = SUCCESS;
#ifndef EZ_CPU_ARC
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
         return FAILED;
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
         return FAILED;
      }

      ez_ret_val = EZdevSim_WaitForInitSocket( 1 );
      if ( EZrc_IS_ERROR( ez_ret_val ) )
      {
         printf("init_board: EZdevSim_WaitForInitSocket failed.\n" );
         return FAILED;
      }
   }

#endif

   return ret_val;
}



/*******************************************************************************/

static
status    delete_cp( void )
{
   EZstatus   ez_ret_val = EZok;


   ez_ret_val = EZapiCP_Delete( );
   if( EZrc_IS_ERROR( ez_ret_val ) )
   {
      printf("delete_cp: EZapiCP_Delete failed.\n" );
      return FAILED;
   }

   ez_ret_val = EZenv_Delete();
   if ( EZrc_IS_ERROR( ez_ret_val ) )
   {
      printf("delete_cp: EZenv_Delete failed.\n" );
      return FAILED;
   }

   return SUCCESS;
}



/*******************************************************************************/

static
status    delete_board( void )
{
   EZstatus     ez_ret_val = EZok;

   ez_ret_val = EZdev_Delete();
   if ( EZrc_IS_ERROR( ez_ret_val ) )
   {
      printf("delete_board: EZdev_Delete failed.\n" );
      return FAILED;
   }

   return SUCCESS;
}



/*******************************************************************************/

