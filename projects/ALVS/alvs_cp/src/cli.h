/***************************************************************************
*
*                    Copyright by EZchip Technologies, 2015
*
*
*  Project:          cli
*  File:             cli.h
*  Description:      Functions declarations.
*  Notes:
*
****************************************************************************/


#ifndef _cli_H_
#define _cli_H_


#include <EZapiStruct.h>
#include <cmds.h>


/*****************************************************************************/

#define     USER_INPUT_BUFFER_LEN         10
#define     FILE_INPUT_BUFFER_LEN         1024


/******************************************************************************
* Name:     cp_menu_CLI
* Desc:     Command line interface function which lets user choose what to do.
* Args:
* Notes:
* Return:
*/

void            cp_menu_CLI( void );



/******************************************************************************
* Name:     cp_agt_CLI
* Desc:     Command line interface function which lets the user to configure AGT.
* Args:
* Notes:
* Return:
*/

void            cp_agt_CLI( void );



/******************************************************************************
* Name:     cp_add_rule_CLI
* Desc:     Command line interface function which lets the user to add rule.
* Args:
* Notes:
* Return:
*/

void            cp_add_rule_CLI( void );



/******************************************************************************
* Name:     cp_delete_rule_CLI
* Desc:     Command line interface function which lets the user to delete rule.
* Args:
* Notes:
* Return:
*/

void            cp_delete_rule_CLI( void );



/******************************************************************************
* Name:     cp_update_rule_CLI
* Desc:     Command line interface function which lets the user to update rule.
* Args:
* Notes:
* Return:
*/

void            cp_update_rule_CLI( void );



/******************************************************************************
* Name:     cp_show_all_rules_CLI
* Desc:     Command line interface function which shows the user all the rules in the system.
* Args:
* Notes:
* Return:
*/

void            cp_show_all_rules_CLI( void );



/*******************************************************************************/

#endif /* _cli_H_ */
