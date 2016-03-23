/******************************************************************************
*
*						  Copyright by EZchip Technologies, 2012
*
*
*  Project:	 	 NPS400
*  File:		 log.c
*  Desc:		 Logging implementation
*
*******************************************************************************/

/* system include */
#include <string.h>

/* dp include */
#include <ezdp_processor.h>

/* application include */
#include "log.h"


#define  MAX_LOGGER_STRING	1320


/******************************************************************************/

void	  print(const char *log_level, const char *format, ... )
{
	char		  buffer[MAX_LOGGER_STRING];
	va_list	valist;

	sprintf(buffer, "CPU#%d %s", ezdp_get_cpu_id(), log_level);
	uint32_t buffer_len = strlen(buffer);

	va_start(valist, format);
	vsnprintf(&buffer[buffer_len], (MAX_LOGGER_STRING - buffer_len), format, valist);
	va_end(valist);

	fprintf(stdout, buffer);
	fflush(stdout);
}



/******************************************************************************/
