/***************************************************************************
*
*                    Copyright by EZchip Technologies, 2015
*
*
*  Project:          cli
*  File:             cli.c
*  Description:      
*  Notes:
*
*
****************************************************************************/

#include "cli.h"

/*******************************************************************************/

static unsigned int   rule_number = 0;

unsigned int   agt_enabled = 0;
static unsigned int   agt_port = 0;

/*******************************************************************************/

void        cp_menu_CLI( void )
{
   char input_buffer[ USER_INPUT_BUFFER_LEN ];
   int  exit_menu = 0;
   int  scenario_select;

   while ( 1 != exit_menu )
   {
      printf("\n\n");
      printf("***********************************************\n");
      printf("* Menu                                        *\n");
      printf("***********************************************\n");
      printf("\nPlease enter what would you like to do\n\n");
      printf("\n0. Exit.\n");
      printf("\n1. Add rule.\n");
      printf("\n2. Delete rule.\n");
      printf("\n3. Update rule.\n");
      printf("\n4. Show all rules.\n");
      printf("\n5. Enable/disable AGT debug agent interface.\n");


      scanf ( "%9s", input_buffer );

      getchar();

      scenario_select = atoi( input_buffer );

      if( scenario_select > 5 )
      {
         printf("cp_menu_CLI: Illegal scenario selection. Must be 1 - 3\n ");
         continue;
      }

      switch( scenario_select )
      {
      case 0:
         exit_menu = 1;
         continue;
      case 1:
         cp_add_rule_CLI();
         break;
      case 2:
         cp_delete_rule_CLI();
         break;
      case 3:
         cp_update_rule_CLI();
         break;
      case 4:
         cp_show_all_rules_CLI();
         break;
      case 5:
         cp_agt_CLI();
         break;
      }
   }
}



/*******************************************************************************/


void        cp_add_rule_CLI( void )
{
   char          input_buffer[ USER_INPUT_BUFFER_LEN ];
   int           exit_add_rule = 0;
   unsigned int  source_port, dest_port, source_vlan, dest_vlan;
   status        ret_val = SUCCESS;

   while ( 1 != exit_add_rule )
   {
      printf("\n\n");
      printf("*********************************************************\n");
      printf("* Rule selection %u.                                    *\n" , rule_number);
      printf("* (To Quit press 'q' any time)                          *\n");
      printf("*********************************************************\n");
      printf("\nPlease enter the source port\n\n");

      scanf ( "%9s", input_buffer );
      if( strcmp( "q", input_buffer) == 0 )
      {
         exit_add_rule = 1;
         continue;
      }

      getchar();

      source_port = atoi( input_buffer );

      if( source_port > 3 )
      {
         printf("cp_add_rule_CLI: Illegal source port number. Must be 0 - 3\n ");
         continue;
      }

      printf("\nPlease enter the source vlan ID.\n\n");

      scanf ( "%9s", input_buffer );

      if( strcmp( "q", input_buffer) == 0 )
      {
         exit_add_rule = 1;
         continue;
      }

      source_vlan = atoi( input_buffer );

      printf("\nPlease enter the destination port.\n\n");

      scanf ( "%9s", input_buffer );

      getchar();
      if( strcmp( "q", input_buffer) == 0 )
      {
         exit_add_rule = 1;
         continue;
      }

      dest_port = atoi( input_buffer );

      if( dest_port > 3 )
      {
         printf("cp_add_rule_CLI: Illegal destination port number. Must be 0 - 3\n ");
         continue;
      }

      printf("\nPlease enter the destination vlan ID.\n\n");

      scanf ( "%9s", input_buffer );

      if( strcmp( "q", input_buffer) == 0 )
      {
         exit_add_rule = 1;
         continue;
      }

      dest_vlan = atoi( input_buffer );

      ret_val = modify_source_port_and_vlan( ADD_RULE, source_port, source_vlan, dest_port, dest_vlan );
      if( FAILED == ret_val)
      {
         printf("cp_add_rule_CLI: failed to add source port and vlan\n ");
         continue;
      }

      printf("\nRule added with\n   source port: %u,\n   vlan: %u,\n   destination port: %u,\n   vlan: %u.\n\n",
             source_port,
             source_vlan,
             dest_port,
             dest_vlan);

      rule_number++;

      while ( getchar() != '\n' );
   }
}



