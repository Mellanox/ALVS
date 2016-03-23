/***************************************************************************
*
*						  Copyright by EZchip Technologies, 2012
*
*
*  Project:	 	NPS400 demo dp_dp application
*  File:		main.c
*  Desc:		Implementation of the demo dp_dp application
*
****************************************************************************/


/* system includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <net/ethernet.h>

/* dp includes */
#include <ezdp.h>
#include <ezframe.h>

/* application include */
#include "defs.h"
#include "log.h"

extern void			signal_terminate_handler( int signum );
extern void       signal_terminate_handler_gracefully_stop( int signum );
extern void			add_run_cpus( char* processors_str );
extern bool			is_child_process;
extern uint32_t 	num_cpus;
extern int32_t 		run_cpus[];
extern uint32_t 	pids[];



/***************** global CMEM data *************************/
ezframe_t			frame 								__cmem_var;
		/**< Frame	*/

uint8_t				frame_data[EZFRAME_BUF_DATA_SIZE] 	__cmem_var;
		/**< Frame data pointer */

static struct ezdp_decode_mac_result	 decode_result 	__cmem_var;
		/**< Result of Decode MAC */

static struct search_forwarding_key	  	lookup_key	  	__cmem_var;
		/**< Physical Port Lookup Key  */

static char  work_area[EZDP_HASH_WORK_AREA_SIZE( sizeof(struct search_forwarding_entry), sizeof(lookup_key) )]  __cmem_var;
		/**< Work area for lookup function */

ezdp_hash_struct_desc_t 			forwarding_hash_struct_desc	__cmem_shared_var;
		/**< Search struct descriptor*/

bool cancel_frame_signal __cmem_var = false;
      /**< Cancel frame signal */


/*****************************************************************************/
static const int8_t __scm_rev__[] = EZDP_VERSION_XSTR(_SCM_REV_);

struct ezdp_version version_info =
{
	EZDP_VERSION_DEF_PROJECT_NAME,		/* const int8_t*		project_name */
	"demo dp",							         /* const int8_t*		module_name */
	EZDP_VERSION_DEF_MAJOR_VER,			/* uint32_t				major_version*/
	EZDP_VERSION_DEF_MINOR_VER,			/* uint32_t				minor_version */
	EZDP_VERSION_DEF_VERSION_CHAR,		/* uint8_t				version_char */
	EZDP_VERSION_DEF_VERSION_STRING,	/* const int8_t*		Version_string */
	EZDP_VERSION_DEF_PATCH_MAJOR_VER,	/* uint8_t				major_patch_version */
	EZDP_VERSION_DEF_PATCH_MINOR_VER,	/* uint8_t				minor_patch_version */
	EZDP_VERSION_DEF_PATCH_MICRO_VER,	/* uint8_t				micro_patch_version */
	__scm_rev__,						/* const int8_t*		build_number */
	__DATE__,							/* const int8_t*		creation_date */
	__TIME__,							/* const int8_t*		creation_time */
};

/*****************************************************************************/


bool cancel_frame_required(void) __fast_path_code;
bool cancel_frame_required()
{
   return (cancel_frame_signal);
}
ezframe_cancel_required_register(cancel_frame_required);


static void		  print_versions( void )
{
	char					ver_info[EZDP_VERSION_MAX_STRING_LENGTH];
	struct ezdp_version		*ver_info_ptr;

	/* demo dp_dp version */
	ver_info_ptr = &version_info;
	ezdp_version_get_string(ver_info_ptr, ver_info);
	printf("%s\n", ver_info);

	/* get ezdp version info */
	ver_info_ptr = ezdp_get_version();
	ezdp_version_get_string(ver_info_ptr, ver_info);
	printf("%s\n", ver_info);

	/* get ezframe version info */
	ver_info_ptr = ezframe_get_version();
	ezdp_version_get_string(ver_info_ptr, ver_info);
	printf("%s\n", ver_info);

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
				print_warning("An unrecognized argument was found: %s\n", argv[ idx ]);
				idx++;
			}
		}
		else
		{
			print_warning("An unrecognized argument was found: %s\n", argv[ idx ]);
			idx++;
		}
	}
}

/******************************************************************************
 * \brief		 Initialize shared CMEM constructor
 *
 * \return	  true/false (success / failed )
 */
