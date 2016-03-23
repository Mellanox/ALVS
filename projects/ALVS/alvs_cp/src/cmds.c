/***************************************************************************
*
*                    Copyright by EZchip Technologies, 2015
*
*
*  Project:          cmds
*  File:             cmds.c
*  Description:      
*  Notes:
*
*
****************************************************************************/

#include "cmds.h"
//#include <ezdp.h>

static EZagtRPCServer   host_server;

/*******************************************************************************/
static
void       EZhost_RPCJSONTask( void )
{
   EZagtRPC_ServerRun(host_server);
}



/*************************************************************************************/


status modify_source_port_and_vlan( rule_operation in_rule_operation,
                                    unsigned int source_port,
                                    unsigned int source_vlan,
                                    unsigned int dest_port __unused,
                                    unsigned int dest_vlan )
{
   EZapiEntry                       entry;
   EZapiStruct_EntryParams          entry_params;
   EZchannelMap                     channel_map;
   unsigned char                    mask  [ 4 ];
   struct search_forwarding_key     search_key;
   struct search_forwarding_result  search_result;
   EZstatus                         ez_ret_val = EZok;

   memset( &search_key, 0, sizeof( search_key ) );
   memset( &search_result, 0, sizeof( search_result ) );
   memset( &channel_map, 0, sizeof( channel_map ) );
   memset( &entry, 0, sizeof( entry ) );
   memset( &entry_params, 0, sizeof( entry_params ) );
   memset( &mask, 0, sizeof( mask ) );

   /* Fill the data for the entry. */
   channel_map.bSingleDest = FALSE;
   channel_map.uDest.aucChanMap[0] = 0x01;

   search_key.source_port = source_port;
   search_key.vlan = cp_to_dp16( source_vlan );

   /* Fill the destination vlan. */
   set_dp_bitfield( search_result.raw_data,
                    SEARCH_FORWARDING_RESULT_VLAN_SIZE,
                    1,
                    SEARCH_FORWARDING_RESULT_NEXT_DEST_SIZE,
                    dest_vlan );

   set_dp_bitfield( search_result.raw_data,
                    20,
                    1,
                    0,
                    (interfaces_mapping[dest_port].if_side << 19) | (get_flow_id( interfaces_mapping[dest_port].tx_base_oq_channel ) ) );

   entry.pucKey       = (unsigned char*)&search_key;
   entry.pucMask      = mask;
   entry.pucResult    = (unsigned char*)&search_result.raw_data;
   entry.uiKeySize    = sizeof(search_key);
   entry.uiResultSize = 8;

   switch( in_rule_operation )
   {
   case ADD_RULE:
      ez_ret_val = EZapiStruct_AddEntry( 0, &channel_map, &entry, &entry_params );
      if ( EZrc_IS_ERROR ( ez_ret_val ) )
      {
         printf("modify_source_port_and_vlan: EZapiStruct_AddEntry failed with key 0x%x.\n", *entry.pucKey);
         return FAILED;
      }
      break;

   case DELETE_RULE:
      ez_ret_val = EZapiStruct_DeleteEntry( 0, &channel_map, &entry, &entry_params );
      if ( EZrc_IS_ERROR ( ez_ret_val ) )
      {
         printf("modify_source_port_and_vlan: EZapiStruct_DeleteEntry failed with key 0x%x.\n", *entry.pucKey);
         return FAILED;
      }

      if( EZrc_CP_SRH_ENTRY_NOT_FOUND_INF == ez_ret_val )
      {
         printf("modify_source_port_and_vlan: EZapiStruct_DeleteEntry failed with key 0x%x. No such rule\n", *entry.pucKey);
         return FAILED;
      }
      break;

   case UPDATE_RULE:
      ez_ret_val = EZapiStruct_UpdateEntry( 0, &channel_map, &entry, &entry_params );
      if ( EZrc_IS_ERROR ( ez_ret_val ) )
      {
         printf("modify_source_port_and_vlan: EZapiStruct_UpdateEntry failed with key 0x%x.\n", *entry.pucKey);
         return FAILED;
      }

      if( EZrc_CP_SRH_ENTRY_NOT_FOUND_INF == ez_ret_val )
      {
         printf("modify_source_port_and_vlan: EZapiStruct_UpdateEntry failed. No such rule\n");
         return FAILED;
      }
      break;
   }

   return SUCCESS;
}



