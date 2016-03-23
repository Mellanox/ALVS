/***************************************************************************
*
*                    Copyright by EZchip Technologies, 2015
*
*
*  Project:          utils
*  File:             utils.h
*  Description:      Functions declarations.
*  Notes:
*
****************************************************************************/


#ifndef _utils_H_
#define _utils_H_

#include <EZtype.h>
#include <defs.h>

/*****************************************************************************/

#define     NUMBER_OF_PORTS               2

/*****************************************************************************/

/* User return codes. */
typedef enum
{
   SUCCESS,
   FAILED
}status;


/*****************************************************************************/


typedef struct _port_info
{
   unsigned int    port_number;
   unsigned int    if_side;
   unsigned int    if_engine;
   unsigned int    if_type;
   unsigned int    if_number;
   unsigned int    tx_base_oq_channel;
} port_info;

port_info    interfaces_mapping[ NUMBER_OF_PORTS ];


/******************************************************************************
* Name:     get_flow_id
* Desc:     calculates the flow_id
* Args:     oq_channel - the OQ channel
* Notes:
* Return:
*/

unsigned int    get_flow_id( unsigned int oq_channel );


/******************************************************************************
* Name:     cp_to_dp16
* Desc:     swap cp endian to dp endian for 16 bits
* Args:
* Notes:
* Return:
*/

uint16_t        cp_to_dp16( uint16_t x );


/******************************************************************************
* Name:     dp_to_cp16
* Desc:     swap dp endian to cp endian for 16 bits
* Args:
* Notes:
* Return:
*/

uint16_t        dp_to_cp16( uint16_t x );


/******************************************************************************
* Name:     cp_to_dp32
* Desc:     swap cp endian to dp endian for 32 bits
* Args:
* Notes:
* Return:
*/

uint32_t        cp_to_dp32( uint32_t x );


/******************************************************************************
* Name:     dp_to_cp32
* Desc:     swap dp endian to cp endian for 32 bits
* Args:
* Notes:
* Return:
*/

uint32_t        dp_to_cp32( uint32_t x );


/******************************************************************************
* Name:     set_dp_bitfield
* Desc:     write the bitwise fields in raw_data
* Args:     raw_data - the result
*           size     - size of the bits to write
*           word     - the word count inside the raw_data
*           offset   - the offset from the beginning of the word.
*           value    - the value to write
* Notes:
* Return:
*/

void            set_dp_bitfield( uint32_t *raw_data, int size, int word, int offset, uint32_t value );


/******************************************************************************
* Name:     get_dp_bitfield
* Desc:     read the bitwise fields in raw_data
* Args:     raw_data - the result
*           size     - size of the bits to read
*           word     - the word count inside the raw_data
*           offset   - the offset from the beginning of the word.
* Notes:
* Return:
*/

uint32_t        get_dp_bitfield( uint32_t *raw_data, int size, int word, int offset );


#endif /* _utils_H_ */
