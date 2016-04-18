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


/* system includes */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/wait.h>

/* dp includes */
#include <ezdp.h>
#include <ezframe.h>

/*internal includes*/
#include "defs.h"
#include "alvs_dp_defs.h"
#include "cmem_defs.h"
#include "nw_routing.h"
#include "nw_host.h"
#include "nw_recieve.h"


/******************************************************************************
 * Globals
 *****************************************************************************/

alvs_cmem			cmem        __cmem_var;
alvs_shared_cmem  	shared_cmem __cmem_shared_var;

extern void			signal_terminate_handler( int signum );
extern void       	signal_terminate_handler_gracefully_stop( int signum );
extern void			add_run_cpus( char* processors_str );
extern bool			is_child_process;
extern uint32_t 	num_cpus;
extern int32_t 		run_cpus[];
extern uint32_t 	pids[];

 /******************************************************************************
 * \brief		Main packet processing function
 *
 * \note		 Receive frame

 * \return	  void
 */
void		  packet_processing(void) __fast_path_code;
void		  packet_processing(void)
{
	int32_t logical_id;
	uint8_t * my_mac;

	//before packet processing task save my mac address to shared cmem
	if ((my_mac = nw_interface_get_mac_address(ALVS_NETWORK_PORT_LOGICAL_ID)) != NULL)
	{
		ezdp_mem_copy(shared_cmem.my_mac.ether_addr_octet, my_mac, sizeof(struct ether_addr));
	}
	else
	{
		printf("Error! fail to found my mac address in interface table!\n");
		exit(0);
	}

	while (true)
	{
		/* === Receive Frame === */
		logical_id = ezframe_receive(&cmem.frame, EZFRAME_HANDLE_FRAME_CANCEL);

		/* ===  received cancel frame === */
		if(unlikely(logical_id == -1))
		{
			exit(0);
		}

		/* === Check validity of received frame === */
		if(ezframe_valid(&cmem.frame) != 0)
		{
			printf("Frame is invalid - send to host!\n");
			nw_interface_update_statistic_counter(logical_id, ALVS_EZFRAME_VALIDATION_FAIL);
			nw_send_frame_to_host();
			continue;
		}

		nw_recieve_and_parse_frame(logical_id);
	} /* while (true) */
}

/******************************************************************************
 * \brief		 Initialize shared CMEM constructor
 *
 * \return	  true/false (success / failed )
 */
