/***************************************************************************
*
*						  Copyright by EZchip Technologies, 2012
*
*
*  Project:	 	NPS400 template application
*  File:		main.c
*  Desc:		Hello Packet in NPS
*
****************************************************************************/


/* system includes */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

/* dp includes */
#include <ezdp.h>
#include <ezframe.h>


/*****************  definitions  *************************/
#define			__fast_path_code						__imem_1_cluster_func


/***************** global CMEM data *************************/

ezframe_t			frame 								__cmem_var;
		/**< Frame	*/

uint8_t				frame_data[EZFRAME_BUF_DATA_SIZE] 	__cmem_var;
		/**< Frame data buffer */



void  set_gracefull_stop(void);
void  set_gracefull_stop()
{
	ezframe_set_cancel_signal();
}


/************************************************************************
 * \brief      signal terminate handler
 *
 * \note       assumes num_cpus==0 if and only if child process
 *
 * \return     void
 */
void       signal_terminate_handler_gracefully_stop( int signum __unused );
void       signal_terminate_handler_gracefully_stop( int signum __unused )
{
      set_gracefull_stop();
}

/************************************************************************
 * \brief      signal terminate handler
 *
 * \note       assumes num_cpus==0 if and only if child process
 *
 * \return     void
 */
void       signal_terminate_handler( int signum __unused );
void       signal_terminate_handler( int signum __unused )
{
   abort();
}

 /******************************************************************************
 * \brief		Main packet processing function
 *
 *
 * \note		 Receive frame
 *				 Load (DMA) the first frame buffer to CMEM
 *				 Print "Hello Packet" message
 *				 Transmit the received frame to the port 0
 *
 * \return	  void
 */
void		  packet_processing(void) __fast_path_code;
void		  packet_processing(void)
{
	uint8_t *frame_base;
	int32_t logical_id;

	while (true)
	{
		/* === Receive Frame === */
		logical_id = ezframe_receive(&frame, EZFRAME_HANDLE_FRAME_CANCEL);

		/* ===  received cancel frame === */
		if(unlikely(logical_id == -1))
		{
			exit(0);
		}

		/* === Check validity of received frame === */
		if(ezframe_valid(&frame) != 0)
		{
			printf("Frame is invalid - Free frame \n");
			ezframe_free(&frame, 0);
			continue;
		}

		/* === Load Data of first frame buffer === */
		frame_base = ezframe_load_buf(&frame, frame_data, NULL, 0);

		/* === Print hello packet message === */
		printf("!!!Hello Packet!!!\n");
		printf("		Packet length: %d\n", ezframe_get_len(&frame));
		printf("		Packet data: %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x  ...\n",
				frame_base[0],  frame_base[1],  frame_base[2],  frame_base[3],
				frame_base[4],  frame_base[5],  frame_base[6],  frame_base[7],
				frame_base[8],  frame_base[9],  frame_base[10], frame_base[11],
				frame_base[12], frame_base[13], frame_base[14], frame_base[15]);


		/* === Transmit Frame === */
		/* transmit frame to the same input interface */
		ezframe_send_to_tm(&frame,0,0,0,NULL,0);
	} /* while (true) */
}



/******************************************************************************
* \brief		 Main function
*/
int			main(void)
{
	uint32_t  result;

	/* listen to the SHUTDOWN signal to handle terminate signal from the simulator */
	signal(SIGINT,  signal_terminate_handler_gracefully_stop);
	signal(SIGTERM, signal_terminate_handler_gracefully_stop);
	signal(SIGILL,  signal_terminate_handler);
	signal(SIGSEGV, signal_terminate_handler);
	signal(SIGBUS,  signal_terminate_handler);

    result = ezdp_sync_cp();
	if(result != 0)
	{
		printf("ezdp_sync_cp failed. result %d %s\n", result, ezdp_get_err_msg());
		return EXIT_FAILURE;
	}

	result = ezdp_init_global(0);
	if(result != 0)
	{
		printf("ezdp_init_global failed. result %d %s\n", result, ezdp_get_err_msg());
		return EXIT_FAILURE;
	}

	/* init the dp/application, bind to processor 0 */
	result = ezdp_init_local( 0 , 16, NULL, 0, 0);
	if(result != 0)
	{
		printf("ezdp_init_local failed. result %d %s\n", result, ezdp_get_err_msg());
		return EXIT_FAILURE;
	}

	result = ezframe_init_local();
	if (result != 0)
	{
		printf("ezframe_init_local failed. result %d %s\n", result, ezdp_get_err_msg());
		return EXIT_FAILURE;
	}

	/* call to packet processing function */
	ezdp_run(&packet_processing, 0 );

	return EXIT_SUCCESS;
}


