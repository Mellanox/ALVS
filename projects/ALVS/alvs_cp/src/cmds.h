/***************************************************************************
*
*                    Copyright by EZchip Technologies, 2015
*
*
*  Project:          cmds
*  File:             cmds.h
*  Description:      Functions declarations.
*  Notes:
*
****************************************************************************/


#ifndef _cmds_H_
#define _cmds_H_


#include <EZapiStruct.h>
#include <EZagtRPC.h>
#include <EZagtCPMain.h>
#include <EZsocket.h>
#include <EZenv.h>
#include <EZdev.h>
#include <utils.h>


/*****************************************************************************/

#define     SEARCH_FORWARDING_RESULT_VLAN_WORD_OFFSET       12

/*****************************************************************************/

/* Rule operation */
typedef enum
{
   ADD_RULE,
   DELETE_RULE,
   UPDATE_RULE
}rule_operation;

/******************************************************************************
* Name:     modify_source_port_and_vlan
* Desc:     modify source port and vlan to the hash
* Args:     source_port - the source port
*           source_vlan - the source vlan
*           dest_port   - the destination port
*           dest_vlan   - the destination vlan
* Notes:
* Return:
*/

status     modify_source_port_and_vlan( rule_operation in_rule_operation,
                                        unsigned int source_port,
                                        unsigned int source_vlan,
                                        unsigned int dest_port,
                                        unsigned int dest_vlan );



/******************************************************************************
* Name:     get_source_port_and_vlan
* Desc:     get source port and vlan to the hash
* Args:     key     - the key from the entry
*           result - the result from the entry
* Notes:
* Return:
*           source_port - the source port
*           source_vlan - the source vlan
*           dest_port   - the destination port
*           dest_vlan   - the destination vlan
*/

void     get_source_port_and_vlan( EZapiEntry    *entry,
                                   unsigned int  *source_port,
                                   unsigned int  *source_vlan,
                                   unsigned int  *dest_port,
                                   unsigned int  *dest_vlan );



/******************************************************************************
* Name:     create_agt
* Desc:     Create EZagt library.
* Args:
* Notes:
* Return:
*/

status          create_agt( unsigned int agt_port );



/******************************************************************************
* Name:     delete_agt
* Desc:     Delete EZagt library.
* Args:
* Notes:
* Return:
*/

status          delete_agt( void );

#endif /* _cmds_H_ */