bool init_shared_cmem( void );
bool init_shared_cmem( void )
{
	uint32_t  result __unused;

	printf("init_shared_cmem cpu_id=%d\n", ezdp_get_cpu_id());

	/*Init interfaces DB*/
	result =  ezdp_init_table_struct_desc(ALVS_STRUCT_ID_INTERFACES,
										 &shared_cmem.interface_struct_desc,
										 cmem.table_work_area,
										 sizeof(cmem.table_work_area));
	if (result != 0)
	{
		printf("ezdp_init_hash_struct_desc of %d struct fail. Error Code %d. Error String %s \n",
				ALVS_STRUCT_ID_INTERFACES, result, ezdp_get_err_msg());
		return false;
	}

	result =  ezdp_validate_table_struct_desc( &shared_cmem.interface_struct_desc,
											  sizeof(struct dp_interface_result));
	if (result != 0)
	{
		printf("ezdp_validate_table_struct_desc of %d struct fail. Error Code %d. Error String %s \n",
				ALVS_STRUCT_ID_INTERFACES, result, ezdp_get_err_msg());
		return false;
	}

	/*Init services DB*/
	result =  ezdp_init_hash_struct_desc(ALVS_STRUCT_ID_SERVICES,
	                                     &shared_cmem.services_struct_desc,
	                                     cmem.service_hash_wa,
										 sizeof(cmem.service_hash_wa));
	if (result != 0)
	{
		printf("ezdp_init_hash_struct_desc of %d struct fail. Error Code %d. Error String %s \n",
				ALVS_STRUCT_ID_SERVICES, result, ezdp_get_err_msg());
		return false;
	}

	result =  ezdp_validate_hash_struct_desc( &shared_cmem.services_struct_desc,
	                                  	  	  true,
											  sizeof(struct alvs_service_key),
											  sizeof(struct alvs_service_result),
											  sizeof(_EZDP_LOOKUP_HASH_CALC_ENTRY_SIZE(sizeof(struct alvs_service_result),sizeof(struct alvs_service_key))));
	if (result != 0)
	{
		printf("ezdp_validate_hash_struct_desc of %d struct fail. Error Code %d. Error String %s \n",
				ALVS_STRUCT_ID_SERVICES, result, ezdp_get_err_msg());
		return false;
	}

	/*Init arp DB*/
	result =  ezdp_init_hash_struct_desc(ALVS_STRUCT_ID_ARP,
	                                     &shared_cmem.arp_struct_desc,
	                                     cmem.arp_hash_wa,
										 sizeof(cmem.arp_hash_wa));
	if (result != 0)
	{
		printf("ezdp_init_hash_struct_desc of %d struct fail. Error Code %d. Error String %s \n",
				ALVS_STRUCT_ID_ARP, result, ezdp_get_err_msg());
		return false;
	}

	result =  ezdp_validate_hash_struct_desc( &shared_cmem.arp_struct_desc,
	                                  	  	  true,
											  sizeof(struct alvs_arp_key),
											  sizeof(struct alvs_arp_result),
											  sizeof(_EZDP_LOOKUP_HASH_CALC_ENTRY_SIZE(sizeof(struct alvs_arp_result),sizeof(struct alvs_arp_key))));
	if (result != 0)
	{
		printf("ezdp_validate_hash_struct_desc of %d struct fail. Error Code %d. Error String %s \n",
				ALVS_STRUCT_ID_ARP, result, ezdp_get_err_msg());
		return false;
	}

	/*init stats addresses*/
	shared_cmem.nw_interface_stats_base_address.mem_type		= EZDP_EXTERNAL_MS;
	shared_cmem.nw_interface_stats_base_address.msid 			= ALVS_STATISTICS_MSID;
	shared_cmem.nw_interface_stats_base_address.element_index 	= 0;

	return true;
}


/******************************************************************************
 * \brief		 Call back function for Initializing memory
 *
 * \return	  true/false (success / failed )
 */
bool init_memory( enum ezdp_data_mem_space    data_ms_type, uintptr_t user_data __unused);
bool init_memory( enum ezdp_data_mem_space    data_ms_type, uintptr_t user_data __unused)
{
	switch (data_ms_type)
	{
	case EZDP_SHARED_CMEM_DATA:
		return init_shared_cmem();

	default:
		return true;
	}
}

/************************************************************************
 * \brief	Parses the run-time arguments of the application.
 *
 * \return	void
 */
static
void		parse_arguments(int argc,
							char **argv,
							uint32_t *num_cpus_set_p)
{
	int idx;
	(*num_cpus_set_p) = 0;

	for ( idx = 1 ; idx < argc ; )
	{
		if ( argv[ idx ][ 0 ] == '-' )
		{
			if ( strcmp( argv[ idx ], "-run_cpus" ) == 0 )
			{
				add_run_cpus( argv[ idx+1 ] );
				(*num_cpus_set_p) = 1;
				idx += 2;
			}
			else
			{
				printf("An unrecognized argument was found: %s\n", argv[ idx ]);
				idx++;
			}
		}
		else
		{
			printf("An unrecognized argument was found: %s\n", argv[ idx ]);
			idx++;
		}
	}
}