bool init_shared_cmem( void );
bool init_shared_cmem( void )
{
	uint32_t  result;

	print_debug("init_shared_cmem cpu_id=%d\n", ezdp_get_cpu_id());

	result =  ezdp_init_hash_struct_desc(SEARCH_STRUCT_ID_FORWARDING,
	                                     &forwarding_hash_struct_desc,
	                                     work_area,
										 sizeof(work_area));
	if (result != 0)
	{
		print_error("ezdp_init_hash_struct_desc of %d struct fail. Error Code %d. Error String %s \n",
				SEARCH_STRUCT_ID_FORWARDING, result, ezdp_get_err_msg());
		return false;
	}

	result =  ezdp_validate_hash_struct_desc( &forwarding_hash_struct_desc,
	                                  	  	  true,
											  sizeof(lookup_key),
											  sizeof(struct search_forwarding_result),
											  sizeof(struct search_forwarding_entry));
	if (result != 0)
	{
		print_error("ezdp_validate_hash_struct_desc of %d struct fail. Error Code %d. Error String %s \n",
				SEARCH_STRUCT_ID_FORWARDING, result, ezdp_get_err_msg());
		return false;
	}
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


/**********************************
 *  \brief   discard received frame
 *
 * \return	  void
 */
void discard_frame(void) __slow_path_code;
void discard_frame(void)
{
	print_debug( "Free frame \n");
	/* free all BDs and Job */
	ezframe_free(&frame,0);
}



 /******************************************************************************
 * \brief		Main packet processing function
 *
 *				 Frame In:  C-VLAN or Untagged (IPv4 or IPv6 only) packet
 *				 Frame Out: If untagged, adds C-VLAN to the packet, else updates the
 *								C-VLAN data according to the configuration
 *
 * \note	Get frame
 * 			Load first frame buffer data
 * 			decode mac into decode_result & check for decode errors.
 * 			if error -> free frame.
 * 			build a key for the lookup:
 * 				lookup_key.source_port  = logical_id
 * 				extract vlan from eth frame header into lookup_key.vlan
 * 				if eth type not supported - free frame
 * 			perform lookup - ezdp_lookup_hash_entry
 * 			if bad RC - free frame
 *
 * 			if CVLAN header already exists, update data according to lookup result
 * 			else - make room in header for cvlan after MAC Addresses
 *
 * 			store frame data
 * 			transmit frame to TM. destination is in lookup result.
 *
 * \return	  void
 */
void		  packet_processing(void)  __fast_path_code;
void		  packet_processing(void)
{
	uint8_t                                     *buffer_ptr;
	uint8_t                                     *frame_base;
	struct ether_header							*eth_hdr_ptr;
	struct vlan_tag								*vlan_ptr;
	uint32_t                                     rc;
	uint32_t                                     found_result_size;
	struct search_forwarding_entry              *lookup_entry_ptr;
	int32_t logical_id;
	uint32_t length;

	while (true)
	{

		print_debug( "****** Attempting to receive new frame... ******\n");
		/* ========================== Receive Frame ========================= */
		logical_id = ezframe_receive(&frame, 0);

		 //Received cancel frame
		if(unlikely(logical_id == -1))
		{
			exit(0);
		}

		if(ezframe_valid(&frame) != 0)
		{
			print_warning("Frame is invalid - Free frame \n");
			discard_frame();
			continue;
		}
		/* ===================== Load Data of first frame buffer ================== */
		frame_base = ezframe_load_buf(&frame, frame_data, &length, 0);
		if (frame_base == NULL)
		{
			print_warning("Problem loading first buffer - Free frame \n");
			discard_frame();
			continue;
		}


		/* decode mac to ensure it is valid */
		ezdp_decode_mac(frame_base,
						MAX_DECODE_SIZED,
						&decode_result);

		if (decode_result.error_codes.decode_error)
		{
			print_warning("Decode MAC failed - Free frame \n");
			discard_frame();
			continue;
		}

		/* ========================== Perform Lookup ========================= */

		/* build a key for the lookup */
		lookup_key.source_port  = logical_id;


		/* Get VLAN from frame ETH header */
		buffer_ptr	= frame_base;
		eth_hdr_ptr = (struct ether_header*)buffer_ptr;
		if (eth_hdr_ptr->ether_type == ETHERTYPE_VLAN)
		{
			/* skip the Ethernet header */
			buffer_ptr += sizeof(struct ether_header);
			vlan_ptr	 = (struct vlan_tag*)buffer_ptr;
			lookup_key.vlan = vlan_ptr->vlan_id;
		}
		else if (	(eth_hdr_ptr->ether_type == ETHERTYPE_IP) ||
					(eth_hdr_ptr->ether_type == ETHERTYPE_IPV6))
		{
			/* set default VLAN */
			lookup_key.vlan = DEFAULT_VLAN_ID;
		}
		else
		{
			/* drop the frame */
			print_warning("Ethernet Type (%d) not supported - Free frame \n", eth_hdr_ptr->ether_type);
			discard_frame();
			continue;
		}

		print_debug("Lookup Key: \n");
		print_debug("	 source_port = %d \n", lookup_key.source_port);
		print_debug("	 vlan        = %d \n", lookup_key.vlan);

		rc = ezdp_lookup_hash_entry(&forwarding_hash_struct_desc,
												(void*)&lookup_key,
												sizeof(lookup_key),
												(void **)&lookup_entry_ptr,
												&found_result_size,
												0,
												work_area,
												sizeof(work_area));

		/* check for error. */
		if (rc != 0)
		{
			/* Error - drop the frame. */
			print_warning("Lookup failed: Error Code %d. free frame \n", rc);
			discard_frame();
			continue;
		}

		/* print match */
		print_debug("Lookup success. Result: \n");
		print_debug("   Destination          = %d \n",  lookup_entry_ptr->result.next_destination);
		print_debug("	vlan            	 = %d \n", lookup_entry_ptr->result.vlan);

		/* ========================== Update Frame ========================= */
		/* if CVLAN already exists, update data according to lookup result */
		if (eth_hdr_ptr->ether_type == ETHERTYPE_VLAN)
		{
			/* write CVLAN Data according to search result */
			vlan_ptr	 = (struct vlan_tag*)buffer_ptr;
			vlan_ptr->priority 	= DEFAULT_PRIORITY;
			vlan_ptr->cfi		= 0;
			vlan_ptr->vlan_id  	= lookup_entry_ptr->result.vlan;

			/* Store modified frame data */
			rc = ezframe_store_buf(	&frame,
									frame_base,		// pointer to the frame data without the offset
									length,
									0);
		}
		else
		{
			/* make room for CVLAN after MAC Addresses */
			buffer_ptr  = frame_base;
			buffer_ptr -= (sizeof(uint16_t) + sizeof(struct  vlan_tag));	//eth type is 2 bytes.

			/* copy the MAC addresses to new position */
			ezdp_mem_copy(buffer_ptr, frame_base,
							  sizeof(struct ether_addr) *2);

			/* update frame base pointer */
			frame_base = buffer_ptr;

			eth_hdr_ptr = (struct ether_header*)buffer_ptr;
			/* write CVLAN Ethernet Type */
			eth_hdr_ptr->ether_type = ETHERTYPE_VLAN;
			/* skip the Ethernet header */
			buffer_ptr += sizeof(struct ether_header);

			vlan_ptr 	= (struct vlan_tag*)buffer_ptr;

			/* write CVLAN Data according to search result */
			vlan_ptr->priority 	= DEFAULT_PRIORITY;
			vlan_ptr->cfi		= 0;
			vlan_ptr->vlan_id  	= lookup_entry_ptr->result.vlan;

			/* Store modified frame data */
			rc = ezframe_store_buf(	&frame,
									frame_base,		// pointer to the frame data without the offset
									length + (sizeof(uint16_t) + sizeof(struct vlan_tag)),	// Add header bytes to the beginning of frame
									0);
		}

		if(rc)
		{
			print_warning("Problem storing frame data - Free frame \n");
			discard_frame();
			continue;
		}

		print_debug("Transmitting Frame... \n");
		print_debug("    Destination  = %d \n", lookup_entry_ptr->result.next_destination);

		/* ========================== Transmit Frame ========================= */
		/*	Send frame to TM.
		 * 		Take destination from lookup result.	*/
		/* 		Use MAC decode hash to select link for LAG */
		ezframe_send_to_tm(&frame,lookup_entry_ptr->result.next_destination,decode_result.da_sa_hash,0,NULL,0);

	} /* while (true) */
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


	/* print application and libraries version info */
	print_versions();

	/* listen to the SHUTDOWN signal to handle terminate signal from the simulator */
	signal(SIGINT,  signal_terminate_handler);
	signal(SIGTERM, signal_terminate_handler);
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
		print_error("ezdp_sync_cp failed %d %s. Exiting... \n", result, ezdp_get_err_msg());
		exit(1);
	}

	/* get memory section info */
	ezdp_get_mem_section_info(&mem_info, 0);

	/* print memory section info */
	printf("%s\n", ezdp_mem_section_info_str(&mem_info));

	/* validate that at least 256 bytes thread cache is enabled*/
	if(mem_info.cache_size < 256)
	{
		print_error("Too many cmem variables. Thread cache size is zero. private_cmem_size is %d shared_cmem_size is %d. Exiting... \n",
				mem_info.private_cmem_size, mem_info.shared_cmem_size);
		exit(1);
	}

	result = ezdp_init_global(0);
	if(result != 0)
	{
		print_error("ezdp_init_global failed %d %s. Exiting... \n", result, ezdp_get_err_msg());
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
			print_error("ezdp_init_local failed %d %s. Exiting... \n", result, ezdp_get_err_msg());
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
			print_error("ezdp_init_local failed %d %s. Exiting... \n", result, ezdp_get_err_msg());
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
				print_error("ezdp_init_local failed %d %s. Exiting... \n", result, ezdp_get_err_msg());
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


