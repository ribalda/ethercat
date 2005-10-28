/****************************************************************
 *
 *  e c _ c o m m a n d . c
 *
 *  Methoden für ein EtherCAT-Kommando.
 *
 *  $Date$
 *  $Author$
 *
 ***************************************************************/

#include <linux/slab.h>

#include "ec_command.h"

/***************************************************************/

/**
   Kommando-Konstruktor.

   Initialisiert alle Variablen innerhalb des Kommandos auf die
   Default-Werte.
   
   @param cmd Zeiger auf das zu initialisierende Kommando.
*/

void EtherCAT_command_init(EtherCAT_command_t *cmd)
{
  cmd->type = ECAT_CMD_NONE;
  cmd->address.logical = 0x00000000;
  cmd->data_length = 0;
  cmd->next = NULL;
  cmd->state = ECAT_CS_READY;
  cmd->index = 0;
  cmd->working_counter = 0;
}

/***************************************************************/

/**
   Kommando-Destruktor.

   Setzt alle Attribute auf den Anfangswert zurueck.
   
   @param cmd Zeiger auf das zu initialisierende Kommando.
*/

void EtherCAT_command_clear(EtherCAT_command_t *cmd)
{
  EtherCAT_command_init(cmd);
}

/***************************************************************/