/*******************************************************************************/

void        cp_delete_rule_CLI( void )
{
   char          input_buffer[ USER_INPUT_BUFFER_LEN ];
   int           exit_delete_rule = 0;
   unsigned int  source_port, dest_port, source_vlan, dest_vlan;
   status        ret_val = SUCCESS;

   while ( 1 != exit_delete_rule )
   {
      printf("\n\n");
      printf("*********************************************************\n");
      printf("* Delete rule                                           *\n" );
      printf("* (To Quit press 'q' any time)                          *\n");
      printf("*********************************************************\n");
      printf("\nPlease enter the source port\n\n");

      scanf ( "%9s", input_buffer );
      if( strcmp( "q", input_buffer) == 0 )
      {
         exit_delete_rule = 1;
         continue;
      }

      getchar();

      source_port = atoi( input_buffer );

      if( source_port > 3 )
      {
         printf("cp_delete_rule_CLI: Illegal source port number. Must be 0 - 3\n ");
         continue;
      }

      printf("\nPlease enter the source vlan ID.\n\n");

      scanf ( "%9s", input_buffer );

      if( strcmp( "q", input_buffer) == 0 )
      {
         exit_delete_rule = 1;
         continue;
      }

      source_vlan = atoi( input_buffer );

      printf("\nPlease enter the destination port.\n\n");

      scanf ( "%9s", input_buffer );

      getchar();
      if( strcmp( "q", input_buffer) == 0 )
      {
         exit_delete_rule = 1;
         continue;
      }

      dest_port = atoi( input_buffer );

      if( dest_port > 3 )
      {
         printf("cp_delete_rule_CLI: Illegal destination port number. Must be 0 - 3\n ");
         continue;
      }

      printf("\nPlease enter the destination vlan ID.\n\n");

      scanf ( "%9s", input_buffer );

      if( strcmp( "q", input_buffer) == 0 )
      {
         exit_delete_rule = 1;
         continue;
      }

      dest_vlan = atoi( input_buffer );

      ret_val = modify_source_port_and_vlan( DELETE_RULE, source_port, source_vlan, dest_port, dest_vlan );
      if( FAILED == ret_val)
      {
         printf("cp_delete_rule_CLI: failed to delete rule\n ");
         continue;
      }

      printf("\nRule delete with\n   source port: %u,\n   vlan: %u,\n   destination port: %u,\n   vlan: %u.\n\n",
             source_port,
             source_vlan,
             dest_port,
             dest_vlan);

      while ( getchar() != '\n' );
   }
}



/*******************************************************************************/

void        cp_update_rule_CLI( void )
{
   char          input_buffer[ USER_INPUT_BUFFER_LEN ];
   int           exit_update_rule = 0;
   unsigned int  source_port, dest_port, source_vlan, dest_vlan;
   status        ret_val = SUCCESS;

   while ( 1 != exit_update_rule )
   {
      printf("\n\n");
      printf("*********************************************************\n");
      printf("* Update rule                                           *\n" );
      printf("* (To Quit press 'q' any time)                          *\n");
      printf("*********************************************************\n");
      printf("\nPlease enter the source port\n\n");

      scanf ( "%9s", input_buffer );
      if( strcmp( "q", input_buffer) == 0 )
      {
         exit_update_rule = 1;
         continue;
      }

      getchar();

      source_port = atoi( input_buffer );

      if( source_port > 3 )
      {
         printf("cp_update_rule_CLI: Illegal source port number. Must be 0 - 3\n ");
         continue;
      }

      printf("\nPlease enter the source vlan ID.\n\n");

      scanf ( "%9s", input_buffer );

      if( strcmp( "q", input_buffer) == 0 )
      {
         exit_update_rule = 1;
         continue;
      }

      source_vlan = atoi( input_buffer );

      printf("\nPlease enter the new destination port.\n\n");

      scanf ( "%9s", input_buffer );

      getchar();
      if( strcmp( "q", input_buffer) == 0 )
      {
         exit_update_rule = 1;
         continue;
      }

      dest_port = atoi( input_buffer );

      if( dest_port > 3 )
      {
         printf("cp_update_rule_CLI: Illegal destination port number. Must be 0 - 3\n ");
         continue;
      }

      printf("\nPlease enter the new destination vlan ID.\n\n");

      scanf ( "%9s", input_buffer );

      if( strcmp( "q", input_buffer) == 0 )
      {
         exit_update_rule = 1;
         continue;
      }

      dest_vlan = atoi( input_buffer );

      ret_val = modify_source_port_and_vlan( UPDATE_RULE, source_port, source_vlan, dest_port, dest_vlan );
      if( FAILED == ret_val)
      {
         printf("cp_update_rule_CLI: failed to delete rule\n ");
         continue;
      }

      printf("\nRule update with\n   source port: %u,\n   vlan: %u,\n   destination port: %u,\n   vlan: %u.\n\n",
             source_port,
             source_vlan,
             dest_port,
             dest_vlan);

      while ( getchar() != '\n' );
   }
}