/*************************************************************************************/


void     get_source_port_and_vlan( EZapiEntry    *entry,
                                   unsigned int  *source_port,
                                   unsigned int  *source_vlan,
                                   unsigned int  *dest_port,
                                   unsigned int  *dest_vlan )
{
   unsigned int                     temp_oq, temp_port, next_destination, flow_id, side;
   struct search_forwarding_key     search_key;
   struct search_forwarding_result  search_result;

   if( ( entry == NULL ) || ( entry->pucKey == NULL ) || ( entry->pucResult == NULL ) )
   {
      return;
   }

   memcpy(&search_key, entry->pucKey, entry->uiKeySize );
   *source_port = search_key.source_port;
   *source_vlan = dp_to_cp16(search_key.vlan);

   memcpy(&search_result.raw_data, entry->pucResult, entry->uiResultSize );

   *dest_vlan = get_dp_bitfield( search_result.raw_data,
                                 SEARCH_FORWARDING_RESULT_VLAN_SIZE,
                                 1,
                                 SEARCH_FORWARDING_RESULT_NEXT_DEST_SIZE );

   next_destination  = get_dp_bitfield( search_result.raw_data,
                                        SEARCH_FORWARDING_RESULT_NEXT_DEST_SIZE,
                                        1,
                                        0 );

   flow_id = next_destination & 0x7FFFF;

   side = next_destination >> 19;

   /* find the TX oq channel according to flow_id */
   for( temp_oq = 0; temp_oq < 128; temp_oq++ )
   {
      if( get_flow_id( temp_oq ) == flow_id )
      {
         for( temp_port = 0; temp_port < NUMBER_OF_PORTS; temp_port++ )
         {
            if( ( interfaces_mapping[ temp_port ].tx_base_oq_channel == temp_oq ) &&
                ( interfaces_mapping[ temp_port ].if_side == side ) )
            {
               *dest_port = temp_port;
            }
         }
      }
   }

   return;
}



/*******************************************************************************/

status    create_agt( unsigned int agt_port )
{
   EZtask      task;
   EZstatus    ez_ret_val = EZok;

   /* Create rpc server for given port */
   host_server = EZagtRPC_CreateServer( agt_port );
   if( NULL == host_server )
   {
      printf( "create_agt: Cannot create agt server" );
      return FAILED;
   }

   /* Register standard CP commands */
   ez_ret_val = EZagt_RegisterFunctions( host_server );
   if ( EZrc_IS_ERROR( ez_ret_val ) )
   {
      printf( "create_agt: Cannot register standard CP functions" );
      return FAILED;
   }

   /* Register standard CP commands */
   ez_ret_val = EZagtCPMain_RegisterFunctions( host_server );
   if ( EZrc_IS_ERROR( ez_ret_val ) )
   {
      printf( "create_agt: Cannot register CP main functions" );
      return FAILED;
   }

   /* Create task for run rpc-json commands  */
   task = EZosTask_Spawn( "agt",
                           EZosTask_NORMAL_PRIORITY,
                           0x100000,
                           (EZosTask_Spawn_FuncPtr)EZhost_RPCJSONTask,
                           0 );

   if(  EZosTask_INVALID_TASK == task )
   {
      printf( "create_agt: Cannot spawn agt thread" );
      return FAILED;
   }

   return SUCCESS;
}



/*******************************************************************************/

status    delete_agt( void )
{
   EZagtRPC_ServerStop( host_server );
   EZosTask_Delay(10);
   EZagtRPC_ServerDestroy( host_server );

   return SUCCESS;
}



/*******************************************************************************/

