//---------------------------------------------------------------
//
//  e c _ c o m m a n d . c
//
//  $LastChangedDate$
//  $Author$
//
//---------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "ec_command.h"

//---------------------------------------------------------------

void EtherCAT_command_init(EtherCAT_command_t *cmd)
{
  cmd->command_type = 0x00;
  cmd->node_address = 0x0000;
  cmd->ring_position = 0x0000;
  cmd->mem_address = 0x0000;
  cmd->logical_address = 0x00000000;
  cmd->data_length = 0;
  cmd->status = Waiting;
  cmd->next = NULL;
  cmd->working_counter = 0;
  cmd->data = NULL;
}

//---------------------------------------------------------------

void EtherCAT_command_clear(EtherCAT_command_t *cmd)
{
  if (cmd->data)
  {
    free(cmd->data);
  }

  EtherCAT_command_init(cmd);
}

//---------------------------------------------------------------

void EtherCAT_command_print_data(EtherCAT_command_t *cmd)
{
  unsigned int i;

  printf("[");

  for (i = 0; i < cmd->data_length; i++)
  {
    printf("%02X", cmd->data[i]);

    if (i < cmd->data_length - 1) printf(" ");
  }

  printf("]\n");
}

//---------------------------------------------------------------
