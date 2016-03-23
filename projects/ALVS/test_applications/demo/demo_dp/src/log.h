/******************************************************************************
*
*						  Copyright by EZchip Technologies, 2012
*
*
*  Project:	 	NPS400
*  File:		log.h
*  Desc:		Header file for Logging interface
*
*******************************************************************************/

#ifndef _LOG_H_
#define _LOG_H_

/* system include */
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>


#ifndef  va_copy
#define  va_copy(_tgt_,_src_) ((_tgt_)=(_src_))
#endif

#ifdef DEBUG_LOG
#define  LOG_DEBUG( _code_ )  _code_
#else
#define  LOG_DEBUG( _code_ )
#endif

/************************************************************************
* \brief	Write formatted output using a pointer to a list of
*			arguments. Will write to the log file represented to
*			by file_stream.
* \param[in]	log_level  -
* \param[in]	format		- format specification
* \return		void
*/
void		print(const char *log_level, const char *format, ... );


/************************************************************************
* \brief	Write formatted output using a pointer to a list of
*			arguments. Will write to the log file represented to
*			by file_stream, with log level ERROR. If file_stream is
*			null, will write to stdout.
* \note		  Assumes initialize method was called with file_stream.
* \param[in]	format		- format specification
* \return		void
*/
#define	print_error(format, ...)	  print("ERROR: ",  format, ##__VA_ARGS__)


/************************************************************************
* \brief	Write formatted output using a pointer to a list of
*			arguments. Will write to the log file represented to
*			by file_stream, with log level DEBUG. If file_stream is
*			null, will write to stdout.
* \note		  Assumes initialize method was called with file_stream.
* \param[in]	format		- format specification
* \return		void
*/
#define	print_debug(format, ...)  print("DEBUG: ", format, ##__VA_ARGS__)


/************************************************************************
* \brief	Write formatted output using a pointer to a list of
*			arguments. Will write to the log file represented to
*			by file_stream, with log level WARN. If file_stream is
*			null, will write to stdout.
* \param[in]	format		- format specification
* \return		void
*/
#define	print_warning(format, ...)	  print("WARN: ",  format, ##__VA_ARGS__)



/*****************************************************************************/

#endif  /* _LOG_H_ */
