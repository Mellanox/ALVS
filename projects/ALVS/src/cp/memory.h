/***************************************************************************
*
*                    Copyright by EZchip Technologies, 2016
*
*
*  Project:          ALVS
*  File:             memory.h
*  Description:      Memory definitions.
*  Notes:
*
****************************************************************************/


#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <stdint.h>
#include <stdbool.h>
#include <EZapiChannel.h>

#define     ALVS_NUM_OF_MSIDS                   12
#define     ALVS_HALF_CLUSTER_CODE_SIZE		0
#define     ALVS_HALF_CLUSTER_DATA_SIZE         0
#define     ALVS_1_CLUSTER_CODE_SIZE            0
#define     ALVS_1_CLUSTER_DATA_SIZE            0
#define     ALVS_2_CLUSTER_CODE_SIZE		0
#define     ALVS_2_CLUSTER_DATA_SIZE		0
#define     ALVS_4_CLUSTER_CODE_SIZE		0
#define     ALVS_4_CLUSTER_DATA_SIZE		0
#define     ALVS_16_CLUSTER_CODE_SIZE		0
#define     ALVS_16_CLUSTER_DATA_SIZE		0
#define     ALVS_ALL_CLUSTER_CODE_SIZE		0
#define     ALVS_ALL_CLUSTER_DATA_SIZE		0

static
uint32_t alvs_imem_sizes[ALVS_NUM_OF_MSIDS][2] = {
		{EZapiChannel_IntMemSpaceType_HALF_CLUSTER_CODE, ALVS_HALF_CLUSTER_CODE_SIZE},
		{EZapiChannel_IntMemSpaceType_HALF_CLUSTER_DATA, ALVS_HALF_CLUSTER_DATA_SIZE},
		{EZapiChannel_IntMemSpaceType_1_CLUSTER_CODE, ALVS_1_CLUSTER_CODE_SIZE},
		{EZapiChannel_IntMemSpaceType_1_CLUSTER_DATA, ALVS_1_CLUSTER_DATA_SIZE},
		{EZapiChannel_IntMemSpaceType_2_CLUSTER_CODE, ALVS_2_CLUSTER_CODE_SIZE},
		{EZapiChannel_IntMemSpaceType_2_CLUSTER_DATA, ALVS_2_CLUSTER_DATA_SIZE},
		{EZapiChannel_IntMemSpaceType_4_CLUSTER_CODE, ALVS_4_CLUSTER_CODE_SIZE},
		{EZapiChannel_IntMemSpaceType_4_CLUSTER_DATA, ALVS_4_CLUSTER_DATA_SIZE},
		{EZapiChannel_IntMemSpaceType_16_CLUSTER_CODE, ALVS_16_CLUSTER_CODE_SIZE},
		{EZapiChannel_IntMemSpaceType_16_CLUSTER_DATA, ALVS_16_CLUSTER_DATA_SIZE},
		{EZapiChannel_IntMemSpaceType_ALL_CLUSTER_CODE, ALVS_ALL_CLUSTER_CODE_SIZE},
		{EZapiChannel_IntMemSpaceType_ALL_CLUSTER_DATA, ALVS_ALL_CLUSTER_DATA_SIZE} };

bool alvs_create_mem_partition(void);

uint32_t alvs_get_imem_index(void);


#endif /* _MEMORY_H_ */
