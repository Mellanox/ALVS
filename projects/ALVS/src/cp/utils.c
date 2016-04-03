/***************************************************************************
*
*                    Copyright by EZchip Technologies, 2015
*
*
*  Project:          utils
*  File:             utils.c
*  Description:      
*  Notes:
*
*
****************************************************************************/

#include "utils.h"



/*******************************************************************************/

unsigned int get_flow_id( unsigned int oq_channel )
{
   /* Channels are spread between the TM chunks.
      4 chunks per TM each chunk serves 32 OQ channels.

      First part of the formula, calculates the chunk id entity id,(  each chunk has 128K entities )
      Second part of the formula is the offset inside the chunk- each channel has 512 flow */

   return ( ( ( oq_channel / 32 ) * 128 * 1024 ) + ( ( oq_channel % 32 ) * 512 ) );
}



/*************************************************************************************/

inline  uint16_t cp_to_dp16( uint16_t x )
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
   return ((x<<8) | (x>>8));
#else
   return x;
#endif
}



/*************************************************************************************/

inline  uint16_t dp_to_cp16( uint16_t x )
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
   return ((x>>8) | (x<<8));
#else
   return x;
#endif
}



/*************************************************************************************/

inline uint32_t cp_to_dp32( uint32_t x )
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
   return (x<<24 | x>>24 | (x & (uint32_t)0x0000ff00UL)<<8 | (x & (uint32_t)0x00ff0000UL)>>8);
#else
   return x;
#endif

}



/*************************************************************************************/

inline uint32_t dp_to_cp32( uint32_t x )
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
   return (x>>24 | x<<24 | (x & (uint32_t)0x0000ff00UL)<<8 | (x & (uint32_t)0x00ff0000UL)>>8);
#else
   return x;
#endif

}



/*************************************************************************************/

inline void set_dp_bitfield( uint32_t *raw_data, int size, int word, int offset, uint32_t value )
{
   /* Create the mask to write in the bit fields */
   uint32_t mask = cp_to_dp32(((1 << size) - 1) << offset);

   /* Swap the value from cp to dp */
   value = cp_to_dp32(value << offset);

   /* Write the data in the correct endian to the raw_data */
   raw_data[word] &= ~mask;
   raw_data[word] |= value;
}



/*************************************************************************************/

inline uint32_t get_dp_bitfield( uint32_t *raw_data, int size, int word, int offset )
{
   /* Create the mask to read in the bit fields */
   uint32_t mask = cp_to_dp32(((1 << size) - 1) << offset);

   /* read the data in the correct endian from the raw_data */
   return (dp_to_cp32( ((raw_data[word] & mask)) )>>offset);
}



/*******************************************************************************/

