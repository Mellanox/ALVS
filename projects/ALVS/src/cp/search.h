/***************************************************************************
*
*                    Copyright by EZchip Technologies, 2016
*
*
*  Project:          ALVS
*  File:             search.h
*  Description:      Search definitions.
*  Notes:
*
****************************************************************************/


#ifndef _SEARCH_H_
#define _SEARCH_H_

#include "defs.h"

bool alvs_create_all_dbs(void);
bool alvs_create_classification_db(void);
bool alvs_create_arp_db(void);
bool alvs_load_partition(void);

bool alvs_add_classification_entry(struct alvs_service_key *key, struct alvs_service_result *result);
bool alvs_add_arp_entry(struct alvs_arp_key *key, struct alvs_arp_result *result);

bool alvs_delete_classification_entry(struct alvs_service_key *key);
bool alvs_delete_arp_entry(struct alvs_arp_key *key);

bool alvs_create_search_mem_partition(void);

#endif /* _SEARCH_H_ */