/******************************************************************************
* \brief		 Main function
*/
int			main( int argc, char **argv )
{
	struct ezdp_mem_section_info   mem_info;
	int32_t                        cpid = 0;
	uint32_t                       idx;
	int32_t                        status;
	uint32_t                       num_cpus_set;
	uint32_t                       result;

	/* listen to the SHUTDOWN signal to handle terminate signal from the simulator */
	signal(SIGINT,  signal_terminate_handler_gracefully_stop);
	signal(SIGTERM, signal_terminate_handler_gracefully_stop);
	signal(SIGILL,  signal_terminate_handler);
	signal(SIGSEGV, signal_terminate_handler);
	signal(SIGBUS,  signal_terminate_handler);
	signal(SIGCHLD, SIG_IGN);

	is_child_process = false;

	/* Parse the run-time arguments */
	parse_arguments( argc, argv, &num_cpus_set );

	result = ezdp_sync_cp();
	if(result != 0)
	{
		printf("ezdp_sync_cp failed %d %s. Exiting... \n", result, ezdp_get_err_msg());
		exit(1);
	}

	/* get memory section info */
	ezdp_get_mem_section_info(&mem_info, 0);

	/* print memory section info */
	printf("%s\n", ezdp_mem_section_info_str(&mem_info));

	/* validate that at least 256 bytes thread cache is enabled*/
	if(mem_info.cache_size < 256)
	{
		printf("Too many cmem variables. Thread cache size is zero. private_cmem_size is %d shared_cmem_size is %d. Exiting... \n",
				mem_info.private_cmem_size, mem_info.shared_cmem_size);
		exit(1);
	}

	result = ezdp_init_global(0);
	if(result != 0)
	{
		printf("ezdp_init_global failed %d %s. Exiting... \n", result, ezdp_get_err_msg());
		exit(1);
	}

	if ( !num_cpus_set )
	{
		/* run single process on processor 0*/
		num_cpus = 1;

		/* init the application */
		result = ezdp_init_local( 0 , 16, init_memory, 0, 0);
		if( result != 0 )
		{
			printf("ezdp_init_local failed %d %s. Exiting... \n", result, ezdp_get_err_msg());
			exit(1);
		}
		result = ezframe_init_local();
		if ( result != 0 )
		{
			printf("ezframe_init_local failed. %d; %s. Exiting... \n", result, ezdp_get_err_msg());
			exit(1);
		}

		/* call to packet processing function */
		ezdp_run(&packet_processing, num_cpus);

		return 0;
	}

	if ( num_cpus == 1 )
	{
		/* init the application */
		result = ezdp_init_local( 0 , run_cpus[0], init_memory, 0, 0);
		if( result != 0 )
		{
			printf("ezdp_init_local failed %d %s. Exiting... \n", result, ezdp_get_err_msg());
			exit(1);
		}
		result = ezframe_init_local();
		if ( result != 0 )
		{
			printf("ezframe_init_local failed. %d; %s. Exiting... \n", result, ezdp_get_err_msg());
			exit(1);
		}

		/* call to packet processing function */
		ezdp_run(&packet_processing, num_cpus);

		return 0;
	}

	for ( idx = 0; idx < num_cpus; idx++ )
	{
		cpid = fork();

		if ( cpid == -1) /* error */
		{
			perror("Error creating child process - fork fail\n");
			exit(1);
		}

		if ( cpid  == 0 ) 	/* code executed by child */
		{
			/* this will mark this process as a child process */
			is_child_process = true;

			/* init the dp/application */
			result = ezdp_init_local( 0 , run_cpus[idx], init_memory, 0, 0);
			if( result != 0 )
			{
				printf("ezdp_init_local failed %d %s. Exiting... \n", result, ezdp_get_err_msg());
				exit(1);
			}
			result = ezframe_init_local();
			if ( result != 0 )
			{
				printf("ezframe_init_local failed. %d; %s. Exiting... \n", result, ezdp_get_err_msg());
				exit(1);
			}
			/* call to packet processing function */
			ezdp_run(&packet_processing, num_cpus );

			return 0;
		}
		pids[idx] = cpid;
	}

	/* code executed by parent */
	for ( idx = 0; idx < num_cpus; idx++ )
	{
		wait(&status);
	}

	return 0;
}


