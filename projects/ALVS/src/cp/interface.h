/***************************************************************************
*
*                    Copyright by EZchip Technologies, 2016
*
*
*  Project:          ALVS
*  File:             interface.h
*  Description:      Interface definitions.
*  Notes:
*
****************************************************************************/


#ifndef _INTERFACE_H_
#define _INTERFACE_H_

#include <stdbool.h>

#define		ALVS_EXT_IF_NUM                 4
#define		ALVS_EXT_IF_TYPE                EZapiChannel_EthIFType_100GE
#define		ALVS_EXT_IF_LAG_ENABLED         false

bool alvs_create_if_mapping(void);

#endif /* _INTERFACE_H_ */