/*******************************************************************************/

void        cp_show_all_rules_CLI( void )
{
   EZapiStruct_IteratorParams     iterator_params;
   EZapiEntry                     entry;
   unsigned char                  entry_key[ 4 ];
   unsigned char                  entry_result[ 20 ];
   int                            exit_show_all_rules = 0, channel_id = 0;
   unsigned int                   source_port, dest_port, source_vlan, dest_vlan;

   while ( 1 != exit_show_all_rules )
   {
      printf("\n\n");
      printf("*********************************************************\n");
      printf("* All Rules:                                            *\n");
      printf("*********************************************************\n");

      entry.uiKeySize = 4;
      entry.uiResultSize = 20;
      entry.pucKey = entry_key;
      entry.pucResult = entry_result;
      iterator_params.uiChannelId = channel_id;
      iterator_params.psEntry = &entry;

      EZapiStruct_Iterate( 0, EZapiStruct_IterateCmd_Create, &iterator_params );
      while ( EZapiStruct_Iterate( 0, EZapiStruct_IterateCmd_GetNext, &iterator_params ) == EZrc_CP_SRH_ENTRY_FOUND )
      {
         get_source_port_and_vlan( &entry, &source_port, &source_vlan, &dest_port, &dest_vlan );

         printf( "\nRule \n   source port: %u,\n   vlan: %u,\n   destination port: %u,\n   vlan: %u.\n\n",
                 source_port,
                 source_vlan,
                 dest_port,
                 dest_vlan);
      }

      while ( getchar() != '\n' );
      exit_show_all_rules = 1;
   }
}



/*******************************************************************************/

void        cp_agt_CLI( void )
{
   char        input_buffer[ USER_INPUT_BUFFER_LEN ];
   int         exit_agt = 0;
   status      ret_val = SUCCESS;

   while ( 1 != exit_agt )
   {
      printf("\n\n");
      printf("*********************************************************\n");
      printf("* Agent selection                                       *\n");
      printf("* (To Quit press 'q' any time)                          *\n");
      printf("*********************************************************\n");

      if( 0 == agt_enabled )
      {
         printf("\nPlease enter the Agent port\n\n");

         scanf ( "%9s", input_buffer );
         if( strcmp( "q", input_buffer) == 0 )
         {
            exit_agt = 1;
            continue;
         }

         getchar();

         agt_port = atoi( input_buffer );

         ret_val = create_agt( agt_port );
         if( FAILED == ret_val)
         {
            printf("cp_agt_CLI: failed to create agent\n ");
            continue;
         }

         printf("\nAgent created on port %u\n\n", agt_port );
         agt_enabled = 1;
         while ( getchar() != '\n' );
      }
      else
      {
         printf("\nWould you like to close the Agent port %u? (y/n)\n\n", agt_port );
         scanf ( "%9s", input_buffer );

         if( strcmp( "y", input_buffer) == 0 )
         {
            ret_val = delete_agt( );
            if( FAILED == ret_val)
            {
               printf("cp_agt_CLI: failed to delete agent\n ");
               return;
            }

            agt_enabled = 0;
         }
      }

      exit_agt = 1;

      return;
   }
}



/*******************************************************************************/

